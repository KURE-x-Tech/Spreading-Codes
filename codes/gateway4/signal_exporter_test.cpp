#include "signal_exporter.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace {

lunanet::gateway4::IqSignal MakeSmallSignal() {
    lunanet::gateway4::IqSignal signal;
    signal.sample_rate_hz = 1023000;
    signal.i = {+1.0f, -1.0f};
    signal.q = {-1.0f, +1.0f};
    return signal;
}

bool OverwriteBytes(const std::string& path,
                    long offset,
                    const unsigned char* bytes,
                    std::size_t count) {
    FILE* file = std::fopen(path.c_str(), "r+b");
    if (!file) return false;
    const bool ok = std::fseek(file, offset, SEEK_SET) == 0 &&
        std::fwrite(bytes, 1, count, file) == count;
    std::fclose(file);
    return ok;
}

bool TestStandardFormatRoundTrip() {
    lunanet::gateway4::IqSignal signal;
    signal.sample_rate_hz = 1023000;
    for (int n = 0; n < 100; ++n) {
        signal.i.push_back((n % 2 == 0) ? 1.0f : -1.0f);
        signal.q.push_back((n % 3 == 0) ? 1.0f : -1.0f);
    }

    const std::string path = "test_iq_standard.tmp.iq";
    std::string error;
    if (!lunanet::gateway4::ExportIqBinaryStandard(signal, /*prn=*/7, path, &error)) {
        std::cerr << "FAIL [export]: " << error << "\n";
        return false;
    }

    lunanet::gateway4::IqSignal read_back;
    lunanet::gateway4::IqFileHeader header;
    if (!lunanet::gateway4::ImportIqBinaryStandard(path, &read_back, &header, &error)) {
        std::cerr << "FAIL [import]: " << error << "\n";
        std::remove(path.c_str());
        return false;
    }
    std::remove(path.c_str());

    bool ok = true;
    if (header.prn != 7) {
        std::cerr << "FAIL: prn mismatch, got " << header.prn << "\n";
        ok = false;
    }
    if (header.sample_rate_hz != 1023000.0) {
        std::cerr << "FAIL: sample_rate_hz mismatch, got " << header.sample_rate_hz << "\n";
        ok = false;
    }
    if (read_back.i.size() != signal.i.size() || read_back.q.size() != signal.q.size()) {
        std::cerr << "FAIL: sample count mismatch\n";
        ok = false;
    }
    for (size_t n = 0; n < signal.i.size() && ok; ++n) {
        if (read_back.i[n] != signal.i[n] || read_back.q[n] != signal.q[n]) {
            std::cerr << "FAIL: sample " << n << " mismatch\n";
            ok = false;
        }
    }
    return ok;
}

bool TestRawFormatRoundTrip() {
    lunanet::gateway4::IqSignal signal;
    signal.sample_rate_hz = 1023000;
    signal.i = {+1.0f, -0.5f, +0.25f};
    signal.q = {-1.0f, +0.5f, -0.25f};

    const std::string path = "test_iq_raw.tmp.iq32";
    std::string error;
    if (!lunanet::gateway4::ExportIqBinary(signal, path, &error)) {
        std::cerr << "FAIL [raw export]: " << error << "\n";
        return false;
    }

    lunanet::gateway4::IqSignal read_back;
    const bool imported = lunanet::gateway4::ImportIqBinary(
        path, signal.sample_rate_hz, &read_back, &error);
    std::remove(path.c_str());
    if (!imported) {
        std::cerr << "FAIL [raw import]: " << error << "\n";
        return false;
    }

    return read_back.sample_rate_hz == signal.sample_rate_hz &&
        read_back.i == signal.i && read_back.q == signal.q;
}

bool TestRawRejectsPartialPair() {
    const std::string path = "test_iq_partial.tmp.iq32";
    {
        FILE* file = std::fopen(path.c_str(), "wb");
        if (!file) {
            std::cerr << "FAIL: could not create fixture file " << path << "\n";
            return false;
        }
        const unsigned char bytes[7] = {0};
        std::fwrite(bytes, 1, sizeof(bytes), file);
        std::fclose(file);
    }

    lunanet::gateway4::IqSignal signal;
    std::string error;
    const bool imported =
        lunanet::gateway4::ImportIqBinary(path, 1023000, &signal, &error);
    std::remove(path.c_str());
    return !imported && !error.empty();
}

