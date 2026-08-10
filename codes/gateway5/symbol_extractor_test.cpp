#include "gateway5/symbol_extractor.h"
#include "gateway2/bch_codec.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool FloatEq(double a, double b, double tol = 1e-9) {
    return std::abs(a - b) <= tol;
}

// ---------------------------------------------------------------------------
// Test 1: LLR formula — worked examples from the Stage 2 spec.
// σ² = 0.25  →  scale = 2/0.25 = 8
// ---------------------------------------------------------------------------
static bool TestLlrFormula() {
    const double sigma2 = 0.25;

    const std::vector<double> received = {+0.9, +0.1, -0.4, -1.2};
    const std::vector<double> expected = {+7.2, +0.8, -3.2, -9.6};

    const auto llrs = lunanet::gateway5::ComputeLlr(received, sigma2);

    if (llrs.size() != expected.size()) {
        std::cerr << "FAIL [LLR formula]: output size " << llrs.size()
                  << " != " << expected.size() << '\n';
        return false;
    }

    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (!FloatEq(llrs[i], expected[i], 1e-9)) {
            std::cerr << "FAIL [LLR formula]: llrs[" << i << "] = " << llrs[i]
                      << ", expected " << expected[i] << '\n';
            return false;
        }
    }

    // Sign convention: positive LLR → bit 0, negative LLR → bit 1
    if (llrs[0] <= 0.0 || llrs[1] <= 0.0) {
        std::cerr << "FAIL [LLR formula]: expected positive LLR for bit-0 inputs\n";
        return false;
    }
    if (llrs[2] >= 0.0 || llrs[3] >= 0.0) {
        std::cerr << "FAIL [LLR formula]: expected negative LLR for bit-1 inputs\n";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Test 2: Symbol slicing — verify counts and offset handling.
// ---------------------------------------------------------------------------
static bool TestSymbolExtraction() {
    // Build a synthetic signal: 8000 linearly increasing values starting at 0.
    const std::size_t total = 8000;
    std::vector<double> signal(total);
    for (std::size_t i = 0; i < total; ++i) {
        signal[i] = static_cast<double>(i);
    }

    const std::size_t offset = 500;
    const auto frame = lunanet::gateway5::ExtractFrameSymbols(signal, offset);

    // Region sizes
    if (static_cast<int>(frame.sp.size()) != lunanet::gateway5::kSpSymbolsStage2) {
        std::cerr << "FAIL [extraction]: sp size = " << frame.sp.size()
                  << ", expected " << lunanet::gateway5::kSpSymbolsStage2 << '\n';
        return false;
    }
    if (static_cast<int>(frame.sb1.size()) != lunanet::gateway5::kSb1SymbolsStage2) {
        std::cerr << "FAIL [extraction]: sb1 size = " << frame.sb1.size()
                  << ", expected " << lunanet::gateway5::kSb1SymbolsStage2 << '\n';
        return false;
    }
    if (static_cast<int>(frame.interleaved.size()) != lunanet::gateway5::kInterleavedSymbols) {
        std::cerr << "FAIL [extraction]: interleaved size = " << frame.interleaved.size()
                  << ", expected " << lunanet::gateway5::kInterleavedSymbols << '\n';
        return false;
    }

    // Values: SP starts at signal[offset], SB1 at signal[offset + 68], etc.
    if (!FloatEq(frame.sp[0], static_cast<double>(offset))) {
        std::cerr << "FAIL [extraction]: sp[0] = " << frame.sp[0]
                  << ", expected " << offset << '\n';
        return false;
    }
    if (!FloatEq(frame.sb1[0], static_cast<double>(offset + lunanet::gateway5::kSpSymbolsStage2))) {
        std::cerr << "FAIL [extraction]: sb1[0] = " << frame.sb1[0]
                  << ", expected " << (offset + lunanet::gateway5::kSpSymbolsStage2) << '\n';
        return false;
    }
    if (!FloatEq(frame.interleaved[0],
                 static_cast<double>(offset + lunanet::gateway5::kInterleavedStart))) {
        std::cerr << "FAIL [extraction]: interleaved[0] = " << frame.interleaved[0]
                  << ", expected " << (offset + lunanet::gateway5::kInterleavedStart) << '\n';
        return false;
    }

    // Reject too-short input
    const std::vector<double> short_signal(100, 0.0);
    const auto empty = lunanet::gateway5::ExtractFrameSymbols(short_signal, 0);
    if (!empty.sp.empty() || !empty.sb1.empty() || !empty.interleaved.empty()) {
        std::cerr << "FAIL [extraction]: expected empty frame for short input\n";
        return false;
    }

    // Regression guard for the frame_offset + kFrameSymbols integer-overflow
    // bug (fixed upstream): a huge frame_offset near SIZE_MAX must be
    // rejected rather than wrapping the bounds check and reading out of
    // bounds.
    const auto huge_offset_result = lunanet::gateway5::ExtractFrameSymbols(
        signal, std::numeric_limits<std::size_t>::max() - 10);
    if (!huge_offset_result.sp.empty() || !huge_offset_result.sb1.empty() ||
        !huge_offset_result.interleaved.empty()) {
        std::cerr << "FAIL [extraction]: expected empty frame for huge frame_offset"
                  << " (overflow regression)\n";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Test 3: BCH round-trip — validation-ladder rung 1.
//
// Encode SB1 value 0x045 → 52 bits.
// Map bits to ±1 BPSK (logic 0 → +1.0, logic 1 → −1.0).
// Scale to LLRs with σ² = 1.0 (so LLR = 2r, i.e. ±2).
// BCH-decode the LLR vector.
// Expect decoded value == 0x045.
// ---------------------------------------------------------------------------
static bool TestBchRoundTrip() {
    constexpr uint16_t kTestValue = 0x045;

    // Encode
    const auto bits = lunanet::gateway2::BchEncode(kTestValue);
    if (static_cast<int>(bits.size()) != lunanet::gateway2::kBchEncodedSymbols) {
        std::cerr << "FAIL [BCH round-trip]: encoded size = " << bits.size() << '\n';
        return false;
    }

    // Map bits to ±1 (BPSK: logic 0 → +1, logic 1 → −1)
    std::vector<double> soft(bits.size());
    for (std::size_t i = 0; i < bits.size(); ++i) {
        soft[i] = (bits[i] == 0) ? +1.0 : -1.0;
    }

    // Convert to LLRs with σ² = 1.0 → LLR = 2r = ±2
    const auto llrs = lunanet::gateway5::ComputeLlr(soft, 1.0);

    // Decode
    const int decoded = lunanet::gateway2::BchDecodeSoft(llrs);
    if (decoded != kTestValue) {
        std::cerr << "FAIL [BCH round-trip]: decoded = 0x"
                  << std::hex << decoded
                  << ", expected 0x" << kTestValue << '\n';
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Test 4: ComputeLlr rejects non-positive noise_variance.
// ---------------------------------------------------------------------------
static bool TestLlrInvalidVariance() {
    const std::vector<double> v = {1.0, -1.0};

    try {
        lunanet::gateway5::ComputeLlr(v, 0.0);
        std::cerr << "FAIL [LLR invalid variance]: expected exception for sigma2=0\n";
        return false;
    } catch (const std::invalid_argument&) {
        // expected
    }

    try {
        lunanet::gateway5::ComputeLlr(v, -1.0);
        std::cerr << "FAIL [LLR invalid variance]: expected exception for sigma2<0\n";
        return false;
    } catch (const std::invalid_argument&) {
        // expected
    }

    try {
        lunanet::gateway5::ComputeLlr(
            v, std::numeric_limits<double>::quiet_NaN());
        std::cerr << "FAIL [LLR invalid variance]: expected exception for NaN\n";
        return false;
    } catch (const std::invalid_argument&) {
        // expected
    }

    try {
        lunanet::gateway5::ComputeLlr(
            {std::numeric_limits<double>::infinity()}, 1.0);
        std::cerr << "FAIL [LLR invalid sample]: expected exception for Infinity\n";
        return false;
    } catch (const std::invalid_argument&) {
        // expected
    }

    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    bool ok = true;

    if (TestLlrFormula()) {
        std::cout << "PASS: LLR formula matches spec worked examples (sigma2=0.25)\n";
        std::cout << "PASS: positive LLR maps to bit 0 (logic 0 → +1 convention)\n";
        std::cout << "PASS: negative LLR maps to bit 1 (logic 1 → −1 convention)\n";
    } else {
        ok = false;
    }

    if (TestSymbolExtraction()) {
        std::cout << "PASS: SP region contains " << lunanet::gateway5::kSpSymbolsStage2
                  << " symbols\n";
        std::cout << "PASS: SB1 region contains " << lunanet::gateway5::kSb1SymbolsStage2
                  << " symbols\n";
        std::cout << "PASS: interleaved block contains "
                  << lunanet::gateway5::kInterleavedSymbols << " symbols\n";
        std::cout << "PASS: frame offset applied correctly\n";
        std::cout << "PASS: short input returns empty frame\n";
    } else {
        ok = false;
    }

    if (TestBchRoundTrip()) {
        std::cout << "PASS: BCH round-trip 0x045 — sign convention is correct\n";
    } else {
        ok = false;
    }

    if (TestLlrInvalidVariance()) {
        std::cout << "PASS: ComputeLlr rejects invalid variance and samples\n";
    } else {
        ok = false;
    }

    return ok ? 0 : 1;
}
