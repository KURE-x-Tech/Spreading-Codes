#pragma once
#ifndef LUNANET_GATEWAY5_LDPC_DECODER_H
#define LUNANET_GATEWAY5_LDPC_DECODER_H

#include <string>
#include <vector>

#include "gateway2/ldpc_encoder.h"

namespace lunanet::gateway5 {

// SB3/SB4 filler bits are shortened code bits whose transmitted value is
// known to be zero. A strong finite prior preserves that information without
// introducing infinities into min-sum message arithmetic.
constexpr double kLdpcMessageLimit = 50.0;
constexpr double kShortenedZeroLlr = kLdpcMessageLimit;

struct LdpcDecodeResult {
    std::vector<uint8_t> decoded_data_bits;
    bool converged = false;
    int iterations = 0;
    int syndrome_weight = -1;
};

/**
 * Build parity-check matrix H for codeword layout (s; p1; p2):
 *   band1: A*s XOR B*p1 = 0
 *   band2: C*s XOR D*p1 XOR p2 = 0
 *
 * H = [A B 0; C D I]
 */
bool BuildParityCheckMatrix(const lunanet::gateway2::LdpcMatrices& matrices,
                            const lunanet::gateway2::BinaryMatrix& b_matrix,
                            lunanet::gateway2::BinaryMatrix* out_h,
                            std::string* error_message = nullptr);

/**
 * Reconstruct full-codeword LLRs by inverting LdpcEncode puncturing exactly.
 *
 * Unobserved symbols are filled with 0.0 LLR (erasure), including punctured
 * systematic positions and non-transmitted parity tail.
 */
std::vector<double> RestorePuncturedCodewordLlrs(
    const std::vector<double>& received_llrs,
    const lunanet::gateway2::LdpcParams& params,
    int p1_bits,
    int p2_bits,
    std::string* error_message = nullptr);

/**
 * Decode one LDPC subframe using normalized min-sum (flooding schedule).
 *
 * - alpha defaults to 0.75
 * - max_iterations defaults to 50
 * - hard decision: LLR < 0 => bit 1, else bit 0
 * - SB3/SB4 output strips the 10 filler bits automatically
 */
LdpcDecodeResult DecodeLdpcMinSum(
    const std::vector<double>& received_llrs,
    const lunanet::gateway2::LdpcMatrices& matrices,
    const lunanet::gateway2::BinaryMatrix& b_matrix,
    const lunanet::gateway2::LdpcParams& params,
    int max_iterations = 50,
    double alpha = 0.75,
    std::string* error_message = nullptr);

}  // namespace lunanet::gateway5

#endif  // LUNANET_GATEWAY5_LDPC_DECODER_H