bool TestRejectsBadMagic() {
    // Write a file whose header lacks the "LSISIQ\0\0" magic entirely and
    // confirm the importer rejects it rather than misinterpreting garbage.
    const std::string path = "test_iq_bad_magic.tmp.iq";
    {
        std::string junk(128 + 8, 'X');
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) {
            std::cerr << "FAIL: could not create fixture file " << path << "\n";
            return false;
        }
        std::fwrite(junk.data(), 1, junk.size(), f);
        std::fclose(f);
    }

    lunanet::gateway4::IqSignal signal;
    lunanet::gateway4::IqFileHeader header;
    std::string error;
    const bool imported =
        lunanet::gateway4::ImportIqBinaryStandard(path, &signal, &header, &error);
    std::remove(path.c_str());

    if (imported || error.empty()) {
        std::cerr << "FAIL: expected magic-mismatch rejection with an error message\n";
        return false;
    }
    return true;
}

bool TestStandardRejectsInvalidDuration() {
    const std::string path = "test_iq_bad_duration.tmp.iq";
    std::string error;
    if (!lunanet::gateway4::ExportIqBinaryStandard(MakeSmallSignal(), 7, path, &error)) {
        std::cerr << "FAIL [duration fixture]: " << error << "\n";
        return false;
    }

    const unsigned char quiet_nan_le[8] = {0, 0, 0, 0, 0, 0, 0xF8, 0x7F};
    if (!OverwriteBytes(path, 20, quiet_nan_le, sizeof(quiet_nan_le))) {
        std::remove(path.c_str());
        return false;
    }

    lunanet::gateway4::IqSignal signal;
    lunanet::gateway4::IqFileHeader header;
    const bool imported =
        lunanet::gateway4::ImportIqBinaryStandard(path, &signal, &header, &error);
    std::remove(path.c_str());
    return !imported && !error.empty();
}

bool TestStandardRejectsNonFiniteSample() {
    const std::string path = "test_iq_non_finite.tmp.iq";
    std::string error;
    if (!lunanet::gateway4::ExportIqBinaryStandard(MakeSmallSignal(), 7, path, &error)) {
        std::cerr << "FAIL [sample fixture]: " << error << "\n";
        return false;
    }

    const unsigned char positive_infinity_le[4] = {0, 0, 0x80, 0x7F};
    if (!OverwriteBytes(path, 128 + 4, positive_infinity_le,
                        sizeof(positive_infinity_le))) {
        std::remove(path.c_str());
        return false;
    }

    lunanet::gateway4::IqSignal signal;
    lunanet::gateway4::IqFileHeader header;
    const bool imported =
        lunanet::gateway4::ImportIqBinaryStandard(path, &signal, &header, &error);
    std::remove(path.c_str());
    return !imported && !error.empty();
}

bool TestStandardRejectsEmptyPayload() {
    const std::string path = "test_iq_empty_payload.tmp.iq";
    std::string error;
    if (!lunanet::gateway4::ExportIqBinaryStandard(MakeSmallSignal(), 7, path, &error)) {
        std::cerr << "FAIL [empty fixture]: " << error << "\n";
        return false;
    }

    std::array<char, 128> header_bytes{};
    {
        std::ifstream input(path, std::ios::binary);
        input.read(header_bytes.data(), static_cast<std::streamsize>(header_bytes.size()));
        if (!input) {
            std::remove(path.c_str());
            return false;
        }
    }
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(header_bytes.data(), static_cast<std::streamsize>(header_bytes.size()));
        if (!output) {
            std::remove(path.c_str());
            return false;
        }
    }

    lunanet::gateway4::IqSignal signal;
    lunanet::gateway4::IqFileHeader parsed_header;
    const bool imported = lunanet::gateway4::ImportIqBinaryStandard(
        path, &signal, &parsed_header, &error);
    std::remove(path.c_str());
    return !imported && !error.empty();
}

