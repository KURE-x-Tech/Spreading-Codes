#pragma once
#ifndef LUNANET_GATEWAY5_SYNC_DETECTOR_H
#define LUNANET_GATEWAY5_SYNC_DETECTOR_H

#include <cstddef>
#include <vector>

namespace lunanet::gateway5 {

// Detection combines positive peak-to-sidelobe ratio with peak-to-RMS
// separation from the full sidelobe floor. The two metrics avoid making one
// extreme negative correlation the deciding factor on long streams.
constexpr double kDefaultSyncPsrThreshold = 1.10;
constexpr double kDefaultSyncPeakToRmsThreshold = 3.75;
constexpr double kDefaultSyncNormalizedPeakThreshold = 0.55;

// Exclusion window (in symbols) around the peak, inside which candidate
// offsets are NOT considered when searching for the "next-highest sidelobe"
// used in the PSR calculation. One full pattern length (68) is used so the
// sidelobe search isn't contaminated by the main peak's own correlation
// shoulder.
constexpr int kSyncSidelobeExclusion = 68;

struct FrameSyncResult {
    bool detected = false;
    std::size_t frame_offset = 0;
    double peak_correlation = 0.0;
    double psr = 0.0;
    double peak_to_rms = 0.0;
    double normalized_peak = 0.0;
};

/**
 * Computes the raw correlation of the 68-symbol BPSK sync reference
 * pattern (BuildSyncReferenceSymbols) against `received` at every valid
 * offset (0 .. received.size() - 68).
 *
 * Exposed separately from DetectFrameSync so tests can validate the
 * self-correlation property mandated by the spec before trusting the
 * detector on real data, and so callers can inspect the full correlation
 * profile if needed.
 *
 * @param received  Despread, symbol-rate soft values to search.
 * @return One correlation value per valid offset, or empty if `received`
 *         is shorter than the 68-symbol pattern.
 */
std::vector<double> ComputeSyncCorrelation(const std::vector<double>& received);

/**
 * Slides the 68-symbol BPSK sync reference pattern across `received` and
 * locates the frame-start offset via peak-to-sidelobe-ratio (PSR)
 * detection, per LSIS-AFS Table 8 / Sec 2.3 (Gateway 5 Stage 1).
 *
 * `received` must already be despread, symbol-rate soft values (sign = bit
 * guess, magnitude = confidence) -- the same convention ExtractFrameSymbols
 * consumes. Raw I/Q must first pass through DecodeAfsIIqSignal or
 * DespreadAfsI. This detector does not perform sub-symbol/Doppler
 * interpolation; its output is an integer symbol offset.
 *
 * An absolute correlation threshold is deliberately not used: the spec
 * explicitly calls this out as unreliable when channel power varies.
 *
 * @param received               Despread, symbol-rate soft values to search.
 * @param psr_threshold          Minimum positive PSR to declare a detection.
 * @param peak_to_rms_threshold  Minimum peak-to-RMS floor separation.
 * @param normalized_peak_threshold Minimum normalized matched correlation.
 * @return FrameSyncResult with detected=false if `received` is shorter
 *         than the 68-symbol pattern, or if no candidate offset clears
 *         psr_threshold.
 */
FrameSyncResult DetectFrameSync(const std::vector<double>& received,
                                 double psr_threshold = kDefaultSyncPsrThreshold,
                                 double peak_to_rms_threshold =
                                     kDefaultSyncPeakToRmsThreshold,
                                 double normalized_peak_threshold =
                                     kDefaultSyncNormalizedPeakThreshold);

}  // namespace lunanet::gateway5

#endif  // LUNANET_GATEWAY5_SYNC_DETECTOR_H
