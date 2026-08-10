#include "gateway5/sync_detector.h"
#include "gateway5/frame_synchronizer.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace {

struct WilsonInterval {
    double lower = 0.0;
    double upper = 1.0;
};

WilsonInterval OneSidedWilsonInterval(int successes, int trials) {
    constexpr double kOneSided95PercentZ = 1.6448536269514722;
    const double n = static_cast<double>(trials);
    const double proportion = static_cast<double>(successes) / n;
    const double z2 = kOneSided95PercentZ * kOneSided95PercentZ;
    const double denominator = 1.0 + z2 / n;
    const double center = (proportion + z2 / (2.0 * n)) / denominator;
    const double half_width = kOneSided95PercentZ * std::sqrt(
        proportion * (1.0 - proportion) / n + z2 / (4.0 * n * n)) /
        denominator;
    return {std::max(0.0, center - half_width),
            std::min(1.0, center + half_width)};
}

// ---------------------------------------------------------------------
// Spec-mandated pre-implementation validation: the reference pattern
// correlated against itself must show a peak of exactly 68 at zero lag.
// This is the required check before trusting the detector on real data --
// it catches bit-order, BPSK-polarity, or pattern-length bugs immediately.
// ---------------------------------------------------------------------
bool TestSelfCorrelation() {
    const auto reference = lunanet::gateway5::BuildSyncReferenceSymbols();
    const std::vector<double> reference_as_double(reference.begin(), reference.end());

    const auto correlation = lunanet::gateway5::ComputeSyncCorrelation(reference_as_double);
    if (correlation.size() != 1) {
        std::cerr << "FAIL [self-correlation]: expected exactly 1 valid offset "
                  << "(input length == pattern length), got " << correlation.size() << "\n";
        return false;
    }
    if (std::fabs(correlation[0] - 68.0) > 1e-9) {
        std::cerr << "FAIL [self-correlation]: expected peak of 68.0 at zero lag, got "
                  << correlation[0] << "\n";
        return false;
    }
    return true;
}

// Embed the reference pattern in a longer random +-1 stream and confirm
// every OTHER offset's correlation is small relative to the true peak --
// i.e. the autocorrelation is genuinely sharp, not just correct at lag 0.
bool TestSelfCorrelationSidelobes() {
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    const auto reference = lunanet::gateway5::BuildSyncReferenceSymbols();
    const std::size_t pattern_len = reference.size();

    std::vector<double> stream(500);
    for (auto& v : stream) {
        v = (noise(rng) >= 0.0) ? 1.0 : -1.0;
    }

    const std::size_t true_offset = 200;
    for (std::size_t i = 0; i < pattern_len; ++i) {
        stream[true_offset + i] = static_cast<double>(reference[i]);
    }

    const auto correlation = lunanet::gateway5::ComputeSyncCorrelation(stream);
    if (correlation.size() != stream.size() - pattern_len + 1) {
        std::cerr << "FAIL [sidelobes]: unexpected correlation vector length\n";
        return false;
    }

    for (std::size_t offset = 0; offset < correlation.size(); ++offset) {
        if (offset == true_offset) continue;
        if (std::fabs(correlation[offset]) >= 68.0 * 0.5) {
            std::cerr << "FAIL [sidelobes]: sidelobe at offset " << offset
                      << " unexpectedly large: " << correlation[offset] << "\n";
            return false;
        }
    }
    return true;
}

