// Tests for ParseSubframe1 (Gateway 6 SB1 parser).
//
// Verifies FID (2 bits: 0..3) and TOI (7 bits: 0..99) parsing from both
// bit vectors and raw 9-bit words per LSIS-AFS §2.4.2.2 (Table 13 & Table 14).

#include "gateway6/subframe1_parser.h"
#include "gateway3/subframe1_builder.h"
#include "gateway2/bch_codec.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{

    // Pack 9 bits MSB-first: bits 0-1 = FID, bits 2-8 = TOI.
    std::vector<uint8_t> PackSb1Bits(uint8_t fid, uint8_t toi)
    {
        std::vector<uint8_t> bits(lunanet::gateway6::kSb1DataBits, 0u);
        bits[0] = (fid >> 1) & 1u;
        bits[1] = fid & 1u;
        for (int i = 0; i < 7; ++i)
        {
            bits[static_cast<size_t>(2 + i)] = (toi >> (6 - i)) & 1u;
        }
        return bits;
    }

    bool TestBitVectorRoundTrip(uint8_t fid, uint8_t toi)
    {
        const std::vector<uint8_t> bits = PackSb1Bits(fid, toi);
        lunanet::gateway6::Subframe1Data out;
        std::string error;

        if (!lunanet::gateway6::ParseSubframe1(bits, &out, &error))
        {
            std::cerr << "FAIL [bit-vector FID=" << static_cast<int>(fid)
                      << " TOI=" << static_cast<int>(toi)
                      << "]: parse error: " << error << "\n";
            return false;
        }

        if (out.fid != fid || out.toi != toi)
        {
            std::cerr << "FAIL [bit-vector FID=" << static_cast<int>(fid)
                      << " TOI=" << static_cast<int>(toi)
                      << "]: got FID=" << static_cast<int>(out.fid)
                      << " TOI=" << static_cast<int>(out.toi) << "\n";
            return false;
        }
        return true;
    }

    bool TestRawWordRoundTrip(uint8_t fid, uint8_t toi)
    {
        const uint16_t raw_word = static_cast<uint16_t>(((fid & 0x3u) << 7) | (toi & 0x7Fu));
        lunanet::gateway6::Subframe1Data out;
        std::string error;

        if (!lunanet::gateway6::ParseSubframe1(raw_word, &out, &error))
        {
            std::cerr << "FAIL [raw-word FID=" << static_cast<int>(fid)
                      << " TOI=" << static_cast<int>(toi)
                      << "]: parse error: " << error << "\n";
            return false;
        }

        if (out.fid != fid || out.toi != toi)
        {
            std::cerr << "FAIL [raw-word FID=" << static_cast<int>(fid)
                      << " TOI=" << static_cast<int>(toi)
                      << "]: got FID=" << static_cast<int>(out.fid)
                      << " TOI=" << static_cast<int>(out.toi) << "\n";
            return false;
        }
        return true;
    }

    bool TestEndToEndBchToSb1Parser(uint8_t fid, uint8_t toi)
    {
        // 1. Build 52-symbol BCH codeword using Gateway 3
        const auto symbols = lunanet::gateway3::BuildSubframe1(fid, toi);
        if (symbols.size() != 52u)
        {
            std::cerr << "FAIL [BCH-e2e]: BuildSubframe1 returned " << symbols.size() << " symbols\n";
            return false;
        }

        // 2. Decode via soft BCH decoder (Gateway 2)
        std::vector<double> soft(symbols.begin(), symbols.end());
        for (auto &s : soft)
        {
            s = (s == 0) ? +1.0 : -1.0;
        }
        const int decoded_val = lunanet::gateway2::BchDecodeSoft(soft);
        if (decoded_val < 0)
        {
            std::cerr << "FAIL [BCH-e2e]: BchDecodeSoft failed\n";
            return false;
        }

        // 3. Parse via Subframe1Parser (Gateway 6)
        lunanet::gateway6::Subframe1Data out;
        std::string error;
        if (!lunanet::gateway6::ParseSubframe1(static_cast<uint16_t>(decoded_val), &out, &error))
        {
            std::cerr << "FAIL [BCH-e2e]: ParseSubframe1 failed: " << error << "\n";
            return false;
        }

        if (out.fid != fid || out.toi != toi)
        {
            std::cerr << "FAIL [BCH-e2e]: recovered FID=" << static_cast<int>(out.fid)
                      << " TOI=" << static_cast<int>(out.toi)
                      << ", expected FID=" << static_cast<int>(fid)
                      << " TOI=" << static_cast<int>(toi) << "\n";
            return false;
        }
        return true;
    }

    bool TestRejectsInvalidToi()
    {
        for (uint8_t bad_toi : {100u, 101u, 115u, 127u})
        {
            // Bit vector test
            const auto bits = PackSb1Bits(0, bad_toi);
            lunanet::gateway6::Subframe1Data out;
            std::string error;
            if (lunanet::gateway6::ParseSubframe1(bits, &out, &error))
            {
                std::cerr << "FAIL [invalid-toi-bits]: accepted TOI=" << static_cast<int>(bad_toi) << "\n";
                return false;
            }
            if (error.empty())
            {
                std::cerr << "FAIL [invalid-toi-bits]: error message is empty for TOI="
                          << static_cast<int>(bad_toi) << "\n";
                return false;
            }

            // Raw word test
            const uint16_t raw_word = static_cast<uint16_t>(bad_toi);
            error.clear();
            if (lunanet::gateway6::ParseSubframe1(raw_word, &out, &error))
            {
                std::cerr << "FAIL [invalid-toi-word]: accepted TOI=" << static_cast<int>(bad_toi) << "\n";
                return false;
            }
            if (error.empty())
            {
                std::cerr << "FAIL [invalid-toi-word]: error message is empty for TOI="
                          << static_cast<int>(bad_toi) << "\n";
                return false;
            }
        }
        return true;
    }

    bool TestRejectsInvalidLength()
    {
        for (int bad_len : {0, 1, 8, 10, 52, 100})
        {
            std::vector<uint8_t> bits(static_cast<size_t>(bad_len), 0u);
            lunanet::gateway6::Subframe1Data out;
            std::string error;
            if (lunanet::gateway6::ParseSubframe1(bits, &out, &error))
            {
                std::cerr << "FAIL [bad-len]: accepted length " << bad_len << "\n";
                return false;
            }
            if (error.empty())
            {
                std::cerr << "FAIL [bad-len]: empty error message for length " << bad_len << "\n";
                return false;
            }
        }
        return true;
    }

    bool TestRejectsNonBinaryBits()
    {
        auto bits = PackSb1Bits(0, 42);
        bits[4] = 2;
        lunanet::gateway6::Subframe1Data out;
        std::string error;
        if (lunanet::gateway6::ParseSubframe1(bits, &out, &error))
        {
            std::cerr << "FAIL [non-binary]: accepted bit value 2\n";
            return false;
        }
        if (error.empty())
        {
            std::cerr << "FAIL [non-binary]: empty error message\n";
            return false;
        }
        return true;
    }

    bool TestRejectsInvalidRawWord()
    {
        for (uint16_t bad_word : {0x0200u, 0x0400u, 0xFFFFu})
        {
            lunanet::gateway6::Subframe1Data out;
            std::string error;
            if (lunanet::gateway6::ParseSubframe1(bad_word, &out, &error))
            {
                std::cerr << "FAIL [bad-raw-word]: accepted word 0x" << std::hex << bad_word << "\n";
                return false;
            }
            if (error.empty())
            {
                std::cerr << "FAIL [bad-raw-word]: empty error message for word 0x" << std::hex << bad_word << "\n";
                return false;
            }
        }
        return true;
    }

    bool TestRejectsNullOutput()
    {
        const auto bits = PackSb1Bits(0, 42);
        std::string error;
        if (lunanet::gateway6::ParseSubframe1(bits, nullptr, &error))
        {
            std::cerr << "FAIL [null-out-bits]: accepted null out_data\n";
            return false;
        }
        if (error.empty())
        {
            std::cerr << "FAIL [null-out-bits]: empty error message\n";
            return false;
        }

        error.clear();
        if (lunanet::gateway6::ParseSubframe1(static_cast<uint16_t>(42), nullptr, &error))
        {
            std::cerr << "FAIL [null-out-word]: accepted null out_data\n";
            return false;
        }
        if (error.empty())
        {
            std::cerr << "FAIL [null-out-word]: empty error message\n";
            return false;
        }
        return true;
    }

} // namespace

