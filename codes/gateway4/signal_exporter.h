#pragma once
#ifndef LUNANET_GATEWAY4_SIGNAL_EXPORTER_H
#define LUNANET_GATEWAY4_SIGNAL_EXPORTER_H

#include <string>

#include "iq_generator.h"

namespace lunanet::gateway4 {

// Writes interleaved little-endian float32 I/Q pairs: [I0, Q0, I1, Q1, ...].
// File size is signal.i.size() * 2 * sizeof(float) bytes.
//
// @return true on success; sets *error_message on failure.
bool ExportIqBinary(const IqSignal& signal,
                    const std::string& output_path,
                    std::string* error_message = nullptr);

// Reads headerless interleaved little-endian float32 I/Q pairs written by
// ExportIqBinary. Since the format carries no metadata, the caller must supply
// the sample rate.
//
// @return true on success; false with *error_message set for an invalid sample
//         rate, empty/truncated input, malformed pair length, or non-finite
//         sample value.
bool ImportIqBinary(const std::string& input_path,
                    int sample_rate_hz,
                    IqSignal* out_signal,
                    std::string* error_message = nullptr);

// Writes I/Q samples as CSV with a header line "index,I,Q", one sample per row.
//
// @return true on success; sets *error_message on failure.
bool ExportIqCsv(const IqSignal& signal,
                 const std::string& output_path,
                 std::string* error_message = nullptr);

// Metadata carried in the standardized final-submission interop signal file
// header (see the LSIS-AFS interoperability testing document's "Signal
// Export Format"). Distinct from ExportIqBinary's headerless format above,
// which other tooling (e.g. the Goonhilly workshop CLI contract) expects
// without a header -- both are kept side by side for their respective
// test harnesses.
struct IqFileHeader {
    uint32_t version = 1;
    double sample_rate_hz = 0.0;
    double duration_sec = 0.0;
    uint32_t prn = 0;
};

// Writes the standardized interop I/Q file: a 128-byte header (magic
// "LSISIQ\0\0", version, sample_rate float64, duration float64, PRN
// uint32, sample-format string "float32", reserved), followed by
// interleaved little-endian float32 samples [I0, Q0, I1, Q1, ...].
//
// @return true on success; sets *error_message on failure.
bool ExportIqBinaryStandard(const IqSignal& signal,
                            uint32_t prn,
                            const std::string& output_path,
                            std::string* error_message = nullptr);

// Reads a standardized interop I/Q file written by ExportIqBinaryStandard
// (or another spec-compliant implementation). Validates the "LSISIQ\0\0"
// magic and that the sample-format field is "float32" (the only format
// this implementation supports).
//
// @return true on success (out_signal/out_header populated); false with
//         *error_message set on a magic mismatch, unsupported sample
//         format, or truncated/malformed sample payload.
bool ImportIqBinaryStandard(const std::string& input_path,
                            IqSignal* out_signal,
                            IqFileHeader* out_header,
                            std::string* error_message = nullptr);

}  // namespace lunanet::gateway4

#endif  // LUNANET_GATEWAY4_SIGNAL_EXPORTER_H