bool TestNoiselessOffsetRecovery() {
    std::mt19937 rng(0x5EED1u);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    const auto reference = lunanet::gateway5::BuildSyncReferenceSymbols();
    const std::size_t pattern_len = reference.size();

    std::vector<double> stream(3000);
    for (auto& v : stream) {
        v = (noise(rng) >= 0.0) ? 1.0 : -1.0;
    }

    const std::size_t true_offset = 1234;
    for (std::size_t i = 0; i < pattern_len; ++i) {
        stream[true_offset + i] = static_cast<double>(reference[i]);
    }

    const auto result = lunanet::gateway5::DetectFrameSync(stream);
    if (!result.detected) {
        std::cerr << "FAIL [noiseless-recovery]: expected detection, PSR=" << result.psr << "\n";
        return false;
    }
    if (result.frame_offset != true_offset) {
        std::cerr << "FAIL [noiseless-recovery]: expected offset " << true_offset
                  << ", got " << result.frame_offset << "\n";
        return false;
    }
    return true;
}

bool TestNoDetectionOnPureNoise() {
    std::mt19937 rng(0xBADC0DEu);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    std::vector<double> stream(2000);
    for (auto& v : stream) {
        v = (noise(rng) >= 0.0) ? 1.0 : -1.0;
    }

    const auto result = lunanet::gateway5::DetectFrameSync(stream);
    if (result.detected) {
        std::cerr << "FAIL [pure-noise]: expected no detection on pure noise, got offset "
                  << result.frame_offset << " PSR=" << result.psr << "\n";
        return false;
    }
    return true;
}

bool TestRejectsShortInput() {
    const std::vector<double> too_short(10, 1.0);

    const auto result = lunanet::gateway5::DetectFrameSync(too_short);
    if (result.detected) {
        std::cerr << "FAIL [short-input]: expected no detection for input shorter than the pattern\n";
        return false;
    }

    const auto correlation = lunanet::gateway5::ComputeSyncCorrelation(too_short);
    if (!correlation.empty()) {
        std::cerr << "FAIL [short-input]: expected an empty correlation vector\n";
        return false;
    }
    return true;
}

bool TestRejectsInvalidConfidenceInput() {
    std::vector<double> stream(300, 1.0);
    stream[100] = std::numeric_limits<double>::quiet_NaN();
    if (lunanet::gateway5::DetectFrameSync(stream).detected) {
        std::cerr << "FAIL [invalid-input]: non-finite stream was accepted\n";
        return false;
    }

    const std::vector<double> finite_stream(300, 1.0);
    if (lunanet::gateway5::DetectFrameSync(finite_stream, 0.0).detected ||
        lunanet::gateway5::DetectFrameSync(finite_stream, 1.0, 0.0).detected ||
        lunanet::gateway5::DetectFrameSync(finite_stream, 1.0, 1.0, 1.1).detected) {
        std::cerr << "FAIL [invalid-input]: invalid confidence threshold was accepted\n";
        return false;
    }
    return true;
}

// Statistical validation of the spec's stated performance target
// (">99% correct detection at SNR > 0 dB"). Exercise 0.1 dB so the automated
// qualification remains immediately above the boundary instead of relying on
// previous 3 dB margin.
// SNR(dB) = 10*log10(signal_power/noise_power); for unit-amplitude BPSK,
// noise_stddev = 10^(-SNR_dB/20).
bool TestMonteCarloDetectionRateAboveZeroDb() {
    std::mt19937 rng(0x1234ABCDu);
    std::normal_distribution<double> gaussian(0.0, 1.0);

    const auto reference = lunanet::gateway5::BuildSyncReferenceSymbols();
    const std::size_t pattern_len = reference.size();

    const double snr_db = 0.1;
    const double noise_stddev = std::pow(10.0, -snr_db / 20.0);

    const int kTrials = 10000;
    int successes = 0;

    for (int trial = 0; trial < kTrials; ++trial) {
        std::vector<double> stream(2000);
        for (auto& v : stream) {
            const double bit = (gaussian(rng) >= 0.0) ? 1.0 : -1.0;
            v = bit + gaussian(rng) * noise_stddev;
        }

        std::uniform_int_distribution<std::size_t> offset_dist(0, stream.size() - pattern_len);
        const std::size_t true_offset = offset_dist(rng);
        for (std::size_t i = 0; i < pattern_len; ++i) {
            stream[true_offset + i] = static_cast<double>(reference[i]) + gaussian(rng) * noise_stddev;
        }

        const auto result = lunanet::gateway5::DetectFrameSync(stream);
        if (result.detected && result.frame_offset == true_offset) {
            ++successes;
        }
    }

    const double success_rate = static_cast<double>(successes) / kTrials;
    const WilsonInterval confidence = OneSidedWilsonInterval(successes, kTrials);
    std::cout << "  (Monte Carlo: " << successes << "/" << kTrials << " = "
              << (success_rate * 100.0) << "% detected at SNR=" << snr_db
              << " dB; one-sided 95% lower bound=" << (confidence.lower * 100.0)
              << "%)\n";

    if (confidence.lower <= 0.99) {
        std::cerr << "FAIL [monte-carlo]: 95% lower bound "
                  << (confidence.lower * 100.0)
                  << "% does not exceed the spec's 99% target at SNR>0dB\n";
        return false;
    }
    return true;
}

