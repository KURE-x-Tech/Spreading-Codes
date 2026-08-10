// Tests for ParseSubframe3 (Gateway 6 SB3 router).

#include "gateway6/subframe3_parser.h"
#include "gateway3/subframe3_builder.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace {

void AppendBits(std::vector<uint8_t>* bits, uint16_t value, int count) {
    for (int i = count - 1; i >= 0; --i) {
        bits->push_back(static_cast<uint8_t>((value >> i) & 1u));
    }
}

std::vector<uint8_t> MakeAlmanacPayload(uint8_t prn,
                                        uint16_t reference_week,
                                        uint16_t reference_itow,
                                        uint8_t health) {
    std::vector<uint8_t> payload;
    payload.reserve(lunanet::gateway6::kSb3PayloadBits);
    AppendBits(&payload, prn, 8);
    AppendBits(&payload, reference_week, 13);
    AppendBits(&payload, reference_itow, 9);
    AppendBits(&payload, health, 2);

    while (static_cast<int>(payload.size()) < lunanet::gateway6::kSb3PayloadBits) {
        payload.push_back(static_cast<uint8_t>(payload.size() & 1u));
    }
    return payload;
}

std::vector<uint8_t> PackSubframe3(uint8_t type,
                                   const std::vector<uint8_t>& payload) {
    lunanet::gateway3::Subframe3Data builder_data;
    builder_data.type = type;
    builder_data.payload_bits = payload;
    return lunanet::gateway3::PackSubframe3(builder_data);
}

lunanet::gateway6::Subframe3Data MakeSentinelOutput() {
    lunanet::gateway6::Subframe3Data sentinel;
    sentinel.type = 9;
    sentinel.raw_payload = {1u, 0u, 1u};
    lunanet::gateway6::OrbitAlmanacData almanac;
    almanac.prn = 7;
    almanac.reference_week = 8;
    almanac.reference_itow = 9;
    almanac.health = 2;
    sentinel.decoded = almanac;
    return sentinel;
}

bool IsSentinelOutput(const lunanet::gateway6::Subframe3Data& output) {
    const auto* almanac =
        std::get_if<lunanet::gateway6::OrbitAlmanacData>(&output.decoded);
    return output.type == 9 && output.raw_payload == std::vector<uint8_t>({1u, 0u, 1u}) &&
           almanac && almanac->prn == 7 && almanac->reference_week == 8 &&
           almanac->reference_itow == 9 && almanac->health == 2;
}

bool TestAlmanacRoundTrip(uint8_t prn,
                          uint16_t reference_week,
                          uint16_t reference_itow,
                          uint8_t health,
                          const std::string& label) {
    const auto payload =
        MakeAlmanacPayload(prn, reference_week, reference_itow, health);
    const auto packed =
        PackSubframe3(lunanet::gateway6::kSb3TypeOrbitAlmanac, payload);

    if (static_cast<int>(packed.size()) != lunanet::gateway6::kSb3DataBits) {
        std::cerr << "FAIL [" << label << "]: packer returned " << packed.size()
                  << " bits\n";
        return false;
    }

    lunanet::gateway6::Subframe3Data parsed;
    std::string error = "stale error";
    if (!lunanet::gateway6::ParseSubframe3(packed, &parsed, &error)) {
        std::cerr << "FAIL [" << label << "]: " << error << "\n";
        return false;
    }

    const auto* almanac =
        std::get_if<lunanet::gateway6::OrbitAlmanacData>(&parsed.decoded);
    if (parsed.type != lunanet::gateway6::kSb3TypeOrbitAlmanac ||
        parsed.raw_payload != payload || !almanac || almanac->prn != prn ||
        almanac->reference_week != reference_week ||
        almanac->reference_itow != reference_itow || almanac->health != health ||
        !error.empty()) {
        std::cerr << "FAIL [" << label << "]: decoded Almanac mismatch\n";
        return false;
    }

    std::cout << "PASS [" << label << "]: PRN=" << static_cast<int>(prn)
              << " week=" << reference_week << " ITOW=" << reference_itow
              << " health=" << static_cast<int>(health) << "\n";
    return true;
}

