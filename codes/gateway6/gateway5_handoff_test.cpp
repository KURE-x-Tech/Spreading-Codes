#include "gateway3/frame_assembler.h"
#include "gateway5/frame_decoder.h"
#include "gateway6/subframe1_parser.h"
#include "gateway6/subframe2_parser.h"
#include "gateway6/subframe3_parser.h"
#include "gateway6/subframe4_parser.h"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace
{

    void AppendBits(std::vector<uint8_t> *bits, uint16_t value, int count)
    {
        for (int bit = count - 1; bit >= 0; --bit)
        {
            bits->push_back(static_cast<uint8_t>((value >> bit) & 1u));
        }
    }

    std::vector<uint8_t> BuildAlmanacPayload(uint8_t prn,
                                             uint16_t week,
                                             uint16_t itow,
                                             uint8_t health)
    {
        std::vector<uint8_t> payload;
        AppendBits(&payload, prn, 8);
        AppendBits(&payload, week, 13);
        AppendBits(&payload, itow, 9);
        AppendBits(&payload, health, 2);
        return payload;
    }

} // namespace

int main()
{
    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::filesystem::path csv_dir = repo_root / "Validation" / "annex3" / "csv";

    std::string error;
    lunanet::gateway5::DecoderMatrices decoder_matrices;
    if (!lunanet::gateway5::LoadDecoderMatrices(
            csv_dir.string(), &decoder_matrices, &error))
    {
        std::cerr << "FAIL [setup]: " << error << "\n";
        return 1;
    }

    lunanet::gateway3::FrameMatrices encoder_matrices;
    encoder_matrices.sb2 = decoder_matrices.sb2;
    encoder_matrices.sb34 = decoder_matrices.sb34;

    constexpr uint16_t kWeek = 1234;
    constexpr uint16_t kItow = 256;
    constexpr uint8_t kToi = 73;
    constexpr uint8_t kAlmanacPrn = 42;
    constexpr uint16_t kAlmanacWeek = 1222;
    constexpr uint16_t kAlmanacItow = 255;
    constexpr uint8_t kAlmanacHealth = 2;

    lunanet::gateway3::FrameInput input{};
    input.fid = 0;
    input.toi = kToi;
    input.sb2.wn = kWeek;
    input.sb2.itow = kItow;
    input.sb2.toi = kToi;
    input.sb3.type = lunanet::gateway6::kSb3TypeOrbitAlmanac;
    input.sb3.payload_bits = BuildAlmanacPayload(
        kAlmanacPrn, kAlmanacWeek, kAlmanacItow, kAlmanacHealth);
    input.sb4.type = 2;
    input.sb4.payload_bits = {1u, 0u, 1u, 1u, 0u};

    const auto frame = lunanet::gateway3::AssembleFrame(input, encoder_matrices, &error);
    if (frame.empty())
    {
        std::cerr << "FAIL [assemble]: " << error << "\n";
        return 1;
    }

    std::vector<double> symbols;
    symbols.reserve(frame.size());
    for (const uint8_t bit : frame)
    {
        symbols.push_back(bit == 0u ? +1.0 : -1.0);
    }

    lunanet::gateway5::FrameDecoderConfig config;
    config.prn = 1;
    const auto decoded = lunanet::gateway5::DecodeDespreadSymbols(
        symbols, decoder_matrices, config);
    if (!decoded.accepted)
    {
        std::cerr << "FAIL [Gateway 5]: " << decoded.error << "\n";
        return 1;
    }

    lunanet::gateway6::Subframe1Data sb1;
    if (!lunanet::gateway6::ParseSubframe1(static_cast<uint16_t>(decoded.sb1_value), &sb1, &error))
    {
        std::cerr << "FAIL [SB1 handoff]: " << error << "\n";
        return 1;
    }
    if (sb1.fid != input.fid || sb1.toi != input.toi)
    {
        std::cerr << "FAIL [SB1 fields]: FID or TOI changed at handoff (got FID="
                  << static_cast<int>(sb1.fid) << ", TOI=" << static_cast<int>(sb1.toi) << ")\n";
        return 1;
    }

    lunanet::gateway6::Subframe2Data sb2;
    if (!lunanet::gateway6::ParseSubframe2(decoded.sb2_payload, &sb2, &error))
    {
        std::cerr << "FAIL [SB2 handoff]: " << error << "\n";
        return 1;
    }
    if (sb2.wn != kWeek || sb2.itow != kItow || sb2.toi != kToi)
    {
        std::cerr << "FAIL [SB2 fields]: navigation time fields changed at handoff\n";
        return 1;
    }

    lunanet::gateway6::Subframe3Data sb3;
    if (!lunanet::gateway6::ParseSubframe3(decoded.sb3_payload, &sb3, &error))
    {
        std::cerr << "FAIL [SB3 handoff]: " << error << "\n";
        return 1;
    }
    const auto *almanac = std::get_if<lunanet::gateway6::OrbitAlmanacData>(&sb3.decoded);
    if (!almanac || almanac->prn != kAlmanacPrn ||
        almanac->reference_week != kAlmanacWeek ||
        almanac->reference_itow != kAlmanacItow ||
        almanac->health != kAlmanacHealth)
    {
        std::cerr << "FAIL [SB3 fields]: almanac changed at handoff\n";
        return 1;
    }

    lunanet::gateway6::Subframe4Data sb4;
    if (!lunanet::gateway6::ParseSubframe4(decoded.sb4_payload, &sb4, &error))
    {
        std::cerr << "FAIL [SB4 handoff]: " << error << "\n";
        return 1;
    }
    const auto expected_sb4 = lunanet::gateway3::PackSubframe4(input.sb4);
    const std::vector<uint8_t> expected_sb4_payload(
        expected_sb4.begin() + lunanet::gateway6::kSb4TypeFieldBits,
        expected_sb4.end());
    if (sb4.type != input.sb4.type || sb4.raw_payload != expected_sb4_payload)
    {
        std::cerr << "FAIL [SB4 fields]: type or payload changed at handoff\n";
        return 1;
    }

    std::cout << "PASS: Gateway 5 decoded SB1 payload parses in Gateway 6\n";
    std::cout << "PASS: Gateway 5 CRC-stripped SB2 payload parses in Gateway 6\n";
    std::cout << "PASS: Gateway 5 CRC-stripped SB3 almanac parses in Gateway 6\n";
    std::cout << "PASS: Gateway 5 CRC-stripped SB4 payload parses in Gateway 6\n";
    std::cout << "PASS: navigation fields survive encode/decode/parser handoff\n";
    return 0;
}