bool TestMonteCarloFalseAlarmRate() {
    std::mt19937 rng(0xFA15EA1u);
    std::normal_distribution<double> gaussian(0.0, 1.0);
    constexpr int kTrials = 10000;
    constexpr double kSnrDb = 0.1;
    const double noise_stddev = std::pow(10.0, -kSnrDb / 20.0);
    int false_alarms = 0;

    for (int trial = 0; trial < kTrials; ++trial) {
        std::vector<double> stream(2000);
        for (double& value : stream) {
            const double random_symbol = (gaussian(rng) >= 0.0) ? 1.0 : -1.0;
            value = random_symbol + gaussian(rng) * noise_stddev;
        }
        if (lunanet::gateway5::DetectFrameSync(stream).detected) {
            ++false_alarms;
        }
    }

    const double false_alarm_rate = static_cast<double>(false_alarms) / kTrials;
    const WilsonInterval confidence = OneSidedWilsonInterval(false_alarms, kTrials);
    std::cout << "  (Monte Carlo: " << false_alarms << "/" << kTrials << " = "
              << (false_alarm_rate * 100.0)
              << "% false alarms; one-sided 95% upper bound="
              << (confidence.upper * 100.0) << "%)\n";
    if (confidence.upper >= 0.01) {
        std::cerr << "FAIL [false-alarm]: 95% upper bound "
                  << (confidence.upper * 100.0)
                  << "% is not below the 1% qualification bound\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    if (TestSelfCorrelation()) {
        std::cout << "PASS: self-correlation peak is exactly 68.0 at zero lag\n";
    } else {
        ok = false;
    }

    if (TestSelfCorrelationSidelobes()) {
        std::cout << "PASS: embedded pattern produces a sharp peak with small sidelobes\n";
    } else {
        ok = false;
    }

    if (TestNoiselessOffsetRecovery()) {
        std::cout << "PASS: noiseless embedded pattern's offset is recovered exactly\n";
    } else {
        ok = false;
    }

    if (TestNoDetectionOnPureNoise()) {
        std::cout << "PASS: pure noise stream correctly reports no detection\n";
    } else {
        ok = false;
    }

    if (TestRejectsShortInput()) {
        std::cout << "PASS: input shorter than the sync pattern is rejected gracefully\n";
    } else {
        ok = false;
    }

    if (TestRejectsInvalidConfidenceInput()) {
        std::cout << "PASS: non-finite streams and invalid sync thresholds are rejected\n";
    } else {
        ok = false;
    }

    if (TestMonteCarloDetectionRateAboveZeroDb()) {
        std::cout << "PASS: detection rate exceeds 99% at SNR > 0 dB (spec target)\n";
    } else {
        ok = false;
    }

    if (TestMonteCarloFalseAlarmRate()) {
        std::cout << "PASS: no-frame false-alarm rate is at most 1%\n";
    } else {
        ok = false;
    }

    return ok ? 0 : 1;
}