int main()
{
    int passed = 0;
    int failed = 0;

    auto run_check = [&](bool ok, const std::string &name)
    {
        if (ok)
        {
            std::cout << "PASS: " << name << "\n";
            ++passed;
        }
        else
        {
            std::cerr << "FAIL: " << name << "\n";
            ++failed;
        }
    };

    // Test all valid FID (0..3) and TOI (0..99) combinations
    bool all_roundtrips_ok = true;
    for (uint8_t fid = 0; fid <= 3; ++fid)
    {
        for (uint8_t toi = 0; toi <= 99; ++toi)
        {
            if (!TestBitVectorRoundTrip(fid, toi) || !TestRawWordRoundTrip(fid, toi))
            {
                all_roundtrips_ok = false;
            }
        }
    }
    run_check(all_roundtrips_ok, "All 400 valid (FID, TOI) bit-vector and raw-word round-trips");

    // Test representative end-to-end BCH encode -> decode -> SB1 parse
    bool bch_e2e_ok = true;
    for (uint8_t fid : {0u, 1u, 2u, 3u})
    {
        for (uint8_t toi : {0u, 1u, 42u, 50u, 73u, 99u})
        {
            if (!TestEndToEndBchToSb1Parser(fid, toi))
            {
                bch_e2e_ok = false;
            }
        }
    }
    run_check(bch_e2e_ok, "End-to-end Gateway 3 (BchEncode) -> Gateway 2 (BchDecodeSoft) -> Gateway 6 (ParseSubframe1)");

    // Negative tests
    run_check(TestRejectsInvalidToi(), "Rejects out-of-range TOI (> 99)");
    run_check(TestRejectsInvalidLength(), "Rejects invalid bit vector lengths");
    run_check(TestRejectsNonBinaryBits(), "Rejects non-binary bits in input vector");
    run_check(TestRejectsInvalidRawWord(), "Rejects raw words > 0x1FF (9 bits)");
    run_check(TestRejectsNullOutput(), "Rejects null out_data pointer");

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}