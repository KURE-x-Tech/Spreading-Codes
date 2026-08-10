#include "gateway1/gold_code_generator.h"
#include "gateway1/spreading_config.h"
#include "gateway3/frame_assembler.h"
#include "gateway3/subframe2_builder.h"
#include "gateway3/subframe3_builder.h"
#include "gateway3/subframe4_builder.h"
#include "gateway4/bpsk_modulator.h"
#include "gateway4/signal_config.h"
#include "gateway5/frame_decoder.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> AlternatingBits(std::size_t count, uint8_t first) {
    std::vector<uint8_t> bits(count, 0u);
    for (std::size_t i = 0; i < count; ++i) {
        bits[i] = static_cast<uint8_t>((i + first) & 1u);
    }
    return bits;
}

bool AssertDecodedFrame(const lunanet::gateway5::FrameDecodeResult& decoded,
                        const lunanet::gateway3::FrameInput& input) {
    if (!decoded.accepted) {
        std::cerr << "FAIL [decode]: " << decoded.error << "\n";
        return false;
    }
    if (decoded.fid != input.fid || decoded.toi != input.toi) {
        std::cerr << "FAIL [SB1]: decoded FID/TOI mismatch\n";
        return false;
    }
    if (!decoded.sb2_ldpc.converged ||
        !decoded.sb3_ldpc.converged ||
        !decoded.sb4_ldpc.converged) {
        std::cerr << "FAIL [LDPC]: one or more subframes did not converge\n";
        return false;
    }
    if (!decoded.crc.frame_accepted) {
        std::cerr << "FAIL [CRC]: frame gate rejected a valid frame\n";
        return false;
    }

    const auto expected_sb2 = lunanet::gateway3::PackSubframe2(input.sb2);
    const auto expected_sb3 = lunanet::gateway3::PackSubframe3(input.sb3);
    const auto expected_sb4 = lunanet::gateway3::PackSubframe4(input.sb4);
    if (decoded.sb2_payload != expected_sb2 ||
        decoded.sb3_payload != expected_sb3 ||
        decoded.sb4_payload != expected_sb4) {
        std::cerr << "FAIL [payload]: CRC-stripped payload does not match encoded input\n";
        return false;
    }
    if (decoded.elapsed_ms >= 1000.0) {
        std::cerr << "FAIL [performance]: frame decode took " << decoded.elapsed_ms
                  << " ms (target < 1000 ms)\n";
        return false;
    }

    std::cout << "  decode time: " << decoded.elapsed_ms << " ms\n";
    std::cout << "  LDPC iterations: SB2=" << decoded.sb2_ldpc.iterations
              << " SB3=" << decoded.sb3_ldpc.iterations
              << " SB4=" << decoded.sb4_ldpc.iterations << "\n";
    return true;
}

std::vector<double> BuildNoisyOffsetSymbolStream(const std::vector<uint8_t>& frame,
                                                 std::size_t frame_offset,
                                                 double snr_db) {
    std::mt19937 rng(0xDEC0DEu);
    std::uniform_int_distribution<int> random_bit(0, 1);
    std::normal_distribution<double> gaussian(0.0, 1.0);
    const double noise_stddev = std::pow(10.0, -snr_db / 20.0);

    std::vector<double> symbols(frame_offset + frame.size() + 200u, 0.0);
    for (double& symbol : symbols) {
        const double background = random_bit(rng) == 0 ? +1.0 : -1.0;
        symbol = background + gaussian(rng) * noise_stddev;
    }
    for (std::size_t i = 0; i < frame.size(); ++i) {
        const double transmitted = frame[i] == 0u ? +1.0 : -1.0;
        symbols[frame_offset + i] = transmitted + gaussian(rng) * noise_stddev;
    }
    return symbols;
}

}  // namespace

