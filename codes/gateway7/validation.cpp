#include <cstdint>
#include <vector>
#include <iostream>
#include <cmath>
#include <string>
#include "gateway2/crc24.h"
#include <random>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <fstream>
#include "gateway2/ldpc_encoder.h"
#include "gateway5/frame_decoder.h"
#include "gateway5/ldpc_decoder.h"
struct QualificationResult{
    double snr_db = 0;
    std::uint64_t total_bits = 0;
    std::uint64_t bit_errors = 0;
    double ber_result = 0;
    int accepted_frames = 0; 
    double acceptance_rate = 0;
    int total_iterations = 0;
    int max_iterations = 0;
    double average_iterations = 0;
    double worst_frame_ms = 0.0;
};

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

std::uint64_t CountBitErrors(
    const std::vector<uint8_t>& expected,
    const std::vector<uint8_t>& actual) {

    std::uint64_t error_count = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] != actual[i]) {
            error_count +=1;
        }
    }
    return error_count;
}

int main(){
    constexpr int k_trials = 102;
    std::vector<QualificationResult> results;
    std::vector<double> snr_db_values{
    -5.0, -4.0, -3.0, -2.0, -1.0, 0.0,
    1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0,
    3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0
};
    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::filesystem::path csv_dir = repo_root / "Validation" / "annex3" / "csv";

    std::string error;
    lunanet::gateway5::DecoderMatrices matrices;
    if (!lunanet::gateway5::LoadDecoderMatrices(csv_dir.string(), &matrices, &error)) {
        std::cerr << "FAIL: could not load decoder matrices: " << error << "\n";
        return 1;
    }
    
    for (double snr_db: snr_db_values){
        QualificationResult result;
        result.snr_db = snr_db;
        const double noise_variance = std::pow(10.0, -snr_db / 10.0);

        // Random number generator with a fixed seed for reproducibility
        std::mt19937 rng(42); 
        for (int trial = 0; trial < k_trials; ++trial){
            const auto sb2 = RandomProtectedBits(1176,&rng);
            const auto sb3 = RandomProtectedBits(846,&rng);
            const auto sb4 = RandomProtectedBits(846,&rng);
            //Encoding
            const auto sb2_encoded = lunanet::gateway2::LdpcEncode(
                sb2, matrices.sb2, lunanet::gateway2::kLdpcSb2, &error);
            const auto sb3_encoded = lunanet::gateway2::LdpcEncode(
                sb3, matrices.sb34, lunanet::gateway2::kLdpcSb34, &error);
            const auto sb4_encoded = lunanet::gateway2::LdpcEncode(
                sb4, matrices.sb34, lunanet::gateway2::kLdpcSb34, &error);
            
            if (sb2_encoded.empty() ||
                sb3_encoded.empty() ||
                sb4_encoded.empty()) {
                std::cerr << "FAIL: LDPC encode failed in trial " << trial << ": " << error << "\n";
                return 1;
            }
            //llr
            const auto sb2_llrs = AddAwgnAndComputeLlrs(
                sb2_encoded,
                noise_variance,
                &rng);

            const auto sb3_llrs = AddAwgnAndComputeLlrs(
                sb3_encoded,
                noise_variance,
                &rng);

            const auto sb4_llrs = AddAwgnAndComputeLlrs(
                sb4_encoded,
                noise_variance,
                &rng);
            //Decoding
        const auto frame_start = std::chrono::steady_clock::now();
            const auto sb2_decoded = lunanet::gateway5::DecodeLdpcMinSum(
            sb2_llrs,
            matrices.sb2,
            matrices.sb2_b,
            lunanet::gateway2::kLdpcSb2,
            50,
            0.75,
            &error);
        const auto sb3_decoded = lunanet::gateway5::DecodeLdpcMinSum(
            sb3_llrs,
            matrices.sb34, matrices.sb34_b, lunanet::gateway2::kLdpcSb34,
            50, 0.75, &error);
        const auto sb4_decoded = lunanet::gateway5::DecodeLdpcMinSum(
            sb4_llrs,
            matrices.sb34, matrices.sb34_b, lunanet::gateway2::kLdpcSb34,
            50, 0.75, &error);
        result.total_iterations +=
    sb2_decoded.iterations
    + sb3_decoded.iterations
    + sb4_decoded.iterations;
        result.max_iterations = std::max({result.max_iterations,
    sb2_decoded.iterations,
    sb3_decoded.iterations,
    sb4_decoded.iterations});
        const double frame_ms =
    std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - frame_start
    ).count();
    result.worst_frame_ms =
    std::max(result.worst_frame_ms, frame_ms);
        const auto bit_errors_sb2 = CountBitErrors(sb2, sb2_decoded.decoded_data_bits);
        result.bit_errors += bit_errors_sb2;
        result.total_bits += sb2.size();
        const auto bit_errors_sb3 = CountBitErrors(sb3, sb3_decoded.decoded_data_bits);
        result.bit_errors += bit_errors_sb3;
        result.total_bits += sb3.size();
        const auto bit_errors_sb4 = CountBitErrors(sb4, sb4_decoded.decoded_data_bits);
        result.bit_errors += bit_errors_sb4;
        result.total_bits += sb4.size();
        //CRC result
        const auto crc_result = lunanet::gateway5::ValidateFrameCrc(
        sb2_decoded.decoded_data_bits,
        sb3_decoded.decoded_data_bits,
        sb4_decoded.decoded_data_bits);

        if (sb2_decoded.converged
        && sb3_decoded.converged
        && sb4_decoded.converged
        && crc_result.frame_accepted) {
            result.accepted_frames += 1;
        }
    }
        std::cout << "\nSNR: " << snr_db << " dB\n";
        result.ber_result = static_cast<double>(result.bit_errors)/ static_cast<double>(result.total_bits);
        std::cout << "Total bits: " << result.total_bits << '\n';
        std::cout << "Bit errors: " << result.bit_errors << '\n';
        std::cout << "BER: " << result.ber_result << '\n';
        result.acceptance_rate = static_cast<double>(result.accepted_frames) / static_cast<double>(k_trials);
        std::cout << "Acceptance rate: " << result.acceptance_rate << '\n';
        std::cout << "Accepted frames: "
          << result.accepted_frames
          << "/" << k_trials << '\n';
        result.average_iterations = static_cast<double>(result.total_iterations)
        / static_cast<double>(k_trials * 3);
        std::cout << "Average iterations: "
          << result.average_iterations << '\n';
        std::cout << "Max iterations: "
          << result.max_iterations << '\n';
        std::cout << "Worst frame decode time: "
          << result.worst_frame_ms << " ms\n";
        results.push_back(result);
    }
    std::cout << "\n\n========== Gateway 7 SNR Summary ==========\n";

