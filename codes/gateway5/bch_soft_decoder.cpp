#include "gateway5/bch_soft_decoder.h"

#include "gateway2/bch_codec.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace lunanet::gateway5 {

namespace {

const std::array<std::array<uint8_t, gateway2::kBchLfsrOutputSymbols>, 256>&
GetBchHypotheses() {
    // The 256 MSB=0 BCH hypotheses are constant, so cache once.
    static const auto codebook = gateway2::BchGenerateCodebook();
    return codebook;
}

}  // namespace

BchSoftDecodeResult DecodeSb1BchSoftDetailed(
    const std::vector<double>& sb1_soft,
    double min_normalized_correlation,
    double min_normalized_margin) {
    BchSoftDecodeResult result;
    if (sb1_soft.size() != static_cast<std::size_t>(kStage3Sb1SoftSymbols) ||
        !std::all_of(sb1_soft.begin(), sb1_soft.end(), [](const double value) {
            return std::isfinite(value);
        }) ||
        !std::isfinite(min_normalized_correlation) ||
        min_normalized_correlation < 0.0 || min_normalized_correlation > 1.0 ||
        !std::isfinite(min_normalized_margin) ||
        min_normalized_margin < 0.0 || min_normalized_margin > 1.0) {
        return result;
    }

    const auto& hypotheses = GetBchHypotheses();

    double best_abs_corr = -1.0;
    double second_best_abs_corr = -1.0;
    double best_raw_corr = 0.0;
    double total_magnitude = 0.0;
    int best_data = 0;

    for (int i = 0; i < kStage3InnerSoftSymbols; ++i) {
        total_magnitude += std::abs(sb1_soft[static_cast<std::size_t>(i + 1)]);
    }
    constexpr double kMinTotalMagnitude = 1e-12;
    if (total_magnitude <= kMinTotalMagnitude) {
        return result;
    }

    for (int data = 0; data < 256; ++data) {
        double corr = 0.0;

        for (int i = 0; i < kStage3InnerSoftSymbols; ++i) {
            const double soft = sb1_soft[static_cast<std::size_t>(i + 1)];
            const double magnitude = std::abs(soft);

            // LSIS sign convention: positive soft => bit guess 0.
            const bool received_zero = (soft >= 0.0);
            const bool hypothesis_zero = (hypotheses[data][i] == 0);

            corr += (received_zero == hypothesis_zero) ? magnitude : -magnitude;
        }

        const double abs_corr = std::abs(corr);
        if (abs_corr > best_abs_corr) {
            second_best_abs_corr = best_abs_corr;
            best_abs_corr = abs_corr;
            best_raw_corr = corr;
            best_data = data;
        } else if (abs_corr > second_best_abs_corr) {
            second_best_abs_corr = abs_corr;
        }
    }

    result.normalized_correlation = best_abs_corr / total_magnitude;
    result.normalized_margin =
        (best_abs_corr - std::max(0.0, second_best_abs_corr)) / total_magnitude;
    if (result.normalized_correlation < min_normalized_correlation ||
        result.normalized_margin < min_normalized_margin) {
        return result;
    }

    const int decoded_msb = (best_raw_corr > 0.0) ? 0 : 1;
    result.value = (decoded_msb << 8) | best_data;
    result.decoded = true;
    return result;
}

int DecodeSb1BchSoft(const std::vector<double>& sb1_soft) {
    return DecodeSb1BchSoftDetailed(sb1_soft).value;
}

std::optional<uint64_t> PackBch52MsbFirst(const std::vector<uint8_t>& symbols) {
    if (symbols.size() != static_cast<std::size_t>(kStage3Sb1SoftSymbols)) {
        return std::nullopt;
    }

    uint64_t packed = 0;
    for (const uint8_t bit : symbols) {
        if (bit > 1u) {
            return std::nullopt;
        }
        packed <<= 1;
        packed |= static_cast<uint64_t>(bit);
    }
    return packed;
}

std::vector<double> UnpackBch52ToSoft(uint64_t packed) {
    std::vector<double> soft(kStage3Sb1SoftSymbols, 0.0);

    for (int i = 0; i < kStage3Sb1SoftSymbols; ++i) {
        const int bit_index_from_lsb = (kStage3Sb1SoftSymbols - 1) - i;
        const uint8_t bit = static_cast<uint8_t>((packed >> bit_index_from_lsb) & 0x1u);
        soft[static_cast<std::size_t>(i)] = (bit == 0u) ? +1.0 : -1.0;
    }

    return soft;
}

}  // namespace lunanet::gateway5
