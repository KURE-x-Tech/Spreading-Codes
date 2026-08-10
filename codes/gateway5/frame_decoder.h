#pragma once
#ifndef LUNANET_GATEWAY5_FRAME_DECODER_H
#define LUNANET_GATEWAY5_FRAME_DECODER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gateway1/spreading_config.h"
#include "gateway2/ldpc_encoder.h"
#include "gateway4/iq_generator.h"
#include "gateway5/bch_soft_decoder.h"
#include "gateway5/crc_validator.h"
#include "gateway5/ldpc_decoder.h"
#include "gateway5/sync_detector.h"

namespace lunanet::gateway5 {

struct DecoderMatrices {
    lunanet::gateway2::LdpcMatrices sb2;
    lunanet::gateway2::BinaryMatrix sb2_b;
    lunanet::gateway2::LdpcMatrices sb34;
    lunanet::gateway2::BinaryMatrix sb34_b;
};

struct FrameDecoderConfig {
    int prn = 0;
    double symbol_noise_variance = 1.0;
    double sync_psr_threshold = kDefaultSyncPsrThreshold;
    double sync_peak_to_rms_threshold = kDefaultSyncPeakToRmsThreshold;
    double sync_normalized_peak_threshold = kDefaultSyncNormalizedPeakThreshold;
    double lock_threshold = 0.5;
    double bch_min_normalized_correlation = kDefaultBchMinNormalizedCorrelation;
    double bch_min_normalized_margin = kDefaultBchMinNormalizedMargin;
    int max_ldpc_iterations = 50;
    double ldpc_alpha = 0.75;
};

struct FrameDecodeResult {
    bool accepted = false;
    std::string error;

    std::size_t code_phase = 0;
    double lock_correlation = 0.0;
    FrameSyncResult sync;

    BchSoftDecodeResult sb1_bch;
    int sb1_value = -1;
    uint8_t fid = 0;
    uint8_t toi = 0;

    LdpcDecodeResult sb2_ldpc;
    LdpcDecodeResult sb3_ldpc;
    LdpcDecodeResult sb4_ldpc;
    FrameCrcVerdict crc;

    std::vector<uint8_t> sb2_payload;
    std::vector<uint8_t> sb3_payload;
    std::vector<uint8_t> sb4_payload;

    double elapsed_ms = 0.0;
};

bool LoadDecoderMatrices(const std::string& annex3_csv_dir,
                         DecoderMatrices* out,
                         std::string* error_message = nullptr);

FrameDecodeResult DecodeDespreadSymbols(
    const std::vector<double>& symbols,
    const DecoderMatrices& matrices,
    const FrameDecoderConfig& config);

FrameDecodeResult DecodeAfsIChipStream(
    const std::vector<double>& chip_stream,
    const lunanet::gateway1::SpreadingSpecTables& spreading_tables,
    const DecoderMatrices& matrices,
    const FrameDecoderConfig& config);

FrameDecodeResult DecodeAfsIIqSignal(
    const lunanet::gateway4::IqSignal& signal,
    const lunanet::gateway1::SpreadingSpecTables& spreading_tables,
    const DecoderMatrices& matrices,
    const FrameDecoderConfig& config);

}  // namespace lunanet::gateway5

#endif  // LUNANET_GATEWAY5_FRAME_DECODER_H