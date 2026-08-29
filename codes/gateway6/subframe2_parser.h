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
    // Chosen layout (offsets measured from bit 29, after WN/ITOW/TOI header):
    //   [29 .. 60]  af0  32 bits signed, scale 2^-31 s   (clock bias)
    //   [61 .. 76]  af1  16 bits signed, scale 2^-43 s/s (clock drift)
    //   [77 ..]     reserved / placeholder ephemeris fields (not yet defined)
    //
    // These choices MUST be updated if and when the spec publishes the real layout.
    // -------------------------------------------------------------------------
    struct CedData
    {
        double af0 = 0.0; // Clock bias  [s] -- 32-bit signed int × 2^-31
        double af1 = 0.0; // Clock drift [s/s] -- 16-bit signed int × 2^-43
        // Future ephemeris/position fields will be added here once the spec defines
        // MSG-G4 bit allocations.
    };

    // -------------------------------------------------------------------------
    // Health and Safety (MSG-G2).
    //
    // NOTE: MSG-G2 is marked {LSIS-TBW-...} in spec Section 2.5.  The official
    // spec does not define exact bit-level field layouts.  We use a 2-bit field
    // as an ORIGINAL INTERPRETATION:
    //   0 = healthy, 1 = marginal, 2 = unhealthy, 3 = do not use.
    // Bit offset: immediately follows CED fields (bit 77 in data stream).
    // -------------------------------------------------------------------------
    struct HealthData
    {
        uint8_t status = 0; // 2-bit health status: 0=healthy, 1=marginal,
                            //                      2=unhealthy, 3=do-not-use
    };

    // -------------------------------------------------------------------------
    // Time Conversions (MSG-G30).
    // Per interoperability-4.pdf the baseline JSON example shows this as {}.
    // Placeholder struct -- fields will be populated once the spec is finalised.
    // -------------------------------------------------------------------------
    struct TimeConversions
    {
        // No fields defined yet.
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
    //   bits  0–12   WN   (13 bits)
    //   bits 13–21   ITOW  (9 bits)
    //   bits 22–28   TOI   (7 bits)
    //   bits 29–60   CED af0 (32-bit signed, scale 2^-31 s)
    //   bits 61–76   CED af1 (16-bit signed, scale 2^-43 s/s)
    //   bits 77–78   health.status (2 bits)
    //   bits 79–1175 spare / reserved (MSG-G30 placeholder + fill)
    // -------------------------------------------------------------------------
    bool ParseSubframe2(const std::vector<uint8_t> &decoded_data_bits,
                        Subframe2Data *out_data,
                        std::string *error_message = nullptr);

} // namespace lunanet::gateway6

#endif // LUNANET_GATEWAY6_SUBFRAME2_PARSER_H
