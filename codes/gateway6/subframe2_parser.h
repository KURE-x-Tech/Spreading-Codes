#pragma once
#ifndef LUNANET_GATEWAY6_SUBFRAME2_PARSER_H
#define LUNANET_GATEWAY6_SUBFRAME2_PARSER_H

#include <cstdint>
#include <string>
#include <vector>

namespace lunanet::gateway6
{

    // SB2 data width after CRC-24Q has been stripped by Gateway 5.
    // SB2 total = 1200 bits (Table 18): 1176 data + 24 CRC.
    constexpr int kSb2DataBits = 1176;
    constexpr int kSb2WnOffset = 0;
    constexpr int kSb2WnBits = 13;
    constexpr int kSb2ItowOffset = 13;
    constexpr int kSb2ItowBits = 9;
    constexpr int kSb2ToiOffset = 22;
    constexpr int kSb2ToiBits = 7;
    constexpr int kSb2TimeHeaderBits = kSb2WnBits + kSb2ItowBits + kSb2ToiBits;
    constexpr int kSb2HealthOffset = 22;
    constexpr int kSb2HealthBits = 8;
    constexpr int kSb2CedOffset = 30;
    constexpr int kSb2CedBits = 896;
    constexpr int kSb2TimeConversionsOffset = kSb2CedOffset + kSb2CedBits;
    constexpr int kSb2TimeConversionsBits = 224;
    constexpr int kSb2SpareOffset = kSb2TimeConversionsOffset + kSb2TimeConversionsBits;
    constexpr int kSb2SpareBits = kSb2DataBits - kSb2SpareOffset;
    constexpr uint32_t kSecondsPerWeek = 604800;
    constexpr uint16_t kSecondsPerItow = 1200;
    constexpr uint8_t kSecondsPerToi = 12;

    // -------------------------------------------------------------------------
    // Clock and Ephemeris Data (MSG-G4 / MSG-G1).
    //
    // NOTE: MSG-G4 and MSG-G1 are marked {LSIS-TBW-...} ("to be written") in
    // spec Section 2.5 -- the official spec does NOT yet define exact bit-level
    // field layouts.  The representation below is an ORIGINAL INTERPRETATION
    // chosen to be consistent with the interoperability-4.pdf JSON example
    // ("ced": {"af0": 1.23e-9, "af1": 4.56e-12, ...}).
    //
    // Chosen provisional view within the documented 896-bit CED block:
    //   [30 .. 61]  af0  32 bits signed, scale 2^-31 s   (clock bias)
    //   [62 .. 77]  af1  16 bits signed, scale 2^-43 s/s (clock drift)
    //   [78 .. 925] reserved / placeholder ephemeris fields (not yet defined)
    //
    // These choices MUST be updated if and when the spec publishes the real layout.
    // -------------------------------------------------------------------------
    struct CedData
    {
        double af0 = 0.0; // Clock bias  [s] -- 32-bit signed int × 2^-31
        double af1 = 0.0; // Clock drift [s/s] -- 16-bit signed int × 2^-43
        std::vector<uint8_t> raw_bits;
        bool provisional_layout = true;
    };

    // -------------------------------------------------------------------------
    // Health and Safety (MSG-G2).
    //
    // NOTE: MSG-G2 is marked {LSIS-TBW-...} in spec Section 2.5.  The official
    // spec does not define exact bit-level field layouts.  We use a 2-bit field
    // as an ORIGINAL INTERPRETATION:
    //   0 = healthy, 1 = marginal, 2 = unhealthy, 3 = do not use.
    // Bit offset: 22, length: 8 bits, per docs/spec_tables/sb2_bit_layout.csv.
    // -------------------------------------------------------------------------
    struct HealthData
    {
        uint8_t status = 0; // 8-bit provisional health status.
        std::vector<uint8_t> raw_bits;
        bool provisional_layout = true;
    };

    // -------------------------------------------------------------------------
    // Time Conversions (MSG-G30).
    // Per interoperability-4.pdf the baseline JSON example shows this as {}.
    // Placeholder struct -- fields will be populated once the spec is finalised.
    // -------------------------------------------------------------------------
    struct TimeConversions
    {
        std::vector<uint8_t> raw_bits;
        bool provisional_layout = true;
    };

    // -------------------------------------------------------------------------
    // Aggregate output of ParseSubframe2.
    // -------------------------------------------------------------------------
    struct Subframe2Data
    {
        uint16_t wn = 0;                           // Week Number, 13 bits (0–8191), Table 22
        uint16_t itow = 0;                         // Integer Time of Week, 9 bits (0–503), Table 22
        uint8_t toi = 0;                           // Time of Interval, 7 bits (0–99)
        uint64_t time_of_transmission_seconds = 0; // Relative to undefined LRT epoch.
        CedData ced;                               // Clock & Ephemeris Data (our bit layout)
        HealthData health;                         // Health & Safety (our bit layout)
        TimeConversions time_conversions;          // MSG-G30 placeholder
        std::vector<uint8_t> spare_bits;
    };

    // Computes relative LSIS time-of-transmission seconds from Table 22 fields.
    // The absolute LRT start epoch remains {LSIS-TBD-2003}, so this intentionally
    // does not convert to a UTC/GPS-style timestamp.
    uint64_t ComputeTimeOfTransmissionSeconds(uint16_t wn, uint16_t itow, uint8_t toi);

    // -------------------------------------------------------------------------
    // ParseSubframe2
    //
    // Parses a 1176-bit SB2 data vector (CRC already stripped by Gateway 5).
    // Returns true on success; populates *out_data.
    // Returns false and sets *error_message on length mismatch or other error.
    //
    // Bit layout consumed (MSB-first within each field):
    //   bits    0–12    WN   (13 bits)
    //   bits   13–21    ITOW (9 bits)
    //   bits   22–28    TOI  (7 bits, repository handoff compatibility)
    //   bits   22–29    Health raw block (8 bits, per sb2_bit_layout.csv)
    //   bits   30–925   CED raw block (896 bits)
    //   bits  926–1149  Time Conversions raw block (224 bits)
    //   bits 1150–1175  spare / reserved
    // The CED and Time Conversions blocks are retained as raw bits until the
    // detailed LSIS/LNSP message layouts are finalized.
    // -------------------------------------------------------------------------
    bool ParseSubframe2(const std::vector<uint8_t> &decoded_data_bits,
                        Subframe2Data *out_data,
                        std::string *error_message = nullptr);

} // namespace lunanet::gateway6

#endif // LUNANET_GATEWAY6_SUBFRAME2_PARSER_H