std::cout << std::left
          << std::setw(10) << "SNR(dB)"
          << std::setw(15) << "BER"
          << std::setw(15) << "Accepted"
          << std::setw(15) << "Rate(%)"
          << std::setw(15) << "AvgIter"
          << std::setw(12) << "MaxIter"
          << std::setw(15) << "WorstMs"
          << '\n';

std::cout << std::string(97, '-') << '\n';

for (const QualificationResult& result : results) {

    const std::string accepted =
        std::to_string(result.accepted_frames)
        + "/"
        + std::to_string(k_trials);

    std::cout << std::left
              << std::setw(10) << result.snr_db

              << std::scientific
              << std::setprecision(6)
              << std::setw(15) << result.ber_result

              << std::fixed
              << std::setw(15) << accepted

              << std::setprecision(2)
              << std::setw(15) << result.acceptance_rate * 100.0

              << std::setw(15) << result.average_iterations

              << std::setw(12) << result.max_iterations

              << std::setw(15) << result.worst_frame_ms

              << '\n';
}
    const std::filesystem::path output_dir =
    repo_root / "Validation" / "generated" / "gateway7";

std::filesystem::create_directories(output_dir);

const std::filesystem::path output_csv =
    output_dir / "gateway7_snr_results.csv";

std::ofstream csv_file(output_csv);

if (!csv_file.is_open()) {
    std::cerr << "FAIL: could not create CSV file: "
              << output_csv << '\n';
    return 1;
}
csv_file << std::setprecision(12);
csv_file
    << "snr_db,"
    << "total_bits,"
    << "bit_errors,"
    << "ber,"
    << "accepted_frames,"
    << "acceptance_rate,"
    << "average_iterations,"
    << "max_iterations,"
    << "worst_frame_ms\n";

for (const QualificationResult& result : results) {
    csv_file
        << result.snr_db << ','
        << result.total_bits << ','
        << result.bit_errors << ','
        << result.ber_result << ','
        << result.accepted_frames << ','
        << result.acceptance_rate << ','
        << result.average_iterations << ','
        << result.max_iterations << ','
        << result.worst_frame_ms
        << '\n';
}

csv_file.close();
std::cout << "\nCSV written to: "
          << output_csv << '\n';
    return 0;
}






