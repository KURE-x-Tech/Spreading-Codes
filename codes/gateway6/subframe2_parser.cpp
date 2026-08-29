#include "subframe2_parser.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lunanet::gateway6
{

    namespace
    {

        // Extracts 'count' MSB-first bits starting at bit offset 'start' and returns
        // their value as a uint64_t (unsigned).  Caller is responsible for ensuring
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

        // Same as ExtractUBits but sign-extends the result to int64_t.
        int64_t ExtractSBits(const std::vector<uint8_t> &bits, int start, int count)
        {
            uint64_t raw = ExtractUBits(bits, start, count);
            // Sign-extend: if MSB is set, propagate to upper bits.
            if (count > 0 && (raw >> (count - 1)) & 1u)
            {
                raw |= ~((uint64_t(1) << count) - 1u);
            }
            return static_cast<int64_t>(raw);
        }

    } // namespace

    uint64_t ComputeTimeOfTransmissionSeconds(uint16_t wn, uint16_t itow, uint8_t toi)
    {
        return static_cast<uint64_t>(wn) * kSecondsPerWeek +
               static_cast<uint64_t>(itow) * kSecondsPerItow +
               static_cast<uint64_t>(toi) * kSecondsPerToi;
    }

    bool ParseSubframe2(const std::vector<uint8_t> &decoded_data_bits,
                        Subframe2Data *out_data,
                        std::string *error_message)
    {
        if (!out_data)
        {
            if (error_message)
                *error_message = "out_data must not be null";
            return false;
        }

        if (static_cast<int>(decoded_data_bits.size()) != kSb2DataBits)
        {
            if (error_message)
            {
                *error_message = "SB2 parser expects exactly " +
                                 std::to_string(kSb2DataBits) +
                                 " data bits (CRC-stripped), got " +
                                 std::to_string(decoded_data_bits.size());
            }
            return false;
        }

        // -----------------------------------------------------------------------
        // Header fields (Table 22, LSIS-FID0-500/MSG-G8)
        // -----------------------------------------------------------------------
        //   bits  0–12   WN   (13 bits, unsigned, 0–8191)
        //   bits 13–21   ITOW  (9 bits, unsigned, 0–503)
        //   bits 22–28   TOI   (7 bits, unsigned, 0–99)
        out_data->wn = static_cast<uint16_t>(ExtractUBits(decoded_data_bits, 0, 13));

        const auto itow = static_cast<uint16_t>(ExtractUBits(decoded_data_bits, 13, 9));
        if (itow > 503)
        {
            if (error_message)
            {
                *error_message = "ITOW value " + std::to_string(itow) +
                                 " exceeds Table 22 maximum of 503";
            }
            return false;
        }

        const auto toi = static_cast<uint8_t>(ExtractUBits(decoded_data_bits, 22, 7));
        if (toi > 99)
        {
            if (error_message)
            {
                *error_message = "TOI value " + std::to_string(toi) +
                                 " exceeds Table 22 maximum of 99";
            }
            return false;
        }

        out_data->itow = itow;
        out_data->toi = toi;
        out_data->time_of_transmission_seconds =
            ComputeTimeOfTransmissionSeconds(out_data->wn, out_data->itow, out_data->toi);

        // -----------------------------------------------------------------------
        // CED: Clock and Ephemeris Data (MSG-G4 / MSG-G1)
        //
        // ORIGINAL INTERPRETATION -- spec marks this {LSIS-TBW-...}.
        // See subframe2_parser.h for rationale.
        //
        //   bits 29–60   af0  (32-bit signed, scale factor 2^-31 s)
        //   bits 61–76   af1  (16-bit signed, scale factor 2^-43 s/s)
        // -----------------------------------------------------------------------
        constexpr double kAf0Scale = 4.6566128730773926e-10; // 2^-31
        constexpr double kAf1Scale = 1.1368683772161603e-13; // 2^-43
        const int64_t af0_raw = ExtractSBits(decoded_data_bits, 29, 32);
        const int64_t af1_raw = ExtractSBits(decoded_data_bits, 61, 16);
        out_data->ced.af0 = static_cast<double>(af0_raw) * kAf0Scale;
        out_data->ced.af1 = static_cast<double>(af1_raw) * kAf1Scale;

        // -----------------------------------------------------------------------
        // Health and Safety (MSG-G2)
        //
        // ORIGINAL INTERPRETATION -- spec marks this {LSIS-TBW-...}.
        // See subframe2_parser.h for rationale.
        //
        //   bits 77–78   health.status (2 bits, unsigned)
        //                0=healthy, 1=marginal, 2=unhealthy, 3=do-not-use
        // -----------------------------------------------------------------------
        out_data->health.status = static_cast<uint8_t>(ExtractUBits(decoded_data_bits, 77, 2));

        // -----------------------------------------------------------------------
        // Time Conversions (MSG-G30) -- placeholder, no bits consumed yet.
        // The interoperability-4.pdf JSON example shows this as {}.
        // -----------------------------------------------------------------------
        out_data->time_conversions = TimeConversions{};

        return true;
    }

} // namespace lunanet::gateway6
