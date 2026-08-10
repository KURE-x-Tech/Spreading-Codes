#include "signal_exporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>

namespace lunanet::gateway4 {

namespace {

// Serializes one float as 4 little-endian bytes, independent of host byte order.
void WriteLittleEndianFloat(std::ofstream& out, float value) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32 bits");
    std::memcpy(&bits, &value, sizeof(bits));
    char bytes[4] = {
        static_cast<char>(bits & 0xFFu),
        static_cast<char>((bits >> 8) & 0xFFu),
        static_cast<char>((bits >> 16) & 0xFFu),
        static_cast<char>((bits >> 24) & 0xFFu),
    };
    out.write(bytes, sizeof(bytes));
}

float ReadLittleEndianFloat(const unsigned char* p) {
    uint32_t bits = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                    (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void WriteU32Le(std::ofstream& out, uint32_t value) {
    char bytes[4] = {
        static_cast<char>(value & 0xFFu),
        static_cast<char>((value >> 8) & 0xFFu),
        static_cast<char>((value >> 16) & 0xFFu),
        static_cast<char>((value >> 24) & 0xFFu),
    };
    out.write(bytes, sizeof(bytes));
}

uint32_t ReadU32Le(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void WriteF64Le(std::ofstream& out, double value) {
    uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double must be 64 bits");
    std::memcpy(&bits, &value, sizeof(bits));
    char bytes[8];
    for (int i = 0; i < 8; ++i) {
        bytes[i] = static_cast<char>((bits >> (8 * i)) & 0xFFu);
    }
    out.write(bytes, sizeof(bytes));
}

double ReadF64Le(const unsigned char* p) {
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits |= static_cast<uint64_t>(p[i]) << (8 * i);
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

constexpr char kIqMagic[8] = {'L', 'S', 'I', 'S', 'I', 'Q', '\0', '\0'};
constexpr std::size_t kIqHeaderSize = 128;
constexpr char kSampleFormatFloat32[16] = {'f', 'l', 'o', 'a', 't', '3', '2'};
constexpr std::uint64_t kMaxIqFileBytes = 512ull * 1024ull * 1024ull;
constexpr std::size_t kBytesPerIqSample = 2u * sizeof(float);
constexpr uint32_t kMinPrn = 1u;
constexpr uint32_t kMaxPrn = 210u;

bool ValidateSignalForExport(const IqSignal& signal,
                             const std::string& output_path,
                             std::string* error_message) {
    if (signal.i.empty() || signal.i.size() != signal.q.size()) {
        if (error_message) *error_message = "Signal is empty or I/Q length mismatch";
        return false;
    }
    const std::uint64_t sample_count = static_cast<std::uint64_t>(signal.i.size());
    if (sample_count > kMaxIqFileBytes / kBytesPerIqSample) {
        if (error_message) {
            *error_message = "I/Q output exceeds the " +
                std::to_string(kMaxIqFileBytes) + "-byte safety limit: " + output_path;
        }
        return false;
    }
    for (std::size_t index = 0; index < signal.i.size(); ++index) {
        if (!std::isfinite(signal.i[index]) || !std::isfinite(signal.q[index])) {
            if (error_message) {
                *error_message = "Non-finite I/Q value at sample " +
                    std::to_string(index);
            }
            return false;
        }
    }
    return true;
}

bool ValidatePayloadSize(std::streamoff byte_count,
                         const std::string& input_path,
                         std::size_t* out_sample_count,
                         std::string* error_message) {
    if (byte_count <= 0) {
        if (error_message) *error_message = "I/Q sample payload is empty: " + input_path;
        return false;
    }
    if (static_cast<std::uint64_t>(byte_count) > kMaxIqFileBytes) {
        if (error_message) {
            *error_message = "I/Q file exceeds the " +
                std::to_string(kMaxIqFileBytes) + "-byte safety limit: " + input_path;
        }
        return false;
    }
    if (byte_count % static_cast<std::streamoff>(kBytesPerIqSample) != 0) {
        if (error_message) {
            *error_message = "I/Q payload length is not a multiple of " +
                std::to_string(kBytesPerIqSample) + " bytes: " + input_path;
        }
        return false;
    }

    const auto sample_count = static_cast<std::uint64_t>(
        byte_count / static_cast<std::streamoff>(kBytesPerIqSample));
    if (sample_count > static_cast<std::uint64_t>(std::vector<float>().max_size())) {
        if (error_message) *error_message = "I/Q file is too large to load safely: " + input_path;
        return false;
    }
    *out_sample_count = static_cast<std::size_t>(sample_count);
    return true;
}

bool ReadIqSamples(std::ifstream& in,
                   std::size_t sample_count,
                   int sample_rate_hz,
                   const std::string& input_path,
                   IqSignal* out_signal,
                   std::string* error_message) {
    IqSignal signal;
    signal.sample_rate_hz = sample_rate_hz;
    try {
        signal.i.reserve(sample_count);
        signal.q.reserve(sample_count);
    } catch (const std::bad_alloc&) {
        if (error_message) *error_message = "Insufficient memory for I/Q file: " + input_path;
        return false;
    } catch (const std::length_error&) {
        if (error_message) *error_message = "I/Q sample count exceeds vector limits: " + input_path;
        return false;
    }

    std::array<unsigned char, kBytesPerIqSample> bytes{};
    for (std::size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        in.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        if (!in) {
            if (error_message) {
                *error_message = "Truncated I/Q pair at sample " +
                    std::to_string(sample_index) + ": " + input_path;
            }
            return false;
        }

        const float i_sample = ReadLittleEndianFloat(bytes.data());
        const float q_sample = ReadLittleEndianFloat(bytes.data() + sizeof(float));
        if (!std::isfinite(i_sample) || !std::isfinite(q_sample)) {
            if (error_message) {
                *error_message = "Non-finite I/Q value at sample " +
                    std::to_string(sample_index) + ": " + input_path;
            }
            return false;
        }
        signal.i.push_back(i_sample);
        signal.q.push_back(q_sample);
    }

    *out_signal = std::move(signal);
    return true;
}

}  // namespace

bool ExportIqBinary(const IqSignal& signal,
                    const std::string& output_path,
                    std::string* error_message) {
    if (!ValidateSignalForExport(signal, output_path, error_message)) {
        return false;
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        if (error_message) *error_message = "Failed to open: " + output_path;
        return false;
    }

    for (size_t n = 0; n < signal.i.size(); ++n) {
        WriteLittleEndianFloat(out, signal.i[n]);
        WriteLittleEndianFloat(out, signal.q[n]);
    }

    if (!out) {
        if (error_message) *error_message = "Write failed: " + output_path;
        return false;
    }

    return true;
}

bool ImportIqBinary(const std::string& input_path,
                    int sample_rate_hz,
                    IqSignal* out_signal,
                    std::string* error_message) {
    if (out_signal == nullptr) {
        if (error_message) *error_message = "I/Q output must not be null";
        return false;
    }
    if (sample_rate_hz <= 0) {
        if (error_message) *error_message = "Sample rate must be > 0";
        return false;
    }

    std::ifstream in(input_path, std::ios::binary | std::ios::ate);
    if (!in) {
        if (error_message) *error_message = "Failed to open: " + input_path;
        return false;
    }

    std::size_t sample_count = 0;
    if (!ValidatePayloadSize(in.tellg(), input_path, &sample_count, error_message)) {
        return false;
    }

    in.seekg(0, std::ios::beg);
    return ReadIqSamples(
        in, sample_count, sample_rate_hz, input_path, out_signal, error_message);
}

bool ExportIqCsv(const IqSignal& signal,
                 const std::string& output_path,
                 std::string* error_message) {
    if (!ValidateSignalForExport(signal, output_path, error_message)) {
        return false;
    }

    std::ofstream out(output_path);
    if (!out) {
        if (error_message) *error_message = "Failed to open: " + output_path;
        return false;
    }

    out << "index,I,Q\n";
    for (size_t n = 0; n < signal.i.size(); ++n) {
        out << n << ',' << signal.i[n] << ',' << signal.q[n] << '\n';
    }

    if (!out) {
        if (error_message) *error_message = "Write failed: " + output_path;
        return false;
    }

    return true;
}

bool ExportIqBinaryStandard(const IqSignal& signal,
                           uint32_t prn,
                           const std::string& output_path,
                           std::string* error_message) {
    if (!ValidateSignalForExport(signal, output_path, error_message)) {
        return false;
    }
    if (signal.sample_rate_hz <= 0) {
        if (error_message) *error_message = "Signal has an invalid sample rate";
        return false;
    }
    if (prn < kMinPrn || prn > kMaxPrn) {
        if (error_message) *error_message = "PRN must be in the range 1-210";
        return false;
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        if (error_message) *error_message = "Failed to open: " + output_path;
        return false;
    }

    const double duration_sec =
        static_cast<double>(signal.i.size()) / static_cast<double>(signal.sample_rate_hz);

    out.write(kIqMagic, sizeof(kIqMagic));
    WriteU32Le(out, 1);  // version
    WriteF64Le(out, static_cast<double>(signal.sample_rate_hz));
    WriteF64Le(out, duration_sec);
    WriteU32Le(out, prn);
    out.write(kSampleFormatFloat32, sizeof(kSampleFormatFloat32));
    const char reserved[80] = {0};
    out.write(reserved, sizeof(reserved));

    for (size_t n = 0; n < signal.i.size(); ++n) {
        WriteLittleEndianFloat(out, signal.i[n]);
        WriteLittleEndianFloat(out, signal.q[n]);
    }

    if (!out) {
        if (error_message) *error_message = "Write failed: " + output_path;
        return false;
    }
    return true;
}

bool ImportIqBinaryStandard(const std::string& input_path,
                           IqSignal* out_signal,
                           IqFileHeader* out_header,
                           std::string* error_message) {
    if (out_signal == nullptr) {
        if (error_message) *error_message = "I/Q output must not be null";
        return false;
    }

    std::ifstream in(input_path, std::ios::binary | std::ios::ate);
    if (!in) {
        if (error_message) *error_message = "Failed to open: " + input_path;
        return false;
    }

    const std::streamoff total_bytes = in.tellg();
    if (total_bytes < static_cast<std::streamoff>(kIqHeaderSize)) {
        if (error_message) *error_message = "File is shorter than the " +
            std::to_string(kIqHeaderSize) + "-byte header: " + input_path;
        return false;
    }
    const std::streamoff payload_bytes =
        total_bytes - static_cast<std::streamoff>(kIqHeaderSize);
    std::size_t num_samples = 0;
    if (!ValidatePayloadSize(payload_bytes, input_path, &num_samples, error_message)) {
        return false;
    }

    in.seekg(0, std::ios::beg);

    std::vector<unsigned char> header(kIqHeaderSize);
    in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(kIqHeaderSize));
    if (!in || static_cast<std::size_t>(in.gcount()) != kIqHeaderSize) {
        if (error_message) *error_message = "File is shorter than the " +
            std::to_string(kIqHeaderSize) + "-byte header: " + input_path;
        return false;
    }

    if (!std::equal(std::begin(kIqMagic), std::end(kIqMagic), header.begin())) {
        if (error_message) *error_message = "Magic mismatch: not a valid LSISIQ signal file";
        return false;
    }

    IqFileHeader parsed;
    parsed.version = ReadU32Le(&header[8]);
    parsed.sample_rate_hz = ReadF64Le(&header[12]);
    parsed.duration_sec = ReadF64Le(&header[20]);
    parsed.prn = ReadU32Le(&header[28]);

    // The interop spec fixes this format's version at 1; a future version
    // may change the header layout, so an unrecognized version must be
    // rejected rather than blindly parsed with v1 field offsets.
    if (parsed.version != 1) {
        if (error_message) *error_message = "Unsupported I/Q file version " +
            std::to_string(parsed.version) + " (only version 1 is supported): " + input_path;
        return false;
    }

    const bool is_float32 = std::equal(std::begin(kSampleFormatFloat32),
                                       std::end(kSampleFormatFloat32),
                                       header.begin() + 32);
    if (!is_float32) {
        if (error_message) *error_message = "Unsupported sample format (only float32 is supported)";
        return false;
    }

    // This header field comes from a file that may have been produced by
    // another team's implementation, so a garbled/malicious value (NaN,
    // negative, fractional, or larger than an int can hold) must not reach
    // the narrowing static_cast<int> below -- that would be undefined
    // behavior for out-of-range doubles and would silently truncate
    // fractional ones. Our own ExportIqBinaryStandard always writes an
    // exact integer (round-tripped from IqSignal::sample_rate_hz, an int),
    // so requiring an exact integral value here rejects only genuinely
    // malformed files, not legitimate ones.
    const bool rate_is_valid =
        std::isfinite(parsed.sample_rate_hz) &&
        parsed.sample_rate_hz > 0.0 &&
        parsed.sample_rate_hz <= static_cast<double>(std::numeric_limits<int>::max()) &&
        parsed.sample_rate_hz == std::floor(parsed.sample_rate_hz);
    if (!rate_is_valid) {
        if (error_message) *error_message = "Header declares an invalid sample rate: " + input_path;
        return false;
    }
    if (parsed.prn < kMinPrn || parsed.prn > kMaxPrn) {
        if (error_message) *error_message = "Header declares an invalid PRN: " + input_path;
        return false;
    }
    if (!std::isfinite(parsed.duration_sec) || parsed.duration_sec <= 0.0) {
        if (error_message) *error_message = "Header declares an invalid duration: " + input_path;
        return false;
    }

    // Cross-check the recovered sample count against the header's declared
    // duration -- catches a file truncated exactly on an I/Q pair boundary.
    // Half a sample absorbs decimal rounding without admitting a missing pair.
    const double expected_samples = parsed.duration_sec * parsed.sample_rate_hz;
    if (!std::isfinite(expected_samples) ||
        std::fabs(static_cast<double>(num_samples) - expected_samples) >= 0.5) {
        if (error_message) *error_message = "Sample count (" + std::to_string(num_samples) +
            ") does not match header's duration_sec * sample_rate_hz (~" +
            std::to_string(expected_samples) + "): " + input_path;
        return false;
    }

    IqSignal signal;
    if (!ReadIqSamples(in,
                       num_samples,
                       static_cast<int>(parsed.sample_rate_hz),
                       input_path,
                       &signal,
                       error_message)) {
        return false;
    }

    if (out_header) *out_header = parsed;
    *out_signal = std::move(signal);
    return true;
}

}  // namespace lunanet::gateway4
