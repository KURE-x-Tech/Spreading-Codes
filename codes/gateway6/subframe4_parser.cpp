#include "subframe4_parser.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace lunanet::gateway6
{

    namespace
    {

        bool Fail(std::string *error_message, const std::string &message)
        {
            if (error_message)
            {
                *error_message = message;
            }
            return false;
        }

        uint8_t ExtractType(const std::vector<uint8_t> &bits)
        {
            uint8_t type = 0;
            for (int i = 0; i < kSb4TypeFieldBits; ++i)
            {
                type = static_cast<uint8_t>((type << 1u) | bits[static_cast<std::size_t>(i)]);
            }
            return type;
        }

    } // namespace

    bool ParseSubframe4(const std::vector<uint8_t> &decoded_data_bits,
                        Subframe4Data *out_data,
                        std::string *error_message)
    {
        if (!out_data)
        {
            return Fail(error_message, "out_data must not be null");
        }

        if (static_cast<int>(decoded_data_bits.size()) != kSb4DataBits)
        {
            return Fail(error_message,
                        "SB4 parser expects exactly " + std::to_string(kSb4DataBits) +
                            " data bits (type + payload, CRC-stripped), got " +
                            std::to_string(decoded_data_bits.size()));
        }

        for (std::size_t i = 0; i < decoded_data_bits.size(); ++i)
        {
            if (decoded_data_bits[i] > 1u)
            {
                return Fail(error_message,
                            "SB4 parser received non-binary value " +
                                std::to_string(decoded_data_bits[i]) + " at bit " +
                                std::to_string(i));
            }
        }

        Subframe4Data parsed;
        parsed.type = ExtractType(decoded_data_bits);
        parsed.raw_payload.assign(decoded_data_bits.begin() + kSb4TypeFieldBits,
                                  decoded_data_bits.end());

        *out_data = std::move(parsed);
        if (error_message)
        {
            error_message->clear();
        }
        return true;
    }

} // namespace lunanet::gateway6