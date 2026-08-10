#pragma once
#ifndef LUNANET_GATEWAY5_DESPREADER_H
#define LUNANET_GATEWAY5_DESPREADER_H

#include <cstddef>
#include <string>
#include <vector>

#include "gateway1/spreading_config.h"

namespace lunanet::gateway5 {

// Number of Gold primary-code chips per AFS-I data symbol (LSIS-160): one
// full 2046-chip primary code epoch is XORed with each data symbol before
// BPSK mapping (see gateway4::ModulateAfsIData). Mirrors
// gateway4::kAfsIChipsPerSymbol without adding a hard dependency on
// gateway4's signal-generation config from this decode-side module.
constexpr int kDespreadChipsPerSymbol = 2046;

// Minimum cosine-normalized correlation
// (|correlation| / sqrt(code_energy * received_window_energy)) required to
// declare a code-phase lock. Not specified by the LSIS-AFS
// spec -- this stage is only described there as "de-spreading using known
// codes" (REQUIREMENTS_MATRIX FSD-5.3), with no numeric acceptance
// criterion. Chosen empirically (see despreader_test.cpp): Gold codes have
// autocorrelation sidelobes far below their peak, so the noiseless
// separation is large (locked ~1.0, unlocked candidate phases well below
// 0.1), giving a lot of margin at 0.5.
constexpr double kDefaultLockThreshold = 0.5;

struct DespreadResult {
    bool locked = false;
    std::size_t code_phase = 0;   // Chip offset where the lock was found.
    double lock_correlation = 0.0;  // Scale-invariant correlation at code_phase.
    std::vector<double> symbols;    // One soft value per despread symbol.
};

/**
 * Despreads an AFS-I chip-rate signal against the known PRN's Gold primary
 * code, recovering one soft data-symbol value per 2046-chip window.
 *
 * `chip_stream` must already be at the AFS-I chip rate with one sample per
 * chip -- this is gateway4::GenerateIq's *default* sample-rate output (its
 * `i` channel), matching the workshop interop contract
 * (--rate 1023000). Resampling a higher-rate signal down to this convention
 * is the caller's responsibility.
 *
 * Two-phase algorithm:
 *   1. Code-phase acquisition: correlate the first code period of
 *      `chip_stream` against the reference code at every candidate phase
 *      offset in [0, 2046). The correlation MAGNITUDE peaks at the true
 *      phase regardless of the (unknown) data-bit sign, since XORing a data
 *      bit onto the whole code only flips the overall correlation sign, not
 *      its magnitude (BpskMap(a XOR b) == BpskMap(a) * BpskMap(b)).
 *   2. Integrate-and-dump: once locked, correlate every subsequent
 *      2046-chip window at that phase to produce one soft symbol per
 *      window.
 *
 * Scope (deliberate): chip-phase (code-phase) search only. Does not
 * compensate for Doppler, carrier offset, or sub-chip timing error -- this
 * is a software-only reference implementation with no RF front-end (see
 * README's "what you're not building"). Does not despread AFS-Q (the pilot
 * channel carries no data; see the Stage 1 spec's optional tertiary-code
 * backup for a case where that would matter).
 *
 * @param chip_stream    AFS-I samples at the chip rate (sign = chip value;
 *                       noiseless samples are exactly +-1.0).
 * @param prn            PRN whose Gold primary code to despread against.
 * @param tables         Loaded spreading-code tables (see
 *                       gateway1::LoadSpreadingConfig).
 * @param lock_threshold Minimum normalized correlation to declare lock.
 * @param error_message  Optional error string on failure.
 * @return DespreadResult with locked=false if `chip_stream` is shorter than
 *         one code period, code generation for `prn` fails, or no phase
 *         clears lock_threshold.
 */
DespreadResult DespreadAfsI(const std::vector<double>& chip_stream,
                             int prn,
                             const lunanet::gateway1::SpreadingSpecTables& tables,
                             double lock_threshold = kDefaultLockThreshold,
                             std::string* error_message = nullptr);

}  // namespace lunanet::gateway5

#endif  // LUNANET_GATEWAY5_DESPREADER_H
