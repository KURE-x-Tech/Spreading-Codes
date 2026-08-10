#pragma once
#ifndef LUNANET_GATEWAY6_SUBFRAME3_PARSER_H
#define LUNANET_GATEWAY6_SUBFRAME3_PARSER_H

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "gateway3/frame_config.h"

namespace lunanet::gateway6 {

// SB3 data width after Gateway 5 has validated and stripped the CRC-24Q.
// Table 19: 846 data bits + 24 CRC bits = 870 bits total.
constexpr int kSb3DataBits = 846;
constexpr int kSb3TypeFieldBits = lunanet::gateway3::kSb34TypeFieldBits;
constexpr int kSb3PayloadBits = kSb3DataBits - kSb3TypeFieldBits;

// The LSIS does not yet assign numeric SB3 type values. Type 1 is an MVP
// convention matching the interoperability document's example and is subject
// to replacement when LSIS-TBC-2023 or an LNSP SISICD defines the mapping.
constexpr uint8_t kSb3TypeOrbitAlmanac = 1;

// Provisional MSG-G5 profile. LSIS Section 2.5.4 is marked LSIS-TBW-2007,
// so these fields demonstrate end-to-end routing rather than define an
// operational LunaNet Almanac message:
//   raw payload bits  0-7   PRN (0=unspecified, 1-210 valid)
//   raw payload bits  8-20  reference week (13 bits)
//   raw payload bits 21-29  reference ITOW (0-503)
//   raw payload bits 30-31  health (0-3)
//   remaining bits          reserved and retained in raw_payload
struct OrbitAlmanacData {
    uint8_t prn = 0;
    uint16_t reference_week = 0;
    uint16_t reference_itow = 0;
    uint8_t health = 0;
};

using Subframe3Decoded = std::variant<std::monostate, OrbitAlmanacData>;

struct Subframe3Data {
    uint8_t type = 0;
    std::vector<uint8_t> raw_payload;
    Subframe3Decoded decoded;
};

// Parses the complete 846-bit SB3 data field after Gateway 5 has removed CRC.
// The type occupies the leading kSb3TypeFieldBits bits, MSB first. Unknown
// types are successful raw pass-through results with decoded=std::monostate.
// Returns false for an invalid output pointer, length, bit value, or a malformed
// payload for a recognized type. The output object is unchanged on failure.
bool ParseSubframe3(const std::vector<uint8_t>& decoded_data_bits,
                    Subframe3Data* out_data,
                    std::string* error_message = nullptr);

}  // namespace lunanet::gateway6

#endif  // LUNANET_GATEWAY6_SUBFRAME3_PARSER_H