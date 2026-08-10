#include "gateway5/frame_decoder.h"

#include "gateway3/frame_assembler.h"
#include "gateway4/signal_config.h"
#include "gateway5/bch_soft_decoder.h"
#include "gateway5/deinterleaver.h"
#include "gateway5/despreader.h"
#include "gateway5/symbol_extractor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>

namespace lunanet::gateway5 {
namespace {

using Clock = std::chrono::steady_clock;

double ElapsedMilliseconds(const Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

bool ValidateConfig(const FrameDecoderConfig& config, std::string* error) {
    if (config.prn < 1 || config.prn > lunanet::gateway1::kMaxPrns) {
        *error = "PRN must be in the range 1-" +
            std::to_string(lunanet::gateway1::kMaxPrns);
        return false;
    }
    if (!std::isfinite(config.symbol_noise_variance) ||
        config.symbol_noise_variance <= 0.0) {
        *error = "symbol_noise_variance must be finite and > 0";
        return false;
    }
    if (!std::isfinite(config.sync_psr_threshold) ||
        config.sync_psr_threshold <= 0.0) {
        *error = "sync_psr_threshold must be finite and > 0";
        return false;
    }
    if (!std::isfinite(config.lock_threshold) ||
        config.lock_threshold <= 0.0 || config.lock_threshold > 1.0) {
        *error = "lock_threshold must be finite and in (0, 1]";
        return false;
    }
    if (config.max_ldpc_iterations <= 0 || config.max_ldpc_iterations > 50) {
        *error = "max_ldpc_iterations must be in the range 1-50";
        return false;
    }
    if (!std::isfinite(config.ldpc_alpha) ||
        config.ldpc_alpha <= 0.0 || config.ldpc_alpha > 1.0) {
        *error = "ldpc_alpha must be finite and in (0, 1]";
        return false;
    }
    return true;
}

bool AllFinite(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(), [](const double value) {
        return std::isfinite(value);
    });
}

std::vector<uint8_t> StripCrc24(const std::vector<uint8_t>& systematic_bits) {
    constexpr std::size_t kCrcBits = 24;
    if (systematic_bits.size() < kCrcBits) {
        return {};
    }
    return {systematic_bits.begin(), systematic_bits.end() - kCrcBits};
}

FrameDecodeResult DecodeSymbolsImpl(
    const std::vector<double>& symbols,
    const DecoderMatrices& matrices,
    const FrameDecoderConfig& config,
    const Clock::time_point start) {
    FrameDecodeResult result;
    const auto finish = [&](const std::string& error = std::string()) {
        if (!error.empty()) {
            result.error = error;
        }
        result.elapsed_ms = ElapsedMilliseconds(start);
        return result;
    };

    if (!ValidateConfig(config, &result.error)) {
        return finish();
    }
    if (!AllFinite(symbols)) {
        return finish("Despread symbol stream contains a non-finite value");
    }

    result.sync = DetectFrameSync(symbols, config.sync_psr_threshold);
    if (!result.sync.detected) {
        return finish("Frame synchronization failed: no peak cleared the PSR threshold");
    }

    const ExtractedFrame frame = ExtractFrameSymbols(symbols, result.sync.frame_offset);
    if (frame.sp.empty()) {
        return finish("Frame synchronization succeeded but fewer than 6000 symbols remain");
    }

    result.sb1_value = DecodeSb1BchSoft(frame.sb1);
    if (result.sb1_value < 0) {
        return finish("SB1 BCH decoder rejected the extracted symbol count");
    }
    result.fid = static_cast<uint8_t>((result.sb1_value >> 7) & 0x3);
    result.toi = static_cast<uint8_t>(result.sb1_value & 0x7F);

    const Stage4Subframes soft = DeinterleaveToSubframes(frame.interleaved);
    if (soft.sb2.empty() || soft.sb3.empty() || soft.sb4.empty()) {
        return finish("Deinterleaver rejected the extracted interleaved block");
    }

    const auto sb2_llrs = ComputeLlr(soft.sb2, config.symbol_noise_variance);
    const auto sb3_llrs = ComputeLlr(soft.sb3, config.symbol_noise_variance);
    const auto sb4_llrs = ComputeLlr(soft.sb4, config.symbol_noise_variance);

    std::string stage_error;
    result.sb2_ldpc = DecodeLdpcMinSum(
        sb2_llrs, matrices.sb2, matrices.sb2_b, lunanet::gateway2::kLdpcSb2,
        config.max_ldpc_iterations, config.ldpc_alpha, &stage_error);
    if (!result.sb2_ldpc.converged) {
        return finish("SB2 LDPC decode failed: " +
            (stage_error.empty() ? "did not converge" : stage_error));
    }

    stage_error.clear();
    result.sb3_ldpc = DecodeLdpcMinSum(
        sb3_llrs, matrices.sb34, matrices.sb34_b, lunanet::gateway2::kLdpcSb34,
        config.max_ldpc_iterations, config.ldpc_alpha, &stage_error);
    if (!result.sb3_ldpc.converged) {
        return finish("SB3 LDPC decode failed: " +
            (stage_error.empty() ? "did not converge" : stage_error));
    }

    stage_error.clear();
    result.sb4_ldpc = DecodeLdpcMinSum(
        sb4_llrs, matrices.sb34, matrices.sb34_b, lunanet::gateway2::kLdpcSb34,
        config.max_ldpc_iterations, config.ldpc_alpha, &stage_error);
    if (!result.sb4_ldpc.converged) {
        return finish("SB4 LDPC decode failed: " +
            (stage_error.empty() ? "did not converge" : stage_error));
    }

    result.crc = ValidateFrameCrc(
        result.sb2_ldpc.decoded_data_bits,
        result.sb3_ldpc.decoded_data_bits,
        result.sb4_ldpc.decoded_data_bits);
    if (!result.crc.frame_accepted) {
        return finish("CRC-24 validation rejected one or more decoded subframes");
    }

    result.sb2_payload = StripCrc24(result.sb2_ldpc.decoded_data_bits);
    result.sb3_payload = StripCrc24(result.sb3_ldpc.decoded_data_bits);
    result.sb4_payload = StripCrc24(result.sb4_ldpc.decoded_data_bits);
    if (result.sb2_payload.size() != 1176u ||
        result.sb3_payload.size() != 846u ||
        result.sb4_payload.size() != 846u) {
        return finish("CRC-stripped payload lengths do not match the Gateway 6 contract");
    }

    result.accepted = true;
    return finish();
}

}  // namespace

bool LoadDecoderMatrices(const std::string& annex3_csv_dir,
                         DecoderMatrices* out,
                         std::string* error_message) {
    if (out == nullptr) {
        if (error_message) *error_message = "DecoderMatrices output must not be null";
        return false;
    }

    lunanet::gateway3::FrameMatrices frame_matrices;
    if (!lunanet::gateway3::LoadFrameMatrices(
            annex3_csv_dir, &frame_matrices, error_message)) {
        return false;
    }

    const std::filesystem::path csv_dir(annex3_csv_dir);
    DecoderMatrices loaded;
    loaded.sb2 = std::move(frame_matrices.sb2);
    loaded.sb34 = std::move(frame_matrices.sb34);

    if (!lunanet::gateway2::LoadBinaryMatrixCsv(
            (csv_dir / "004j_lunanet_sf2_ldpc_submatrix_b_mat.csv").string(),
            &loaded.sb2_b, error_message)) {
        return false;
    }
    if (!lunanet::gateway2::LoadBinaryMatrixCsv(
            (csv_dir / "004c_lunanet_sf3_ldpc_submatrix_b_mat.csv").string(),
            &loaded.sb34_b, error_message)) {
        return false;
    }

    *out = std::move(loaded);
    return true;
}

FrameDecodeResult DecodeDespreadSymbols(
    const std::vector<double>& symbols,
    const DecoderMatrices& matrices,
    const FrameDecoderConfig& config) {
    return DecodeSymbolsImpl(symbols, matrices, config, Clock::now());
}

FrameDecodeResult DecodeAfsIChipStream(
    const std::vector<double>& chip_stream,
    const lunanet::gateway1::SpreadingSpecTables& spreading_tables,
    const DecoderMatrices& matrices,
    const FrameDecoderConfig& config) {
    const auto start = Clock::now();
    FrameDecodeResult result;
    if (!ValidateConfig(config, &result.error)) {
        result.elapsed_ms = ElapsedMilliseconds(start);
        return result;
    }
    if (!AllFinite(chip_stream)) {
        result.error = "AFS-I chip stream contains a non-finite value";
        result.elapsed_ms = ElapsedMilliseconds(start);
        return result;
    }

    std::string error;
    const DespreadResult despread = DespreadAfsI(
        chip_stream, config.prn, spreading_tables, config.lock_threshold, &error);
    if (!despread.locked) {
        result.error = "AFS-I de-spreading failed: " + error;
        result.lock_correlation = despread.lock_correlation;
        result.elapsed_ms = ElapsedMilliseconds(start);
        return result;
    }

    result = DecodeSymbolsImpl(despread.symbols, matrices, config, start);
    result.code_phase = despread.code_phase;
    result.lock_correlation = despread.lock_correlation;
    return result;
}

FrameDecodeResult DecodeAfsIIqSignal(
    const lunanet::gateway4::IqSignal& signal,
    const lunanet::gateway1::SpreadingSpecTables& spreading_tables,
    const DecoderMatrices& matrices,
    const FrameDecoderConfig& config) {
    const auto start = Clock::now();
    FrameDecodeResult result;

    if (signal.i.empty()) {
        result.error = "I/Q signal has no in-phase samples";
        return result;
    }
    if (!signal.q.empty() && signal.q.size() != signal.i.size()) {
        result.error = "I/Q signal has mismatched channel lengths";
        return result;
    }
    if (signal.sample_rate_hz <= 0 ||
        signal.sample_rate_hz % lunanet::gateway4::kAfsIChipRateHz != 0) {
        result.error = "I/Q sample rate must be a positive integer multiple of " +
            std::to_string(lunanet::gateway4::kAfsIChipRateHz) + " Hz";
        return result;
    }

    const std::size_t samples_per_chip = static_cast<std::size_t>(
        signal.sample_rate_hz / lunanet::gateway4::kAfsIChipRateHz);
    if (signal.i.size() % samples_per_chip != 0u) {
        result.error = "I/Q sample count does not contain a whole number of AFS-I chips";
        return result;
    }

    std::vector<double> chip_stream;
    chip_stream.reserve(signal.i.size() / samples_per_chip);
    for (std::size_t start_sample = 0;
         start_sample < signal.i.size();
         start_sample += samples_per_chip) {
        double sum = 0.0;
        for (std::size_t offset = 0; offset < samples_per_chip; ++offset) {
            const double sample = static_cast<double>(signal.i[start_sample + offset]);
            if (!std::isfinite(sample)) {
                result.error = "I/Q signal contains a non-finite in-phase sample";
                result.elapsed_ms = ElapsedMilliseconds(start);
                return result;
            }
            sum += sample;
        }
        chip_stream.push_back(sum / static_cast<double>(samples_per_chip));
    }

    result = DecodeAfsIChipStream(chip_stream, spreading_tables, matrices, config);
    result.elapsed_ms = ElapsedMilliseconds(start);
    return result;
}

}  // namespace lunanet::gateway5