int main() {
    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::filesystem::path csv_dir = repo_root / "Validation" / "annex3" / "csv";
    const std::filesystem::path config_path =
        repo_root / "config" / "spreading_codes_config.ini";

    std::string error;
    lunanet::gateway5::DecoderMatrices decoder_matrices;
    if (!lunanet::gateway5::LoadDecoderMatrices(
            csv_dir.string(), &decoder_matrices, &error)) {
        std::cerr << "FAIL [setup]: decoder matrices: " << error << "\n";
        return 1;
    }

    lunanet::gateway1::SpreadingSpecTables spreading_tables;
    lunanet::gateway1::Annex3Paths annex3_paths;
    if (!lunanet::gateway1::LoadSpreadingConfig(
            config_path.string(), &spreading_tables, &annex3_paths, &error)) {
        std::cerr << "FAIL [setup]: spreading config: " << error << "\n";
        return 1;
    }

    lunanet::gateway3::FrameMatrices encoder_matrices;
    encoder_matrices.sb2 = decoder_matrices.sb2;
    encoder_matrices.sb34 = decoder_matrices.sb34;

    lunanet::gateway3::FrameInput input{};
    input.fid = 2;
    input.toi = 73;
    input.sb2.wn = 1234;
    input.sb2.itow = 256;
    input.sb2.toi = input.toi;
    input.sb2.payload_bits = AlternatingBits(192, 1u);
    input.sb3.type = 1;
    input.sb3.payload_bits = AlternatingBits(160, 0u);
    input.sb4.type = 2;
    input.sb4.payload_bits = AlternatingBits(160, 1u);

    const auto frame = lunanet::gateway3::AssembleFrame(input, encoder_matrices, &error);
    if (frame.size() != static_cast<std::size_t>(lunanet::gateway3::kFrameSymbols)) {
        std::cerr << "FAIL [setup]: frame assembly: " << error << "\n";
        return 1;
    }

    constexpr int kPrn = 7;
    const auto primary_code =
        lunanet::gateway1::GenerateGoldCode(kPrn, spreading_tables, &error);
    const auto logic_chips =
        lunanet::gateway4::ModulateAfsIData(primary_code, frame, &error);
    if (logic_chips.empty()) {
        std::cerr << "FAIL [setup]: AFS-I modulation: " << error << "\n";
        return 1;
    }

    lunanet::gateway4::IqSignal signal;
    signal.sample_rate_hz = lunanet::gateway4::kAfsIChipRateHz;
    constexpr std::size_t kChipPhase = 777;
    signal.i.reserve(kChipPhase + logic_chips.size());
    std::mt19937 chip_rng(0xC01DF00Du);
    std::uniform_int_distribution<int> random_bit(0, 1);
    for (std::size_t i = 0; i < kChipPhase; ++i) {
        signal.i.push_back(random_bit(chip_rng) == 0 ? +1.0f : -1.0f);
    }
    for (const uint8_t chip : logic_chips) {
        signal.i.push_back(lunanet::gateway4::BpskMap(chip));
    }

    lunanet::gateway5::FrameDecoderConfig decoder_config;
    decoder_config.prn = kPrn;
    decoder_config.symbol_noise_variance = 1.0;

    lunanet::gateway4::IqSignal invalid_signal;
    invalid_signal.sample_rate_hz = lunanet::gateway4::kAfsIChipRateHz;
    invalid_signal.i = {1.0f};
    invalid_signal.q = {std::numeric_limits<float>::quiet_NaN()};
    const auto invalid_result = lunanet::gateway5::DecodeAfsIIqSignal(
        invalid_signal, spreading_tables, decoder_matrices, decoder_config);
    if (invalid_result.accepted || invalid_result.error.empty()) {
        std::cerr << "FAIL [I/Q validation]: non-finite Q sample was accepted\n";
        return 1;
    }

    const auto decoded = lunanet::gateway5::DecodeAfsIIqSignal(
        signal, spreading_tables, decoder_matrices, decoder_config);
    if (!AssertDecodedFrame(decoded, input)) {
        return 1;
    }
    if (decoded.code_phase != kChipPhase || decoded.sync.frame_offset != 0u) {
        std::cerr << "FAIL [chip alignment]: expected code phase " << kChipPhase << "\n";
        return 1;
    }

    constexpr std::size_t kFrameOffset = 137;
    constexpr double kSymbolSnrDb = 3.0;
    const auto noisy_symbols =
        BuildNoisyOffsetSymbolStream(frame, kFrameOffset, kSymbolSnrDb);
    decoder_config.symbol_noise_variance = std::pow(10.0, -kSymbolSnrDb / 10.0);
    const auto noisy_decoded = lunanet::gateway5::DecodeDespreadSymbols(
        noisy_symbols, decoder_matrices, decoder_config);
    if (!AssertDecodedFrame(noisy_decoded, input)) {
        return 1;
    }
    if (noisy_decoded.sync.frame_offset != kFrameOffset) {
        std::cerr << "FAIL [frame alignment]: expected frame offset " << kFrameOffset
                  << ", got " << noisy_decoded.sync.frame_offset << "\n";
        return 1;
    }

    std::cout << "PASS: complete AFS-I frame decodes at a nonzero chip phase\n";
    std::cout << "PASS: noisy 3 dB frame decodes at a nonzero symbol offset\n";
    std::cout << "PASS: SB1, LDPC, CRC, and Gateway 6 payload contracts match\n";
    std::cout << "PASS: full frame decode completes in under one second\n";
    return 0;
}