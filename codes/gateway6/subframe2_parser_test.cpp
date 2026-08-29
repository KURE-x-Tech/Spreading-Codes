// Tests for ParseSubframe2 (Gateway 6 SB2 parser).
//
// Synthetic bit vectors are built using the same packing logic as
// gateway3/subframe2_builder (PackSubframe2) to guarantee round-trip fidelity.

#include "gateway6/subframe2_parser.h"
#include "gateway3/subframe2_builder.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{

    // ---------------------------------------------------------------------------
    // Helper: build a 1176-bit vector via gateway3::PackSubframe2 and verify that
    // ParseSubframe2 recovers the original WN and ITOW values.
    // ---------------------------------------------------------------------------
    bool TestRoundTrip(uint16_t wn, uint16_t itow, const std::string &label)
    {
        lunanet::gateway3::Subframe2Data builder_in;
        builder_in.wn = wn;
        builder_in.itow = itow;
        builder_in.toi = 0;
        // payload_bits left empty; PackSubframe2 fills remainder with spare bits.

        const std::vector<uint8_t> packed = lunanet::gateway3::PackSubframe2(builder_in);

        if (static_cast<int>(packed.size()) != lunanet::gateway6::kSb2DataBits)
        {
            std::cerr << "FAIL [" << label << "]: PackSubframe2 returned "
                      << packed.size() << " bits, expected "
                      << lunanet::gateway6::kSb2DataBits << "\n";
            return false;
        }

        lunanet::gateway6::Subframe2Data parsed;
        std::string error;
        if (!lunanet::gateway6::ParseSubframe2(packed, &parsed, &error))
        {
            std::cerr << "FAIL [" << label << "]: ParseSubframe2 error: " << error << "\n";
            return false;
        }

        if (parsed.wn != wn)
        {
            std::cerr << "FAIL [" << label << "]: WN mismatch: got " << parsed.wn
                      << ", want " << wn << "\n";
            return false;
        }

        if (parsed.itow != itow)
        {
            std::cerr << "FAIL [" << label << "]: ITOW mismatch: got " << parsed.itow
                      << ", want " << itow << "\n";
            return false;
        }

        std::cout << "PASS [" << label << "]: WN=" << wn << " ITOW=" << itow << "\n";
        return true;
    }

    bool TestTimeOfTransmission(uint16_t wn, uint16_t itow, uint8_t toi,
                                const std::string &label)
    {
        lunanet::gateway3::Subframe2Data builder_in;
        builder_in.wn = wn;
        builder_in.itow = itow;
        builder_in.toi = toi;

        const std::vector<uint8_t> packed = lunanet::gateway3::PackSubframe2(builder_in);
        lunanet::gateway6::Subframe2Data parsed;
        std::string error;
        if (!lunanet::gateway6::ParseSubframe2(packed, &parsed, &error))
        {
            std::cerr << "FAIL [" << label << "]: ParseSubframe2 error: " << error << "\n";
            return false;
        }

        const uint64_t expected = static_cast<uint64_t>(wn) * lunanet::gateway6::kSecondsPerWeek +
                                  static_cast<uint64_t>(itow) * lunanet::gateway6::kSecondsPerItow +
                                  static_cast<uint64_t>(toi) * lunanet::gateway6::kSecondsPerToi;
        if (parsed.time_of_transmission_seconds != expected)
        {
            std::cerr << "FAIL [" << label << "]: ToT mismatch: got "
                      << parsed.time_of_transmission_seconds << ", want " << expected << "\n";
            return false;
        }

        if (lunanet::gateway6::ComputeTimeOfTransmissionSeconds(wn, itow, toi) != expected)
        {
            std::cerr << "FAIL [" << label << "]: helper ToT mismatch\n";
            return false;
        }

        std::cout << "PASS [" << label << "]: ToT=" << expected << " seconds\n";
        return true;
    }

    // ---------------------------------------------------------------------------
    // Helper: verify that a wrong-length input is rejected with an error.
    // ---------------------------------------------------------------------------
    bool TestRejectsWrongLength(int bad_length)
    {
        std::vector<uint8_t> bad(static_cast<size_t>(bad_length), 0u);
        lunanet::gateway6::Subframe2Data out;
        std::string error;
        if (lunanet::gateway6::ParseSubframe2(bad, &out, &error))
        {
            std::cerr << "FAIL [wrong-length " << bad_length
                      << "]: expected rejection but ParseSubframe2 returned true\n";
            return false;
        }
        if (error.empty())
        {
            std::cerr << "FAIL [wrong-length " << bad_length
                      << "]: rejection returned empty error message\n";
            return false;
        }
        std::cout << "PASS [wrong-length " << bad_length << "]: rejected with \""
                  << error << "\"\n";
        return true;
    }

    // ---------------------------------------------------------------------------
    // Helper: verify that a null output pointer is rejected.
    // ---------------------------------------------------------------------------
    bool TestRejectsNullOutput()
    {
        std::vector<uint8_t> bits(lunanet::gateway6::kSb2DataBits, 0u);
        std::string error;
        if (lunanet::gateway6::ParseSubframe2(bits, nullptr, &error))
        {
            std::cerr << "FAIL [null-output]: expected rejection but returned true\n";
            return false;
        }
        std::cout << "PASS [null-output]: rejected with \"" << error << "\"\n";
        return true;
    }

    // ---------------------------------------------------------------------------
    // Helper: verify that a TOI value round-trips correctly.
    // Packs TOI into bits 22-28 (7 bits, MSB first) of an otherwise-zero vector.
    // ---------------------------------------------------------------------------
    bool TestToi(uint8_t toi_val, const std::string &label)
    {
        std::vector<uint8_t> bits(lunanet::gateway6::kSb2DataBits, 0u);
        for (int i = 0; i < 7; ++i)
        {
            bits[static_cast<size_t>(22 + i)] = (toi_val >> (6 - i)) & 1u;
        }

        lunanet::gateway6::Subframe2Data parsed;
        std::string error;
        if (!lunanet::gateway6::ParseSubframe2(bits, &parsed, &error))
        {
            std::cerr << "FAIL [" << label << "]: ParseSubframe2 error: " << error << "\n";
            return false;
        }
        if (parsed.toi != toi_val)
        {
            std::cerr << "FAIL [" << label << "]: TOI mismatch: got "
                      << static_cast<int>(parsed.toi)
                      << ", want " << static_cast<int>(toi_val) << "\n";
            return false;
        }
        std::cout << "PASS [" << label << "]: toi=" << static_cast<int>(toi_val) << "\n";
        return true;
    }

    // ---------------------------------------------------------------------------
    // Helper: verify that TOI values outside the Table 22 range are rejected.
    // ---------------------------------------------------------------------------
    bool TestToiOutOfRange(uint8_t toi_val, const std::string &label)
    {
        std::vector<uint8_t> bits(lunanet::gateway6::kSb2DataBits, 0u);
        for (int i = 0; i < 7; ++i)
        {
            bits[static_cast<size_t>(22 + i)] = (toi_val >> (6 - i)) & 1u;
        }

        lunanet::gateway6::Subframe2Data parsed;
        std::string error;
        if (lunanet::gateway6::ParseSubframe2(bits, &parsed, &error))
        {
            std::cerr << "FAIL [" << label << "]: expected rejection for TOI="
                      << static_cast<int>(toi_val) << " but ParseSubframe2 returned true\n";
            return false;
        }
        std::cout << "PASS [" << label << "]: rejected TOI=" << static_cast<int>(toi_val)
                  << " with \"" << error << "\"\n";
        return true;
    }

    // ---------------------------------------------------------------------------
    // Helper: verify that af0 and af1 CED fields are correctly extracted.
    // Bit layout: af0 at bits 29-60 (32-bit signed), af1 at bits 61-76 (16-bit signed).
    // Scale factors: af0 * 2^-31 s, af1 * 2^-43 s/s.
    // ---------------------------------------------------------------------------
    bool TestCedFields(int32_t af0_raw, int16_t af1_raw, const std::string &label)
    {
        std::vector<uint8_t> bits(lunanet::gateway6::kSb2DataBits, 0u);
        // Pack 32-bit af0 into bits 29-60 (MSB first).
        for (int i = 0; i < 32; ++i)
        {
            bits[static_cast<size_t>(29 + i)] =
                (static_cast<uint32_t>(af0_raw) >> (31 - i)) & 1u;
        }
        // Pack 16-bit af1 into bits 61-76 (MSB first).
        for (int i = 0; i < 16; ++i)
        {
            bits[static_cast<size_t>(61 + i)] =
                (static_cast<uint16_t>(af1_raw) >> (15 - i)) & 1u;
        }

        lunanet::gateway6::Subframe2Data parsed;
        std::string error;
        if (!lunanet::gateway6::ParseSubframe2(bits, &parsed, &error))
        {
            std::cerr << "FAIL [" << label << "]: ParseSubframe2 error: " << error << "\n";
            return false;
        }

        constexpr double kAf0Scale = 4.6566128730773926e-10; // 2^-31
        constexpr double kAf1Scale = 1.1368683772161603e-13; // 2^-43
        const double expected_af0 = static_cast<double>(af0_raw) * kAf0Scale;
        const double expected_af1 = static_cast<double>(af1_raw) * kAf1Scale;

        // Use exact comparison: the computation is deterministic integer->double scaling.
        if (parsed.ced.af0 != expected_af0)
        {
            std::cerr << "FAIL [" << label << "]: af0 mismatch: got " << parsed.ced.af0
                      << ", want " << expected_af0 << "\n";
            return false;
        }
        if (parsed.ced.af1 != expected_af1)
        {
            std::cerr << "FAIL [" << label << "]: af1 mismatch: got " << parsed.ced.af1
                      << ", want " << expected_af1 << "\n";
            return false;
        }
        std::cout << "PASS [" << label << "]: af0=" << parsed.ced.af0
                  << " af1=" << parsed.ced.af1 << "\n";
        return true;
    }

    // ---------------------------------------------------------------------------
    // Helper: verify that health status bits are correctly round-tripped.
    // We manually pack a 1176-bit vector with a known health value at bits 77-78.
    // ---------------------------------------------------------------------------
    bool TestHealthBits(uint8_t health_val, const std::string &label)
    {
        std::vector<uint8_t> bits(lunanet::gateway6::kSb2DataBits, 0u);
        // Pack health at bits 77-78 (2 bits, MSB first).
        bits[77] = (health_val >> 1u) & 1u;
        bits[78] = health_val & 1u;

        lunanet::gateway6::Subframe2Data parsed;
        std::string error;
        if (!lunanet::gateway6::ParseSubframe2(bits, &parsed, &error))
        {
            std::cerr << "FAIL [" << label << "]: ParseSubframe2 error: " << error << "\n";
            return false;
        }
        if (parsed.health.status != health_val)
        {
            std::cerr << "FAIL [" << label << "]: health mismatch: got "
                      << static_cast<int>(parsed.health.status)
                      << ", want " << static_cast<int>(health_val) << "\n";
            return false;
        }
        std::cout << "PASS [" << label << "]: health.status=" << static_cast<int>(health_val) << "\n";
        return true;
    }

} // namespace

