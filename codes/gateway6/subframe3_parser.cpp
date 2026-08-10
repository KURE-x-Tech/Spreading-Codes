#include "subframe3_parser.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace lunanet::gateway6 {

namespace {

static_assert(kSb3TypeFieldBits == 4 || kSb3TypeFieldBits == 6,
              "SB3 type field must be 4 or 6 bits");
static_assert(kSb3PayloadBits >= 32,
              "SB3 payload must fit the provisional MSG-G5 header");

bool Fail(std::string* error_message, const std::string& message) {
    if (error_message) {
        *error_message = message;
    }
    return false;
}

uint64_t ExtractUBits(const std::vector<uint8_t>& bits,
                      std::size_t start,
                      int count) {
    uint64_t value = 0;
    for (int i = 0; i < count; ++i) {
        value = (value << 1u) | bits[start + static_cast<std::size_t>(i)];
    }
    return value;
}

bool DecodeOrbitAlmanac(const std::vector<uint8_t>& raw_payload,
                        OrbitAlmanacData* out_almanac,
                        std::string* error_message) {
    constexpr int kAlmanacHeaderBits = 32;
    if (raw_payload.size() < kAlmanacHeaderBits) {
        return Fail(error_message,
                    "MSG-G5 payload is too short for the 32-bit provisional header");
    }

    OrbitAlmanacData almanac;
    almanac.prn = static_cast<uint8_t>(ExtractUBits(raw_payload, 0, 8));
    almanac.reference_week =
        static_cast<uint16_t>(ExtractUBits(raw_payload, 8, 13));
    almanac.reference_itow =
        static_cast<uint16_t>(ExtractUBits(raw_payload, 21, 9));
    almanac.health = static_cast<uint8_t>(ExtractUBits(raw_payload, 30, 2));

    if (almanac.prn > 210) {
        return Fail(error_message,
                    "MSG-G5 PRN value " + std::to_string(almanac.prn) +
                        " exceeds the supported maximum of 210");
    }

    if (almanac.reference_itow > 503) {
        return Fail(error_message,
                    "MSG-G5 reference ITOW value " +
                        std::to_string(almanac.reference_itow) +
                        " exceeds the maximum of 503");
    }

    *out_almanac = almanac;
    return true;
}

}  // namespace

bool ParseSubframe3(const std::vector<uint8_t>& decoded_data_bits,
                    Subframe3Data* out_data,
                    std::string* error_message) {
    if (!out_data) {
        return Fail(error_message, "out_data must not be null");
    }

    if (static_cast<int>(decoded_data_bits.size()) != kSb3DataBits) {
        return Fail(error_message,
                    "SB3 parser expects exactly " + std::to_string(kSb3DataBits) +
                        " data bits (type + payload, CRC-stripped), got " +
                        std::to_string(decoded_data_bits.size()));
    }

    for (std::size_t i = 0; i < decoded_data_bits.size(); ++i) {
        if (decoded_data_bits[i] > 1u) {
            return Fail(error_message,
                        "SB3 parser received non-binary value " +
                            std::to_string(decoded_data_bits[i]) + " at bit " +
                            std::to_string(i));
        }
    }

    Subframe3Data parsed;
    parsed.type = static_cast<uint8_t>(
        ExtractUBits(decoded_data_bits, 0, kSb3TypeFieldBits));
    parsed.raw_payload.assign(
        decoded_data_bits.begin() + kSb3TypeFieldBits,
        decoded_data_bits.end());

    if (parsed.type == kSb3TypeOrbitAlmanac) {
        OrbitAlmanacData almanac;
        if (!DecodeOrbitAlmanac(parsed.raw_payload, &almanac, error_message)) {
            return false;
        }
        parsed.decoded = almanac;
    }

    *out_data = std::move(parsed);
    if (error_message) {
        error_message->clear();
    }
    return true;
}

}  // namespace lunanet::gateway6