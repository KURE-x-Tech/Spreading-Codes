// Tests for ParseSubframe2 (Gateway 6 SB2 parser).
//
// Synthetic bit vectors are built using the same packing logic as
// gateway3/subframe2_builder (PackSubframe2) to guarantee round-trip fidelity.

#include "gateway6/subframe2_parser.h"
#include "gateway3/subframe2_builder.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Helper: build a 1176-bit vector via gateway3::PackSubframe2 and verify that
// ParseSubframe2 recovers the original WN and ITOW values.
// ---------------------------------------------------------------------------
bool TestRoundTrip(uint16_t wn, uint16_t itow, const std::string& label) {
    lunanet::gateway3::Subframe2Data builder_in;
    builder_in.wn   = wn;
    builder_in.itow = itow;
    builder_in.toi  = 0;
    // payload_bits left empty; PackSubframe2 fills remainder with spare bits.

    const std::vector<uint8_t> packed = lunanet::gateway3::PackSubframe2(builder_in);

    if (static_cast<int>(packed.size()) != lunanet::gateway6::kSb2DataBits) {
        std::cerr << "FAIL [" << label << "]: PackSubframe2 returned "
                  << packed.size() << " bits, expected "
                  << lunanet::gateway6::kSb2DataBits << "\n";
        return false;
    }

    lunanet::gateway6::Subframe2Data parsed;
    std::string error;
    if (!lunanet::gateway6::ParseSubframe2(packed, &parsed, &error)) {
        std::cerr << "FAIL [" << label << "]: ParseSubframe2 error: " << error << "\n";
        return false;
    }

    if (parsed.wn != wn) {
        std::cerr << "FAIL [" << label << "]: WN mismatch: got " << parsed.wn
                  << ", want " << wn << "\n";
        return false;
    }

    if (parsed.itow != itow) {
        std::cerr << "FAIL [" << label << "]: ITOW mismatch: got " << parsed.itow
                  << ", want " << itow << "\n";
        return false;
    }

    std::cout << "PASS [" << label << "]: WN=" << wn << " ITOW=" << itow << "\n";
    return true;
}

// ---------------------------------------------------------------------------
// Helper: verify that a wrong-length input is rejected with an error.
// ---------------------------------------------------------------------------
bool TestRejectsWrongLength(int bad_length) {
    std::vector<uint8_t> bad(static_cast<size_t>(bad_length), 0u);
    lunanet::gateway6::Subframe2Data out;
    std::string error;
    if (lunanet::gateway6::ParseSubframe2(bad, &out, &error)) {
        std::cerr << "FAIL [wrong-length " << bad_length
                  << "]: expected rejection but ParseSubframe2 returned true\n";
        return false;
    }
    if (error.empty()) {
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
bool TestRejectsNullOutput() {
    std::vector<uint8_t> bits(lunanet::gateway6::kSb2DataBits, 0u);
    std::string error;
    if (lunanet::gateway6::ParseSubframe2(bits, nullptr, &error)) {
        std::cerr << "FAIL [null-output]: expected rejection but returned true\n";
        return false;
    }
    std::cout << "PASS [null-output]: rejected with \"" << error << "\"\n";
    return true;
}

// ---------------------------------------------------------------------------
// Helper: verify that health status bits are correctly round-tripped.
// We manually pack a 1176-bit vector with a known health value at bits 77-78.
// ---------------------------------------------------------------------------
bool TestHealthBits(uint8_t health_val, const std::string& label) {
    std::vector<uint8_t> bits(lunanet::gateway6::kSb2DataBits, 0u);
    // Pack health at bits 77-78 (2 bits, MSB first).
    bits[77] = (health_val >> 1u) & 1u;
    bits[78] = health_val & 1u;

    lunanet::gateway6::Subframe2Data parsed;
    std::string error;
    if (!lunanet::gateway6::ParseSubframe2(bits, &parsed, &error)) {
        std::cerr << "FAIL [" << label << "]: ParseSubframe2 error: " << error << "\n";
        return false;
    }
    if (parsed.health.status != health_val) {
        std::cerr << "FAIL [" << label << "]: health mismatch: got "
                  << static_cast<int>(parsed.health.status)
                  << ", want " << static_cast<int>(health_val) << "\n";
        return false;
    }
    std::cout << "PASS [" << label << "]: health.status=" << static_cast<int>(health_val) << "\n";
    return true;
}

}  // namespace

int main() {
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
    ok &= TestRejectsWrongLength(1200);  // includes CRC bytes -- must still fail

    // Null output pointer.
    ok &= TestRejectsNullOutput();

    // Health status values (0–3).
    ok &= TestHealthBits(0, "health-healthy");
    ok &= TestHealthBits(1, "health-marginal");
    ok &= TestHealthBits(2, "health-unhealthy");
    ok &= TestHealthBits(3, "health-do-not-use");

    return ok ? 0 : 1;
}