bool TestUnknownTypePassThrough() {
    constexpr uint8_t kUnknownType =
        static_cast<uint8_t>((1u << lunanet::gateway6::kSb3TypeFieldBits) - 1u);
    const auto payload = MakeAlmanacPayload(255, 8191, 511, 3);
    std::vector<uint8_t> packed(lunanet::gateway6::kSb3DataBits, 0u);
    for (int i = 0; i < lunanet::gateway6::kSb3TypeFieldBits; ++i) {
        packed[static_cast<std::size_t>(i)] = static_cast<uint8_t>(
            (kUnknownType >> (lunanet::gateway6::kSb3TypeFieldBits - 1 - i)) & 1u);
    }
    for (std::size_t i = 0; i < payload.size(); ++i) {
        packed[static_cast<std::size_t>(lunanet::gateway6::kSb3TypeFieldBits) + i] =
            payload[i];
    }

    lunanet::gateway6::Subframe3Data parsed;
    std::string error;
    if (!lunanet::gateway6::ParseSubframe3(packed, &parsed, &error)) {
        std::cerr << "FAIL [unknown-type]: " << error << "\n";
        return false;
    }

    if (parsed.type != kUnknownType || parsed.raw_payload != payload ||
        !std::holds_alternative<std::monostate>(parsed.decoded)) {
        std::cerr << "FAIL [unknown-type]: raw pass-through mismatch\n";
        return false;
    }

    std::cout << "PASS [unknown-type]: preserved type and raw payload\n";
    return true;
}

bool ExpectFailure(const std::vector<uint8_t>& bits, const std::string& label) {
    auto output = MakeSentinelOutput();
    std::string error;
    if (lunanet::gateway6::ParseSubframe3(bits, &output, &error)) {
        std::cerr << "FAIL [" << label << "]: expected parser rejection\n";
        return false;
    }
    if (error.empty() || !IsSentinelOutput(output)) {
        std::cerr << "FAIL [" << label
                  << "]: missing error or output mutated on failure\n";
        return false;
    }

    std::cout << "PASS [" << label << "]: rejected with \"" << error << "\"\n";
    return true;
}

bool TestWrongLengths() {
    bool ok = true;
    for (const int length : {0, 845, 847, 870}) {
        ok &= ExpectFailure(std::vector<uint8_t>(static_cast<std::size_t>(length), 0u),
                            "wrong-length-" + std::to_string(length));
    }
    return ok;
}

bool TestNullOutput() {
    std::vector<uint8_t> bits(lunanet::gateway6::kSb3DataBits, 0u);
    std::string error;
    if (lunanet::gateway6::ParseSubframe3(bits, nullptr, &error) || error.empty()) {
        std::cerr << "FAIL [null-output]: expected rejection with an error\n";
        return false;
    }
    std::cout << "PASS [null-output]: rejected with \"" << error << "\"\n";
    return true;
}

bool TestNonBinaryInput() {
    std::vector<uint8_t> bits(lunanet::gateway6::kSb3DataBits, 0u);
    bits[100] = 2u;
    return ExpectFailure(bits, "non-binary-input");
}

bool TestInvalidAlmanacFields() {
    bool ok = true;
    ok &= ExpectFailure(
        PackSubframe3(lunanet::gateway6::kSb3TypeOrbitAlmanac,
                      MakeAlmanacPayload(211, 0, 0, 0)),
        "almanac-prn-211");
    ok &= ExpectFailure(
        PackSubframe3(lunanet::gateway6::kSb3TypeOrbitAlmanac,
                      MakeAlmanacPayload(1, 0, 504, 0)),
        "almanac-itow-504");
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= TestAlmanacRoundTrip(42, 1234, 256, 1, "almanac-nominal");
    ok &= TestAlmanacRoundTrip(0, 0, 0, 0, "almanac-boundary-min");
    ok &= TestAlmanacRoundTrip(210, 8191, 503, 3, "almanac-boundary-max");
    ok &= TestUnknownTypePassThrough();
    ok &= TestWrongLengths();
    ok &= TestNullOutput();
    ok &= TestNonBinaryInput();
    ok &= TestInvalidAlmanacFields();
    return ok ? 0 : 1;
}