int main()
{
    bool ok = true;

    // Nominal round-trip: interop-doc example values (WN=1234, ITOW=256).
    ok &= TestRoundTrip(1234, 256, "nominal WN=1234 ITOW=256");

    // Boundary: WN=8191 (13-bit max), ITOW=503 (spec max per Table 22).
    ok &= TestRoundTrip(8191, 503, "boundary-max WN=8191 ITOW=503");

    // Boundary: WN=0, ITOW=0 (minimum values).
    ok &= TestRoundTrip(0, 0, "boundary-min WN=0 ITOW=0");

    // Wrong-length inputs must be rejected.
    ok &= TestRejectsWrongLength(0);
    ok &= TestRejectsWrongLength(1175);
    ok &= TestRejectsWrongLength(1177);
    ok &= TestRejectsWrongLength(1200); // includes CRC bytes -- must still fail

    // Null output pointer.
    ok &= TestRejectsNullOutput();

    // Health status values (0–3).
    ok &= TestHealthBits(0, "health-healthy");
    ok &= TestHealthBits(1, "health-marginal");
    ok &= TestHealthBits(2, "health-unhealthy");
    ok &= TestHealthBits(3, "health-do-not-use");

    // TOI round-trip: boundary values within Table 22 range (0–99).
    ok &= TestToi(0, "toi-min");
    ok &= TestToi(50, "toi-mid");
    ok &= TestToi(99, "toi-max");

    // Relative Time of Transmission: WN*604800 + ITOW*1200 + TOI*12.
    ok &= TestTimeOfTransmission(0, 0, 0, "tot-zero");
    ok &= TestTimeOfTransmission(1234, 256, 73, "tot-nominal");
    ok &= TestTimeOfTransmission(8191, 503, 99, "tot-boundary-max");

    // TOI out-of-range values must be rejected.
    ok &= TestToiOutOfRange(100, "toi-out-of-range-100");
    ok &= TestToiOutOfRange(127, "toi-out-of-range-127");

    // CED fields: zero, positive, and negative raw values.
    ok &= TestCedFields(0, 0, "ced-zero");
    ok &= TestCedFields(1, 1, "ced-positive-lsb");
    ok &= TestCedFields(2147483647, 32767, "ced-max-positive");
    ok &= TestCedFields(-1, -1, "ced-all-ones");
    ok &= TestCedFields(static_cast<int32_t>(0x80000000), -32768, "ced-min-negative");

    return ok ? 0 : 1;
}
