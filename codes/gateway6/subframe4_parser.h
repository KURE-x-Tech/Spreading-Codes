#pragma once
#ifndef LUNANET_GATEWAY6_SUBFRAME4_PARSER_H
#define LUNANET_GATEWAY6_SUBFRAME4_PARSER_H

#include <cstdint>
#include <string>
#include <vector>

#include "gateway3/frame_config.h"

namespace lunanet::gateway6
{

    // SB4 data width after Gateway 5 has validated and stripped the CRC-24Q.
    constexpr int kSb4DataBits = 846;
    constexpr int kSb4TypeFieldBits = lunanet::gateway3::kSb34TypeFieldBits;
    constexpr int kSb4PayloadBits = kSb4DataBits - kSb4TypeFieldBits;

    // The LSIS defines SB4 as a dynamic, LNSP-specific subframe. Its type values
    // and message payload layouts require the relevant LNSP SISICD.
    struct Subframe4Data
    {
        uint8_t type = 0;
        std::vector<uint8_t> raw_payload;
        std::vector<uint8_t> network_access_payload;
        bool requires_lnsp_sisicd = true;
    };

    // Parses the complete 846-bit SB4 data field after Gateway 5 has removed CRC.
    // The type occupies the leading kSb4TypeFieldBits bits, MSB first. The payload
    // is returned unchanged as raw_payload and network_access_payload because no
    // interoperable SB4 message layouts are yet specified. Returns false for an
    // invalid output pointer, length, or bit value.
    bool ParseSubframe4(const std::vector<uint8_t> &decoded_data_bits,
                        Subframe4Data *out_data,
                        std::string *error_message = nullptr);

} // namespace lunanet::gateway6

#endif // LUNANET_GATEWAY6_SUBFRAME4_PARSER_H