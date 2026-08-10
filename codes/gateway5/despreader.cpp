#include "gateway5/despreader.h"

#include "gateway1/gold_code_generator.h"
#include "gateway4/bpsk_modulator.h"

#include <algorithm>
#include <cmath>

namespace lunanet::gateway5 {

DespreadResult DespreadAfsI(const std::vector<double>& chip_stream,
                             int prn,
                             const lunanet::gateway1::SpreadingSpecTables& tables,
                             double lock_threshold,
                             std::string* error_message) {
    DespreadResult result;

    if (!std::isfinite(lock_threshold) || lock_threshold <= 0.0 ||
        lock_threshold > 1.0) {
        if (error_message) {
            *error_message = "lock_threshold must be finite and in (0, 1]";
        }
        return result;
    }
    if (!std::all_of(chip_stream.begin(), chip_stream.end(), [](const double value) {
            return std::isfinite(value);
        })) {
        if (error_message) *error_message = "chip_stream contains a non-finite value";
        return result;
    }

    if (chip_stream.size() < static_cast<std::size_t>(kDespreadChipsPerSymbol)) {
        if (error_message) {
            *error_message = "chip_stream shorter than one code period (" +
                std::to_string(kDespreadChipsPerSymbol) + " chips)";
        }
        return result;
    }

    std::string gen_error;
    const std::vector<uint8_t> primary_code =
        lunanet::gateway1::GenerateGoldCode(prn, tables, &gen_error);
    if (primary_code.size() != static_cast<std::size_t>(kDespreadChipsPerSymbol)) {
        if (error_message) {
            *error_message = gen_error.empty()
                ? "Failed to generate Gold primary code for PRN " + std::to_string(prn)
                : gen_error;
        }
        return result;
    }

    std::vector<double> code_bpsk(kDespreadChipsPerSymbol);
    for (int i = 0; i < kDespreadChipsPerSymbol; ++i) {
        code_bpsk[i] = static_cast<double>(lunanet::gateway4::BpskMap(primary_code[i]));
    }

    // --- Phase 1: code-phase acquisition -----------------------------------
    // Search every candidate phase within one code period using only the
    // first window of the stream. Track the largest-MAGNITUDE correlation
    // (not signed) since the unknown data-bit sign only flips the overall
    // sign, not the magnitude, of a correctly-phased correlation.
    std::size_t best_phase = 0;
    double best_normalized_corr = -1.0;

    const std::size_t max_phase_exclusive =
        std::min(static_cast<std::size_t>(kDespreadChipsPerSymbol),
                 chip_stream.size() - static_cast<std::size_t>(kDespreadChipsPerSymbol) + 1);

    for (std::size_t phase = 0; phase < max_phase_exclusive; ++phase) {
        double corr = 0.0;
        double window_energy = 0.0;
        for (int i = 0; i < kDespreadChipsPerSymbol; ++i) {
            const double sample = chip_stream[phase + i];
            corr += sample * code_bpsk[i];
            window_energy += sample * sample;
        }
        constexpr double kMinWindowEnergy = 1e-12;
        const double denominator = std::sqrt(
            static_cast<double>(kDespreadChipsPerSymbol) *
            std::max(window_energy, kMinWindowEnergy));
        const double normalized_corr = std::fabs(corr) / denominator;
        if (normalized_corr > best_normalized_corr) {
            best_normalized_corr = normalized_corr;
            best_phase = phase;
        }
    }

    result.lock_correlation = best_normalized_corr;

    if (best_normalized_corr < lock_threshold) {
        if (error_message) {
            *error_message = "Failed to acquire code-phase lock for PRN " +
                std::to_string(prn) + " (normalized correlation " +
                std::to_string(best_normalized_corr) + " below threshold " +
                std::to_string(lock_threshold) + ")";
        }
        return result;
    }

    result.locked = true;
    result.code_phase = best_phase;

    // --- Phase 2: integrate-and-dump ---------------------------------------
    // Walk every subsequent chips-per-symbol window at the locked phase,
    // correlating each against the reference code to recover one soft
    // symbol value per window. Trailing chips that don't fill a full
    // window are dropped (matches ExtractFrameSymbols' "no partial frame"
    // convention elsewhere in Gateway 5).
    std::size_t pos = best_phase;
    while (pos + static_cast<std::size_t>(kDespreadChipsPerSymbol) <= chip_stream.size()) {
        double corr = 0.0;
        for (int i = 0; i < kDespreadChipsPerSymbol; ++i) {
            corr += chip_stream[pos + i] * code_bpsk[i];
        }
        result.symbols.push_back(corr / static_cast<double>(kDespreadChipsPerSymbol));
        pos += static_cast<std::size_t>(kDespreadChipsPerSymbol);
    }

    return result;
}

}  // namespace lunanet::gateway5
