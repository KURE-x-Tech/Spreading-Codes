#include "gateway2/crc24.h"
#include "gateway2/ldpc_encoder.h"
#include "gateway5/crc_validator.h"
#include "gateway5/frame_decoder.h"
#include "gateway5/ldpc_decoder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kTrials = 102;
constexpr double kSnrDb = 3.0;

std::vector<uint8_t> RandomProtectedBits(int payload_bits, std::mt19937* rng) {
    std::uniform_int_distribution<int> bit(0, 1);
    std::vector<uint8_t> bits(static_cast<std::size_t>(payload_bits), 0u);
    for (uint8_t& value : bits) {
        value = static_cast<uint8_t>(bit(*rng));
    }
    lunanet::gateway2::Crc24Append(bits);
    return bits;
}

std::vector<double> AddAwgnAndComputeLlrs(const std::vector<uint8_t>& encoded,
                                          double noise_variance,
                                          std::mt19937* rng) {
    std::normal_distribution<double> gaussian(0.0, std::sqrt(noise_variance));
    std::vector<double> llrs;
    llrs.reserve(encoded.size());
    for (const uint8_t bit : encoded) {
        const double transmitted = bit == 0u ? +1.0 : -1.0;
        const double received = transmitted + gaussian(*rng);
        llrs.push_back(2.0 * received / noise_variance);
    }
    return llrs;
}

std::uint64_t CountBitErrors(const std::vector<uint8_t>& expected,
                             const std::vector<uint8_t>& actual) {
    if (expected.size() != actual.size()) {
        return static_cast<std::uint64_t>(expected.size());
    }
    std::uint64_t errors = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] != actual[i]) {
            ++errors;
        }
    }
    return errors;
}

}  // namespace

int main() {
    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::filesystem::path csv_dir = repo_root / "Validation" / "annex3" / "csv";

    std::string error;
    lunanet::gateway5::DecoderMatrices matrices;
    if (!lunanet::gateway5::LoadDecoderMatrices(csv_dir.string(), &matrices, &error)) {
        std::cerr << "FAIL: could not load decoder matrices: " << error << "\n";
        return 1;
    }

    const double noise_variance = std::pow(10.0, -kSnrDb / 10.0);
    std::mt19937 rng(0xBEE5A17u);
    std::uint64_t total_bits = 0;
    std::uint64_t bit_errors = 0;
    int accepted_frames = 0;
    int total_iterations = 0;
    int max_iterations = 0;
    double worst_frame_ms = 0.0;

    for (int trial = 0; trial < kTrials; ++trial) {
        const auto sb2 = RandomProtectedBits(1176, &rng);
        const auto sb3 = RandomProtectedBits(846, &rng);
        const auto sb4 = RandomProtectedBits(846, &rng);

        const auto sb2_encoded = lunanet::gateway2::LdpcEncode(
            sb2, matrices.sb2, lunanet::gateway2::kLdpcSb2, &error);
        const auto sb3_encoded = lunanet::gateway2::LdpcEncode(
            sb3, matrices.sb34, lunanet::gateway2::kLdpcSb34, &error);
        const auto sb4_encoded = lunanet::gateway2::LdpcEncode(
            sb4, matrices.sb34, lunanet::gateway2::kLdpcSb34, &error);
        if (sb2_encoded.empty() || sb3_encoded.empty() || sb4_encoded.empty()) {
            std::cerr << "FAIL: LDPC encode failed in trial " << trial << ": " << error << "\n";
            return 1;
        }

        const auto frame_start = std::chrono::steady_clock::now();
        const auto sb2_decoded = lunanet::gateway5::DecodeLdpcMinSum(
            AddAwgnAndComputeLlrs(sb2_encoded, noise_variance, &rng),
            matrices.sb2, matrices.sb2_b, lunanet::gateway2::kLdpcSb2,
            50, 0.75, &error);
        const auto sb3_decoded = lunanet::gateway5::DecodeLdpcMinSum(
            AddAwgnAndComputeLlrs(sb3_encoded, noise_variance, &rng),
            matrices.sb34, matrices.sb34_b, lunanet::gateway2::kLdpcSb34,
            50, 0.75, &error);
        const auto sb4_decoded = lunanet::gateway5::DecodeLdpcMinSum(
            AddAwgnAndComputeLlrs(sb4_encoded, noise_variance, &rng),
            matrices.sb34, matrices.sb34_b, lunanet::gateway2::kLdpcSb34,
            50, 0.75, &error);
        const double frame_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - frame_start).count();
        worst_frame_ms = std::max(worst_frame_ms, frame_ms);

        bit_errors += CountBitErrors(sb2, sb2_decoded.decoded_data_bits);
        bit_errors += CountBitErrors(sb3, sb3_decoded.decoded_data_bits);
        bit_errors += CountBitErrors(sb4, sb4_decoded.decoded_data_bits);
        total_bits += sb2.size() + sb3.size() + sb4.size();

        total_iterations += sb2_decoded.iterations +
            sb3_decoded.iterations + sb4_decoded.iterations;
        max_iterations = std::max({max_iterations,
                                   sb2_decoded.iterations,
                                   sb3_decoded.iterations,
                                   sb4_decoded.iterations});

        if (sb2_decoded.converged && sb3_decoded.converged && sb4_decoded.converged &&
            lunanet::gateway5::ValidateFrameCrc(
                sb2_decoded.decoded_data_bits,
                sb3_decoded.decoded_data_bits,
                sb4_decoded.decoded_data_bits).frame_accepted) {
            ++accepted_frames;
        }
    }

    const double measured_ber = static_cast<double>(bit_errors) /
        static_cast<double>(total_bits);
    const double average_iterations = static_cast<double>(total_iterations) /
        static_cast<double>(kTrials * 3);

    std::cout << std::scientific << std::setprecision(6)
              << "Gateway 5 BER qualification\n"
              << "  SNR: " << kSnrDb << " dB\n"
              << "  frames: " << kTrials << "\n"
              << "  decoded bits: " << total_bits << "\n"
              << "  bit errors: " << bit_errors << "\n"
              << "  measured BER: " << measured_ber << "\n"
              << "  CRC-accepted frames: " << accepted_frames << "/" << kTrials << "\n"
              << "  average LDPC iterations: " << average_iterations << "\n"
              << "  maximum LDPC iterations: " << max_iterations << "\n"
              << "  worst three-subframe decode: " << worst_frame_ms << " ms\n";

    if (measured_ber >= 1e-5) {
        std::cerr << "FAIL: observed BER is not below 1e-5\n";
        return 1;
    }
    if (accepted_frames != kTrials) {
        std::cerr << "FAIL: one or more frames failed LDPC convergence or CRC\n";
        return 1;
    }
    if (max_iterations >= 50) {
        std::cerr << "FAIL: LDPC reached the 50-iteration limit\n";
        return 1;
    }
    if (worst_frame_ms >= 1000.0) {
        std::cerr << "FAIL: frame decode exceeded one second\n";
        return 1;
    }

    std::cout << "PASS: empirical BER, CRC acceptance, iteration, and latency targets met\n";
    return 0;
}