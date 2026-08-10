#pragma once
#ifndef LUNANET_GATEWAY5_BCH_SOFT_DECODER_H
#define LUNANET_GATEWAY5_BCH_SOFT_DECODER_H

#include <cstdint>
#include <optional>
#include <vector>

namespace lunanet::gateway5 {

constexpr int kStage3Sb1SoftSymbols = 52;
constexpr int kStage3InnerSoftSymbols = 51;
constexpr double kDefaultBchMinNormalizedCorrelation = 0.5;
constexpr double kDefaultBchMinNormalizedMargin = 0.05;

struct BchSoftDecodeResult {
	bool decoded = false;
	int value = -1;
	double normalized_correlation = 0.0;
	double normalized_margin = 0.0;
};

/**
 * Gateway 5 Stage 3: exhaustive ML BCH soft-decode for SB1.
 *
 * Input must be the 52 signed soft values from ExtractedFrame.sb1:
 *   - sb1_soft[0] = prepended MSB symbol
 *   - sb1_soft[1..51] = 51 BCH inner symbols
 *
 * Decoder rule:
 *   1. Enumerate all 256 MSB=0 BCH hypotheses (cached).
 *   2. Correlate each hypothesis with the 51 inner soft symbols:
 *      add |s| for sign match, subtract |s| for sign mismatch.
 *   3. Choose max |correlation|.
 *   4. MSB = 0 if best correlation > 0, else MSB = 1.
 *      (A correlation of exactly 0.0 is a deliberate tie-break toward
 *      MSB=1; this matches gateway2::BchDecodeSoft's identical rule.)
 *
 * @param sb1_soft 52 signed soft values.
 * @return Decoded 9-bit FID+TOI (bits[8:7]=FID, bits[6:0]=TOI), or -1 on
 *         invalid input size.
 */
BchSoftDecodeResult DecodeSb1BchSoftDetailed(
	const std::vector<double>& sb1_soft,
	double min_normalized_correlation = kDefaultBchMinNormalizedCorrelation,
	double min_normalized_margin = kDefaultBchMinNormalizedMargin);

// Compatibility wrapper returning the decoded value or -1 when confidence
// checks reject the input.
int DecodeSb1BchSoft(const std::vector<double>& sb1_soft);

/**
 * Packs a BCH symbol vector (MSB-first) into a 52-bit integer.
 *
 * @param symbols 52 binary symbols.
 * @return Packed 52-bit value in the low bits of uint64_t, or std::nullopt
 *         on invalid input size. (0 is a legitimate all-zero codeword, so
 *         it cannot double as an error sentinel.)
 */
std::optional<uint64_t> PackBch52MsbFirst(const std::vector<uint8_t>& symbols);

/**
 * Unpacks a 52-bit integer (MSB-first) into signed soft values.
 *
 * Bit mapping uses logic 0 -> +1.0 and logic 1 -> -1.0.
 */
std::vector<double> UnpackBch52ToSoft(uint64_t packed);

}  // namespace lunanet::gateway5

#endif  // LUNANET_GATEWAY5_BCH_SOFT_DECODER_H
