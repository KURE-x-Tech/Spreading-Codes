#include "subframe1_parser.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lunanet::gateway6
{

    namespace
    {

        // Extracts 'count' MSB-first bits starting at bit offset 'start' and returns
        // their value as a uint64_t (unsigned). Caller is responsible for ensuring
        // start + count <= bits.size().
        uint64_t ExtractUBits(const std::vector<uint8_t> &bits, int start, int count)
        {
            uint64_t value = 0;
            for (int i = 0; i < count; ++i)
            {
                value = (value << 1u) | (bits[static_cast<size_t>(start + i)] & 1u);
            }
            return value;
        }

    } // namespace

    bool ParseSubframe1(const std::vector<uint8_t> &decoded_data_bits,
                        Subframe1Data *out_data,
                        std::string *error_message)
    {
        if (!out_data)
        {
            if (error_message)
            {
                *error_message = "out_data must not be null";
            }
            return false;
        }

        if (static_cast<int>(decoded_data_bits.size()) != kSb1DataBits)
        {
            if (error_message)
            {
                *error_message = "SB1 parser expects exactly " +
                                 std::to_string(kSb1DataBits) +
                                 " data bits, got " +
                                 std::to_string(decoded_data_bits.size());
            }
            return false;
        }

        for (size_t i = 0; i < decoded_data_bits.size(); ++i)
        {
            if (decoded_data_bits[i] > 1u)
            {
                if (error_message)
                {
                    *error_message = "SB1 parser received non-binary bit value " +
                                     std::to_string(decoded_data_bits[i]) +
                                     " at index " + std::to_string(i);
                }
                return false;
            }
        }

        // -----------------------------------------------------------------------
        // SB1 bit allocation (LSIS-AFS §2.4.2.2, Table 13 & Table 14):
        //   bits 0–1   FID (2 bits, unsigned, 0–3)
        //   bits 2–8   TOI (7 bits, unsigned, 0–99)
        // -----------------------------------------------------------------------
        const auto fid = static_cast<uint8_t>(ExtractUBits(decoded_data_bits, 0, kSb1FidBits));
        const auto toi = static_cast<uint8_t>(ExtractUBits(decoded_data_bits, kSb1FidBits, kSb1ToiBits));

        if (toi > kSb1ToiMax)
        {
            if (error_message)
            {
                *error_message = "TOI value " + std::to_string(toi) +
                                 " exceeds Table 13 maximum of " +
                                 std::to_string(kSb1ToiMax);
            }
            return false;
        }

        out_data->fid = fid;
        out_data->toi = toi;
        return true;
    }

    bool ParseSubframe1(uint16_t raw_sb1_word,
                        Subframe1Data *out_data,
                        std::string *error_message)
    {
        if (!out_data)
        {
            if (error_message)
            {
                *error_message = "out_data must not be null";
            }
            return false;
        }

        if (raw_sb1_word > 0x1FFu)
        {
            if (error_message)
            {
                *error_message = "SB1 raw word value 0x" +
                                 std::to_string(raw_sb1_word) +
                                 " exceeds 9-bit maximum of 0x1FF";
            }
            return false;
        }

        const auto fid = static_cast<uint8_t>((raw_sb1_word >> 7) & 0x3u);
        const auto toi = static_cast<uint8_t>(raw_sb1_word & 0x7Fu);

        if (toi > kSb1ToiMax)
        {
            if (error_message)
            {
                *error_message = "TOI value " + std::to_string(toi) +
                                 " exceeds Table 13 maximum of " +
                                 std::to_string(kSb1ToiMax);
            }
            return false;
        }

        out_data->fid = fid;
        out_data->toi = toi;
        return true;
    }

} // namespace lunanet::gateway6