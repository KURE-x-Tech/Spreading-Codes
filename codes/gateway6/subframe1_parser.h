#pragma once

#ifndef LUNANET_GATEWAY6_SUBFRAME1_PARSER_H
#define LUNANET_GATEWAY6_SUBFRAME1_PARSER_H

#include <cstdint>
#include <string>
#include <vector>

namespace lunanet::gateway6
{

    // SB1 data width: 9 bits (LSIS-AFS §2.4.2.2, Table 13 & Table 14).
    constexpr int kSb1DataBits = 9;
    constexpr int kSb1FidBits = 2;
    constexpr int kSb1ToiBits = 7;
    constexpr uint8_t kSb1FidMax = 3;
    constexpr uint8_t kSb1ToiMax = 99;

    // -------------------------------------------------------------------------
    // Subframe 1 Data (LSIS-410, Table 13).
    //   FID: Frame ID, 2 bits (0..3)
    //   TOI: Time of Interval, 7 bits (0..99)
    // -------------------------------------------------------------------------
    struct Subframe1Data
    {
        uint8_t fid = 0; // Frame ID, 2 bits (0–3), Table 13
        uint8_t toi = 0; // Time of Interval, 7 bits (0–99), Table 13
    };

    // -------------------------------------------------------------------------
    // ParseSubframe1 (bit vector overload)
    //
    // Parses a 9-bit SB1 data bit vector (MSB-first: bits[0..1]=FID, bits[2..8]=TOI).
    // Returns true on success; populates *out_data.
    // Returns false and sets *error_message on null pointer, length mismatch,
    // non-binary values, or out-of-range fields (e.g. TOI > 99).
    // -------------------------------------------------------------------------
    bool ParseSubframe1(const std::vector<uint8_t> &decoded_data_bits,
                        Subframe1Data *out_data,
                        std::string *error_message = nullptr);

    // -------------------------------------------------------------------------
    // ParseSubframe1 (9-bit raw integer word overload)
    //
    // Parses a 9-bit raw integer word (bits[8:7]=FID, bits[6:0]=TOI).
    // Returns true on success; populates *out_data.
    // Returns false and sets *error_message on null pointer, word > 0x1FF,
    // or out-of-range fields (e.g. TOI > 99).
    // -------------------------------------------------------------------------
    bool ParseSubframe1(uint16_t raw_sb1_word,
                        Subframe1Data *out_data,
                        std::string *error_message = nullptr);

} // namespace lunanet::gateway6

#endif // LUNANET_GATEWAY6_SUBFRAME1_PARSER_H