bool TestExportRejectsInvalidSignalAndPrn() {
    auto signal = MakeSmallSignal();
    signal.q[0] = std::numeric_limits<float>::infinity();
    std::string error;
    if (lunanet::gateway4::ExportIqBinary(
            signal, "test_iq_invalid_export.tmp.iq32", &error)) {
        std::remove("test_iq_invalid_export.tmp.iq32");
        std::cerr << "FAIL [raw export]: non-finite signal was accepted\n";
        return false;
    }

    signal = MakeSmallSignal();
    error.clear();
    if (lunanet::gateway4::ExportIqBinaryStandard(
            signal, 0, "test_iq_invalid_prn.tmp.iq", &error)) {
        std::remove("test_iq_invalid_prn.tmp.iq");
        std::cerr << "FAIL [standard export]: PRN 0 was accepted\n";
        return false;
    }
    return !error.empty();
}

bool TestStandardRejectsInvalidHeaderPrn() {
    const std::string path = "test_iq_bad_prn.tmp.iq";
    std::string error;
    if (!lunanet::gateway4::ExportIqBinaryStandard(MakeSmallSignal(), 7, path, &error)) {
        return false;
    }
    const unsigned char zero_prn[4] = {0, 0, 0, 0};
    if (!OverwriteBytes(path, 28, zero_prn, sizeof(zero_prn))) {
        std::remove(path.c_str());
        return false;
    }

    lunanet::gateway4::IqSignal signal;
    lunanet::gateway4::IqFileHeader header;
    const bool imported =
        lunanet::gateway4::ImportIqBinaryStandard(path, &signal, &header, &error);
    std::remove(path.c_str());
    return !imported && !error.empty();
}

bool TestStandardRejectsOnePairTruncation() {
    const std::string path = "test_iq_one_pair_short.tmp.iq";
    std::string error;
    if (!lunanet::gateway4::ExportIqBinaryStandard(MakeSmallSignal(), 7, path, &error)) {
        return false;
    }

    std::vector<char> shortened(128 + 8, 0);
    {
        std::ifstream input(path, std::ios::binary);
        input.read(shortened.data(), static_cast<std::streamsize>(shortened.size()));
        if (!input) {
            std::remove(path.c_str());
            return false;
        }
    }
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(shortened.data(), static_cast<std::streamsize>(shortened.size()));
        if (!output) {
            std::remove(path.c_str());
            return false;
        }
    }

    lunanet::gateway4::IqSignal signal;
    lunanet::gateway4::IqFileHeader header;
    const bool imported =
        lunanet::gateway4::ImportIqBinaryStandard(path, &signal, &header, &error);
    std::remove(path.c_str());
    return !imported && !error.empty();
}

}  // namespace

int main() {
    bool ok = true;

    if (TestStandardFormatRoundTrip()) {
        std::cout << "PASS: standardized I/Q file header/samples round-trip exactly\n";
    } else {
        ok = false;
    }

    if (TestRejectsBadMagic()) {
        std::cout << "PASS: a file with the wrong magic bytes is rejected\n";
    } else {
        ok = false;
    }

    if (TestStandardRejectsInvalidDuration()) {
        std::cout << "PASS: standardized I/Q rejects a non-finite duration\n";
    } else {
        ok = false;
    }

    if (TestStandardRejectsNonFiniteSample()) {
        std::cout << "PASS: standardized I/Q rejects non-finite samples\n";
    } else {
        ok = false;
    }

    if (TestStandardRejectsEmptyPayload()) {
        std::cout << "PASS: standardized I/Q rejects an empty payload\n";
    } else {
        ok = false;
    }

    if (TestExportRejectsInvalidSignalAndPrn()) {
        std::cout << "PASS: I/Q exporters reject non-finite samples and invalid PRNs\n";
    } else {
        ok = false;
    }

    if (TestStandardRejectsInvalidHeaderPrn()) {
        std::cout << "PASS: standardized I/Q rejects an invalid header PRN\n";
    } else {
        ok = false;
    }

    if (TestStandardRejectsOnePairTruncation()) {
        std::cout << "PASS: standardized I/Q rejects one-pair truncation\n";
    } else {
        ok = false;
    }

    if (TestRawFormatRoundTrip()) {
        std::cout << "PASS: headerless IQ32 samples round-trip exactly\n";
    } else {
        ok = false;
    }

    if (TestRawRejectsPartialPair()) {
        std::cout << "PASS: headerless IQ32 rejects a partial I/Q pair\n";
    } else {
        ok = false;
    }

    return ok ? 0 : 1;
}
