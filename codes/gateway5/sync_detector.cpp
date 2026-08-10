#include "gateway5/sync_detector.h"
#include "gateway5/frame_synchronizer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace lunanet::gateway5 {

std::vector<double> ComputeSyncCorrelation(const std::vector<double>& received) {
    const std::vector<float> reference = BuildSyncReferenceSymbols();
    const std::size_t pattern_len = reference.size();

    if (received.size() < pattern_len) {
        return {};
    }

    const std::size_t num_offsets = received.size() - pattern_len + 1;
    std::vector<double> correlation(num_offsets, 0.0);

    // Brute-force sliding correlation. O(N * 68). The spec explicitly
    // recommends building this simple version first and only reaching for
    // something faster (e.g. FFT-based correlation) if performance testing
    // later shows it's needed.
    for (std::size_t offset = 0; offset < num_offsets; ++offset) {
        double sum = 0.0;
        for (std::size_t k = 0; k < pattern_len; ++k) {
            sum += received[offset + k] * static_cast<double>(reference[k]);
        }
        correlation[offset] = sum;
    }

    return correlation;
}

FrameSyncResult DetectFrameSync(const std::vector<double>& received,
                                 double psr_threshold,
                                 double peak_to_rms_threshold,
                                 double normalized_peak_threshold) {
    FrameSyncResult result;

    if (!std::isfinite(psr_threshold) || psr_threshold <= 0.0 ||
        !std::isfinite(peak_to_rms_threshold) || peak_to_rms_threshold <= 0.0 ||
        !std::isfinite(normalized_peak_threshold) ||
        normalized_peak_threshold <= 0.0 || normalized_peak_threshold > 1.0 ||
        !std::all_of(received.begin(), received.end(), [](const double value) {
            return std::isfinite(value);
        })) {
        return result;
    }

    const std::vector<double> correlation = ComputeSyncCorrelation(received);
    if (correlation.empty()) {
        return result;  // detected = false
    }

    const std::size_t pattern_len = BuildSyncReferenceSymbols().size();
    std::vector<double> normalized(correlation.size(), 0.0);
    double window_energy = 0.0;
    for (std::size_t i = 0; i < pattern_len; ++i) {
        window_energy += received[i] * received[i];
    }
    constexpr double kMinEnergy = 1e-12;
    for (std::size_t offset = 0; offset < correlation.size(); ++offset) {
        const double denominator = std::sqrt(
            static_cast<double>(pattern_len) * std::max(window_energy, kMinEnergy));
        normalized[offset] = correlation[offset] / denominator;

        if (offset + pattern_len < received.size()) {
            window_energy -= received[offset] * received[offset];
            window_energy += received[offset + pattern_len] *
                received[offset + pattern_len];
        }
    }

    // Locate the global normalized peak. Signed max (not max-magnitude): given the
    // reference's fixed sign convention, a genuine match produces a
    // strongly POSITIVE spike (aligned terms are all (+-1)*(+-1) = +1), so
    // we're specifically looking for that positive spike rather than any
    // large-magnitude excursion.
    const auto peak_it = std::max_element(normalized.begin(), normalized.end());
    const std::size_t peak_offset =
        static_cast<std::size_t>(peak_it - normalized.begin());
    const double normalized_peak = *peak_it;
    result.peak_correlation = correlation[peak_offset];
    result.normalized_peak = normalized_peak;

    // A negative excursion cannot imitate this fixed-polarity sync pattern,
    // but it still belongs in the RMS estimate of the correlation floor.
    double next_highest = 0.0;
    double sidelobe_sum_squares = 0.0;
    std::size_t sidelobe_count = 0;
    bool have_sidelobe = false;
    for (std::size_t offset = 0; offset < correlation.size(); ++offset) {
        const long long distance = static_cast<long long>(offset) -
                                    static_cast<long long>(peak_offset);
        if (std::llabs(distance) <= kSyncSidelobeExclusion) {
            continue;
        }
        const double value = normalized[offset];
        next_highest = std::max(next_highest, value);
        sidelobe_sum_squares += value * value;
        ++sidelobe_count;
        have_sidelobe = true;
    }

    if (!have_sidelobe) {
        // Stream too short to have any offsets outside the exclusion
        // window around the peak -- can't compute a meaningful PSR.
        return result;
    }

    // Guard against a near-zero sidelobe floor, which would make PSR
    // blow up numerically rather than reflecting genuine confidence.
    constexpr double kMinSidelobeFloor = 1e-6;
    const double sidelobe_floor = std::max(next_highest, kMinSidelobeFloor);
    result.psr = normalized_peak / sidelobe_floor;
    const double sidelobe_rms = std::sqrt(
        sidelobe_sum_squares / static_cast<double>(sidelobe_count));
    result.peak_to_rms = normalized_peak / std::max(sidelobe_rms, kMinSidelobeFloor);

    if (normalized_peak > 0.0 &&
        result.psr >= psr_threshold &&
        result.peak_to_rms >= peak_to_rms_threshold &&
        result.normalized_peak >= normalized_peak_threshold) {
        result.detected = true;
        result.frame_offset = peak_offset;
    }

    return result;
}

}  // namespace lunanet::gateway5
