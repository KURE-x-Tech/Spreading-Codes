#include "spreading_codes.h"

#include "testing/test_annex3_loader.h"
#include "testing/test_reporter.h"
#include "testing/test_validators.h"

#include "gateway1/spreading_config.h"
#include "gateway2/bch_codec.h"
#include "gateway2/crc24.h"
#include "gateway2/interleaver.h"
#include "gateway2/ldpc_encoder.h"
#include "gateway3/frame_assembler.h"
#include "gateway3/frame_config.h"
#include "gateway3/frame_exporter.h"
#include "gateway3/subframe1_builder.h"
#include "gateway3/subframe2_builder.h"
#include "gateway3/subframe3_builder.h"
#include "gateway3/subframe4_builder.h"
#include "gateway3/sync_pattern.h"
#include "gateway4/bpsk_modulator.h"
#include "gateway4/iq_generator.h"
#include "gateway4/signal_config.h"
#include "gateway4/signal_exporter.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class GatewaySelection {
    kAll,
    kGateway1,
    kGateway2,
    kGateway3,
    kGateway4,
    kGateway5,
};

struct RunOptions {
    std::string config_override;
    std::string reports_base_override;
    GatewaySelection gateway = GatewaySelection::kAll;
    bool show_help = false;
    std::string parse_error;
};

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

bool ParseGatewaySelection(const std::string& value, GatewaySelection* selection) {
    const std::string normalized = ToLowerAscii(value);
    if (normalized == "all") {
        *selection = GatewaySelection::kAll;
        return true;
    }
    if (normalized == "gateway1" || normalized == "g1" || normalized == "1") {
        *selection = GatewaySelection::kGateway1;
        return true;
    }
    if (normalized == "gateway2" || normalized == "g2" || normalized == "2") {
        *selection = GatewaySelection::kGateway2;
        return true;
    }
    if (normalized == "gateway3" || normalized == "g3" || normalized == "3") {
        *selection = GatewaySelection::kGateway3;
        return true;
    }
    if (normalized == "gateway4" || normalized == "g4" || normalized == "4") {
        *selection = GatewaySelection::kGateway4;
        return true;
    }
    if (normalized == "gateway5" || normalized == "g5" || normalized == "5") {
        *selection = GatewaySelection::kGateway5;
        return true;
    }
    return false;
}

std::string GatewayScopeName(GatewaySelection selection) {
    switch (selection) {
        case GatewaySelection::kGateway1: return "gateway1";
        case GatewaySelection::kGateway2: return "gateway2";
        case GatewaySelection::kGateway3: return "gateway3";
        case GatewaySelection::kGateway4: return "gateway4";
        case GatewaySelection::kGateway5: return "gateway5";
        case GatewaySelection::kAll:
        default: return "all";
    }
}

void PrintUsage(std::ostream& out, const char* exe_name) {
    out << "Usage: " << exe_name
        << " [config_path] [reports_base] [--gateway <all|gateway1|gateway2|gateway3|gateway4|gateway5>]" << std::endl;
    out << "       " << exe_name
        << " [--gateway <all|gateway1|gateway2|gateway3|gateway4|gateway5>] [config_path] [reports_base]" << std::endl;
    out << std::endl;
    out << "Options:" << std::endl;
    out << "  --gateway, -g  Validation scope to run (default: all)." << std::endl;
    out << "  --help, -h     Show this help message." << std::endl;
}

RunOptions ParseRunOptions(int argc, char** argv) {
    RunOptions options;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
            return options;
        }

        if (arg == "--gateway" || arg == "-g") {
            if (i + 1 >= argc) {
                options.parse_error = "Missing value for --gateway";
                return options;
            }
            const std::string value = argv[++i];
            if (!ParseGatewaySelection(value, &options.gateway)) {
                options.parse_error = "Invalid --gateway value: '" + value + "'";
                return options;
            }
            continue;
        }

        if (arg.rfind("--gateway=", 0) == 0) {
            const std::string value = arg.substr(std::string("--gateway=").size());
            if (!ParseGatewaySelection(value, &options.gateway)) {
                options.parse_error = "Invalid --gateway value: '" + value + "'";
                return options;
            }
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            options.parse_error = "Unknown option: " + arg;
            return options;
        }

        positional.push_back(arg);
    }

    if (positional.size() > 2) {
        options.parse_error = "Too many positional arguments";
        return options;
    }

    if (!positional.empty()) {
        options.config_override = positional[0];
    }
    if (positional.size() == 2) {
        options.reports_base_override = positional[1];
    }

    return options;
}

std::string FindConfigPath(const char* override_path) {
    if (override_path != nullptr && std::string(override_path).size() > 0) {
        return override_path;
    }

    std::filesystem::path current = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        const std::filesystem::path candidate = current / "config" / "spreading_codes_config.ini";
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return "config/spreading_codes_config.ini";
}

void RunSmokeTests(lunanet::testing::TestReporter& reporter) {
    const std::string kSuite = "Smoke";

    lunanet::testing::ValidateCondition(
        kSuite, "hello_moon response",
        std::string(lunanet::hello_moon()) == "Hello Moon",
        "Expected 'Hello Moon'", reporter);

    const std::vector<uint8_t> gold = lunanet::generate_gold_code(1);
    lunanet::testing::ValidateCodeLength(
        kSuite, "Gold code length PRN 1",
        gold, lunanet::GOLD_CODE_LENGTH, reporter);

    const std::vector<uint8_t> weil_primary = lunanet::generate_weil_primary(1);
    lunanet::testing::ValidateCodeLength(
        kSuite, "Weil primary length PRN 1",
        weil_primary,
        static_cast<size_t>(lunanet::WEIL_PRIMARY_PRIME + lunanet::EXPANSION_LENGTH),
        reporter);

    const std::vector<uint8_t> weil_tertiary = lunanet::generate_weil_tertiary(1);
    lunanet::testing::ValidateCodeLength(
        kSuite, "Weil tertiary length PRN 1",
        weil_tertiary, lunanet::WEIL_TERTIARY_LENGTH, reporter);

    const std::vector<uint8_t> afs_q = lunanet::generate_afs_q(1);
    lunanet::testing::ValidateNonEmpty(kSuite, "AFS-Q generated PRN 1", afs_q, reporter);

    const size_t afs_q_cap = lunanet::get_afs_q_max_chips();
    if (afs_q_cap > 0) {
        lunanet::testing::ValidateCondition(
            kSuite, "AFS-Q respects configured max chips",
            afs_q.size() <= afs_q_cap,
            "Got " + std::to_string(afs_q.size()) + " chips, cap is " + std::to_string(afs_q_cap),
            reporter);
    }

    const std::vector<uint8_t> afs_i = lunanet::generate_afs_i(1);
    lunanet::testing::ValidateEquality(
        kSuite, "AFS-I deterministic repeatability", afs_i, gold, reporter);

    lunanet::testing::ValidateBoundsRejection(
        kSuite, "Invalid PRN 211 returns empty",
        [](int prn) { return lunanet::generate_gold_code(prn); }, 211, reporter);

    const auto all_codes = lunanet::generate_all_spreading_codes();
    lunanet::testing::ValidateCondition(
        kSuite, "Batch generation for all PRNs",
        all_codes.size() == static_cast<size_t>(lunanet::MAX_PRNS),
        "Got " + std::to_string(all_codes.size()) + " PRNs", reporter);

    if (!all_codes.empty()) {
        const auto it = all_codes.find(1);
        lunanet::testing::ValidateCondition(
            kSuite, "PRN 1 exists in batch output",
            it != all_codes.end(), "PRN 1 missing from batch", reporter);
        if (it != all_codes.end()) {
            lunanet::testing::ValidateCodeLength(
                kSuite, "Batch AFS-I length",
                it->second.first, lunanet::GOLD_CODE_LENGTH, reporter);
            lunanet::testing::ValidateNonEmpty(
                kSuite, "Batch AFS-Q non-empty",
                it->second.second, reporter);
        }
    }
}

bool RunAnnex3Validation(
    const std::filesystem::path& annex3_dir,
    lunanet::testing::TestReporter& reporter) {

    std::vector<std::string> gold_hex;
    std::vector<std::string> weil_primary_hex;
    std::vector<std::string> weil_tertiary_hex;
    std::string error;

    if (!lunanet::testing::LoadAnnex3HexLines(
            annex3_dir / "006_GoldCode2046hex210prns.txt",
            lunanet::MAX_PRNS, gold_hex, error)) {
        reporter.Record({"Annex3/Gold", "Load reference file",
            lunanet::testing::TestStatus::kFail, error, 0.0});
        return false;
    }

    if (!lunanet::testing::LoadAnnex3HexLines(
            annex3_dir / "007_l1cp_hex210prns.txt",
            lunanet::MAX_PRNS, weil_primary_hex, error)) {
        reporter.Record({"Annex3/Weil Primary", "Load reference file",
            lunanet::testing::TestStatus::kFail, error, 0.0});
        return false;
    }

    if (!lunanet::testing::LoadAnnex3HexLines(
            annex3_dir / "008_Weil1500hex210prns.txt",
            lunanet::MAX_PRNS, weil_tertiary_hex, error)) {
        reporter.Record({"Annex3/Weil Tertiary", "Load reference file",
            lunanet::testing::TestStatus::kFail, error, 0.0});
        return false;
    }

    lunanet::testing::ValidateAnnex3Suite(
        "Annex3/Gold", gold_hex,
        lunanet::GOLD_CODE_LENGTH, lunanet::MAX_PRNS,
        [](int prn) { return lunanet::generate_gold_code(prn); },
        reporter);

    lunanet::testing::ValidateAnnex3Suite(
        "Annex3/Weil Primary", weil_primary_hex,
        static_cast<size_t>(lunanet::WEIL_PRIMARY_PRIME + lunanet::EXPANSION_LENGTH),
        lunanet::MAX_PRNS,
        [](int prn) { return lunanet::generate_weil_primary(prn); },
        reporter);

    lunanet::testing::ValidateAnnex3Suite(
        "Annex3/Weil Tertiary", weil_tertiary_hex,
        lunanet::WEIL_TERTIARY_LENGTH, lunanet::MAX_PRNS,
        [](int prn) { return lunanet::generate_weil_tertiary(prn); },
        reporter);

    return true;
}

void RunPerformanceBenchmarks(lunanet::testing::TestReporter& reporter) {
    using Clock = std::chrono::high_resolution_clock;
    const std::string kSuite = "Performance";

    for (int prn : {1, 105, 210}) {
        const auto start = Clock::now();

        static_cast<void>(lunanet::generate_gold_code(prn));
        static_cast<void>(lunanet::generate_weil_primary(prn));
        static_cast<void>(lunanet::generate_weil_tertiary(prn));
        static_cast<void>(lunanet::generate_afs_q(prn));

        const double elapsed_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - start).count();

        reporter.Record({kSuite,
            "PRN " + std::to_string(prn) + " full generation",
            elapsed_ms < 1000.0
                ? lunanet::testing::TestStatus::kPass
                : lunanet::testing::TestStatus::kFail,
            std::to_string(elapsed_ms) + " ms",
            elapsed_ms});
    }
}

void RunTable11Validation(
    const std::string& config_path,
    lunanet::testing::TestReporter& reporter) {

    lunanet::gateway1::SpreadingSpecTables tables;
    lunanet::gateway1::Annex3Paths annex3;
    std::string error;

    if (!lunanet::gateway1::LoadSpreadingConfig(config_path, &tables, &annex3, &error)) {
        reporter.Record({"Table11/Assignments", "Load config",
            lunanet::testing::TestStatus::kFail, error, 0.0});
        return;
    }

    // Table 11 defines nodes 1-12 with PRN identity mapping and
    // secondary code cycling S0, S1, S2, S3, S0, S1, S2, S3, ...
    std::vector<lunanet::testing::Table11Entry> entries;
    for (int node = 1; node <= 12; ++node) {
        entries.push_back({
            node,
            node,                     // prn_i = node_id
            node,                     // prn_q = node_id
            (node - 1) % 4,           // S0, S1, S2, S3 cycling
            node,                     // tertiary_prn = node_id
            0,                        // phase_offset = 0
        });
    }

    lunanet::testing::ValidateTable11Suite(
        entries,
        tables.secondary_code_by_prn,
        tables.secondary_codes,
        [](int prn) { return lunanet::generate_gold_code(prn); },
        [](int prn) { return lunanet::generate_weil_primary(prn); },
        [](int prn) { return lunanet::generate_weil_tertiary(prn); },
        [](int prn) { return lunanet::generate_afs_q(prn); },
        reporter);
}

void RunGateway2Tests(lunanet::testing::TestReporter& reporter) {
    const std::string kBch = "Gateway2/BCH";
    const std::string kCrc = "Gateway2/CRC24";
    const std::string kIl = "Gateway2/Interleaver";

    // ── BCH(51,8) Tests ─────────────────────────────────────────────────

    // Test: encode produces 52 symbols
    {
        const auto encoded = lunanet::gateway2::BchEncode(0x000);  // FID=0, TOI=0
        lunanet::testing::ValidateCondition(
            kBch, "Encode length (SB1=0x000)",
            encoded.size() == 52,
            "Got " + std::to_string(encoded.size()) + " symbols, expected 52",
            reporter);
    }

    // Test: all encoded symbols are binary
    {
        const auto encoded = lunanet::gateway2::BchEncode(0x0A5);
        bool all_binary = true;
        for (const auto s : encoded) {
            if (s > 1) { all_binary = false; break; }
        }
        lunanet::testing::ValidateCondition(
            kBch, "Symbols are binary (SB1=0x0A5)",
            all_binary, "Non-binary symbol found", reporter);
    }

    // Test: round-trip encode → soft → decode recovers original
    for (uint16_t sb1 : {0x000, 0x001, 0x07F, 0x080, 0x0FF, 0x100, 0x1FF}) {
        const auto encoded = lunanet::gateway2::BchEncode(sb1);

        // Convert to soft decisions: 0 → +1.0, 1 → -1.0
        std::vector<double> soft(encoded.begin(), encoded.end());
        for (auto& s : soft) { s = (s == 0) ? 1.0 : -1.0; }

        const int decoded = lunanet::gateway2::BchDecodeSoft(soft);
        lunanet::testing::ValidateCondition(
            kBch, "Round-trip SB1=0x" +
                ([](uint16_t v) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%03X", v);
                    return std::string(buf);
                })(sb1),
            decoded == sb1,
            "Decoded 0x" + std::to_string(decoded) +
            ", expected 0x" + std::to_string(sb1),
            reporter);
    }

    // Test: MSB=1 case (FID=2, TOI=0 → SB1=0x100)
    {
        const auto encoded = lunanet::gateway2::BchEncode(0x100);
        lunanet::testing::ValidateCondition(
            kBch, "MSB=1 prepended (SB1=0x100)",
            !encoded.empty() && encoded[0] == 1,
            "First symbol should be 1 for MSB=1", reporter);
    }

    // ── CRC-24 Tests ────────────────────────────────────────────────────

    // Test: CRC of empty data
    {
        std::vector<uint8_t> data;
        const uint32_t crc = lunanet::gateway2::Crc24Compute(data);
        lunanet::testing::ValidateCondition(
            kCrc, "Empty data CRC",
            crc == 0x000000,
            "CRC of empty data should be 0, got 0x" + std::to_string(crc),
            reporter);
    }

    // Test: Append + Verify round-trip
    {
        std::vector<uint8_t> data = {1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1};
        lunanet::gateway2::Crc24Append(data);
        lunanet::testing::ValidateCondition(
            kCrc, "Append grows by 24 bits",
            data.size() == 12 + 24,
            "Size " + std::to_string(data.size()), reporter);

        lunanet::testing::ValidateCondition(
            kCrc, "Verify after append",
            lunanet::gateway2::Crc24Verify(data),
            "CRC verification failed on valid data", reporter);
    }

    // Test: Corruption detection
    {
        std::vector<uint8_t> data = {0, 1, 0, 1, 0, 1, 0, 1};
        lunanet::gateway2::Crc24Append(data);
        data[3] ^= 1;  // Flip one bit
        lunanet::testing::ValidateCondition(
            kCrc, "Detect single bit corruption",
            !lunanet::gateway2::Crc24Verify(data),
            "CRC should fail after corruption", reporter);
    }

    // ── Block Interleaver Tests ──────────────────────────────────────────

    // Test: round-trip
    {
        std::vector<uint8_t> data(lunanet::gateway2::kInterleaverSize);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<uint8_t>(i % 2);
        }

        const auto interleaved = lunanet::gateway2::Interleave(data);
        lunanet::testing::ValidateCondition(
            kIl, "Interleave output size",
            interleaved.size() == lunanet::gateway2::kInterleaverSize,
            "Got " + std::to_string(interleaved.size()), reporter);

        const auto deinterleaved = lunanet::gateway2::Deinterleave(interleaved);
        lunanet::testing::ValidateCondition(
            kIl, "Round-trip (interleave→deinterleave)",
            deinterleaved == data,
            "Data mismatch after round-trip", reporter);
    }

    // Test: interleaver rejects wrong size
    {
        std::vector<uint8_t> bad(100, 0);
        lunanet::testing::ValidateCondition(
            kIl, "Reject invalid size",
            lunanet::gateway2::Interleave(bad).empty(),
            "Should return empty for non-5880 input", reporter);
    }

    // Test: interleaving actually permutes data
    {
        std::vector<uint8_t> seq(lunanet::gateway2::kInterleaverSize, 0);
        seq[1] = 1;  // Single non-zero element at position 1
        const auto interleaved = lunanet::gateway2::Interleave(seq);
        lunanet::testing::ValidateCondition(
            kIl, "Permutation changes positions",
            interleaved != seq,
            "Interleaved output should differ from input", reporter);
    }
}

void RunLdpcTests(
    const std::filesystem::path& repo_root,
    lunanet::testing::TestReporter& reporter) {

    const std::string kSuite = "Gateway2/LDPC";
    const std::filesystem::path csv_dir = repo_root / "Validation" / "annex3" / "csv";

    // ── Load SB2 matrices ────────────────────────────────────────────────
    lunanet::gateway2::LdpcMatrices sb2_matrices;
    std::string error;

    using Clock = std::chrono::high_resolution_clock;
    auto load_start = Clock::now();

    const bool sb2_loaded = lunanet::gateway2::LoadLdpcMatrices(
        (csv_dir / "004h_lunanet_sf2_ldpc_submatrix_a_mat.csv").string(),
        (csv_dir / "004i_lunanet_sf2_ldpc_submatrix_b_inv_mat.csv").string(),
        (csv_dir / "004f_lunanet_sf2_ldpc_submatrix_c_mat.csv").string(),
        (csv_dir / "004g_lunanet_sf2_ldpc_submatrix_d_mat.csv").string(),
        &sb2_matrices, &error);

    double load_ms = std::chrono::duration<double, std::milli>(Clock::now() - load_start).count();

    lunanet::testing::ValidateConditionTimed(
        kSuite, "Load SB2 submatrices",
        sb2_loaded, error, load_ms, reporter);

    if (sb2_loaded) {
        // Validate SB2 matrix dimensions
        lunanet::testing::ValidateCondition(
            kSuite, "SB2 A dimensions (480x1200)",
            sb2_matrices.a.rows == 480 && sb2_matrices.a.cols == 1200,
            "A: " + std::to_string(sb2_matrices.a.rows) + "x" + std::to_string(sb2_matrices.a.cols),
            reporter);

        lunanet::testing::ValidateCondition(
            kSuite, "SB2 B_inv dimensions (480x480)",
            sb2_matrices.b_inv.rows == 480 && sb2_matrices.b_inv.cols == 480,
            "B_inv: " + std::to_string(sb2_matrices.b_inv.rows) + "x" + std::to_string(sb2_matrices.b_inv.cols),
            reporter);

        // Encode SB2 test data (all-zeros)
        std::vector<uint8_t> sb2_data(lunanet::gateway2::kLdpcSb2.data_bits, 0);
        auto enc_start = Clock::now();
        const auto sb2_encoded = lunanet::gateway2::LdpcEncode(
            sb2_data, sb2_matrices, lunanet::gateway2::kLdpcSb2, &error);
        double enc_ms = std::chrono::duration<double, std::milli>(Clock::now() - enc_start).count();

        lunanet::testing::ValidateConditionTimed(
            kSuite, "SB2 encode output length (2400)",
            static_cast<int>(sb2_encoded.size()) == lunanet::gateway2::kLdpcSb2.output_symbols,
            "Got " + std::to_string(sb2_encoded.size()) + (error.empty() ? "" : ": " + error),
            enc_ms, reporter);

        // Verify all symbols are binary
        if (!sb2_encoded.empty()) {
            bool all_binary = true;
            for (const auto s : sb2_encoded) {
                if (s > 1) { all_binary = false; break; }
            }
            lunanet::testing::ValidateCondition(
                kSuite, "SB2 encoded symbols are binary",
                all_binary, "Non-binary symbol found", reporter);
        }

        // Encode SB2 with non-zero data
        std::vector<uint8_t> sb2_data2(lunanet::gateway2::kLdpcSb2.data_bits, 0);
        for (size_t i = 0; i < sb2_data2.size(); i += 3) sb2_data2[i] = 1;
        const auto sb2_enc2 = lunanet::gateway2::LdpcEncode(
            sb2_data2, sb2_matrices, lunanet::gateway2::kLdpcSb2, &error);
        lunanet::testing::ValidateCondition(
            kSuite, "SB2 encode non-zero data",
            static_cast<int>(sb2_enc2.size()) == lunanet::gateway2::kLdpcSb2.output_symbols,
            "Got " + std::to_string(sb2_enc2.size()),
            reporter);

        // SB2 performance check (< 100ms)
        lunanet::testing::ValidateCondition(
            kSuite, "SB2 encode < 100ms",
            enc_ms < 100.0,
            std::to_string(enc_ms) + " ms", reporter);
    }

    // ── Load SB3/SB4 matrices ─────────────────────────────────────────────
    lunanet::gateway2::LdpcMatrices sb3_matrices;
    error.clear();

    load_start = Clock::now();
    const bool sb3_loaded = lunanet::gateway2::LoadLdpcMatrices(
        (csv_dir / "004a_lunanet_sf3_ldpc_submatrix_a_mat.csv").string(),
        (csv_dir / "004b_lunanet_sf3_ldpc_submatrix_b_inv_mat.csv").string(),
        (csv_dir / "004d_lunanet_sf3_ldpc_submatrix_c_mat.csv").string(),
        (csv_dir / "004e_lunanet_sf3_ldpc_submatrix_d_mat.csv").string(),
        &sb3_matrices, &error);
    load_ms = std::chrono::duration<double, std::milli>(Clock::now() - load_start).count();

    lunanet::testing::ValidateConditionTimed(
        kSuite, "Load SB3/SB4 submatrices",
        sb3_loaded, error, load_ms, reporter);

    if (sb3_loaded) {
        // Validate SB3 matrix dimensions
        lunanet::testing::ValidateCondition(
            kSuite, "SB3 A dimensions (352x880)",
            sb3_matrices.a.rows == 352 && sb3_matrices.a.cols == 880,
            "A: " + std::to_string(sb3_matrices.a.rows) + "x" + std::to_string(sb3_matrices.a.cols),
            reporter);

        // Encode SB3 test data (870 data bits + 10 filler)
        std::vector<uint8_t> sb3_data(lunanet::gateway2::kLdpcSb34.data_bits, 0);
        auto enc_start = Clock::now();
        const auto sb3_encoded = lunanet::gateway2::LdpcEncode(
            sb3_data, sb3_matrices, lunanet::gateway2::kLdpcSb34, &error);
        double enc_ms = std::chrono::duration<double, std::milli>(Clock::now() - enc_start).count();

        lunanet::testing::ValidateConditionTimed(
            kSuite, "SB3 encode output length (1740)",
            static_cast<int>(sb3_encoded.size()) == lunanet::gateway2::kLdpcSb34.output_symbols,
            "Got " + std::to_string(sb3_encoded.size()) + (error.empty() ? "" : ": " + error),
            enc_ms, reporter);

        // Verify binary
        if (!sb3_encoded.empty()) {
            bool all_binary = true;
            for (const auto s : sb3_encoded) {
                if (s > 1) { all_binary = false; break; }
            }
            lunanet::testing::ValidateCondition(
                kSuite, "SB3 encoded symbols are binary",
                all_binary, "Non-binary symbol found", reporter);
        }

        lunanet::testing::ValidateCondition(
            kSuite, "SB3 encode < 100ms",
            enc_ms < 100.0,
            std::to_string(enc_ms) + " ms", reporter);
    }
}

void RunGateway3Tests(
    const std::filesystem::path& repo_root,
    lunanet::testing::TestReporter& reporter) {

    const std::string kSync  = "Gateway3/Sync";
    const std::string kSB1   = "Gateway3/SB1";
    const std::string kSB2   = "Gateway3/SB2";
    const std::string kSB3   = "Gateway3/SB3";
    const std::string kSB4   = "Gateway3/SB4";
    const std::string kFrame = "Gateway3/Frame";
    const std::string kPerf  = "Gateway3/Performance";

    using Clock = std::chrono::high_resolution_clock;
    const std::filesystem::path csv_dir = repo_root / "Validation" / "annex3" / "csv";
    std::string error;

    // ── Load LDPC matrices ───────────────────────────────────────────────
    lunanet::gateway3::FrameMatrices matrices;
    const auto load_start = Clock::now();
    const bool matrices_loaded =
        lunanet::gateway3::LoadFrameMatrices(csv_dir.string(), &matrices, &error);
    const double load_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - load_start).count();

    lunanet::testing::ValidateConditionTimed(
        kFrame, "Load LDPC matrices", matrices_loaded, error, load_ms, reporter);

    // ── Sync Pattern ─────────────────────────────────────────────────────
    const auto sync = lunanet::gateway3::BuildSyncPattern();

    lunanet::testing::ValidateCondition(
        kSync, "Length = 68 symbols",
        static_cast<int>(sync.size()) == 68,
        "Got " + std::to_string(sync.size()), reporter);

    {
        bool all_binary = true;
        for (const auto s : sync) { if (s > 1) { all_binary = false; break; } }
        lunanet::testing::ValidateCondition(
            kSync, "All symbols binary", all_binary,
            "Non-binary symbol found", reporter);
    }

    if (sync.size() == 68) {
        // CC = 1100 1100 → first 8 bits
        const uint8_t exp_first8[] = {1,1,0,0, 1,1,0,0};
        bool ok = true;
        for (int i = 0; i < 8; ++i) { if (sync[i] != exp_first8[i]) { ok = false; break; } }
        lunanet::testing::ValidateCondition(
            kSync, "First byte matches 0xCC (1100 1100)", ok,
            "Sync bit mismatch at first byte", reporter);

        // Last nibble A = 1010 → bits 64-67
        const uint8_t exp_last4[] = {1,0,1,0};
        ok = true;
        for (int i = 0; i < 4; ++i) { if (sync[64 + i] != exp_last4[i]) { ok = false; break; } }
        lunanet::testing::ValidateCondition(
            kSync, "Last nibble matches 0xA (1010)", ok,
            "Sync bit mismatch at last nibble", reporter);
    }

    // ── Subframe 1 ───────────────────────────────────────────────────────
    {
        const auto sb1 = lunanet::gateway3::BuildSubframe1(0, 0);
        lunanet::testing::ValidateCondition(
            kSB1, "Length = 52 symbols",
            static_cast<int>(sb1.size()) == lunanet::gateway3::kSb1Symbols,
            "Got " + std::to_string(sb1.size()), reporter);
    }

    // Round-trip: encode → soft-decision decode → verify FID and TOI recovered
    for (const auto& [fid, toi] : std::vector<std::pair<uint8_t, uint8_t>>{
            {0, 0}, {1, 50}, {3, 99}, {2, 1}}) {
        const auto sb1 = lunanet::gateway3::BuildSubframe1(fid, toi);
        std::vector<double> soft(sb1.begin(), sb1.end());
        for (auto& s : soft) { s = (s == 0) ? 1.0 : -1.0; }
        const int decoded = lunanet::gateway2::BchDecodeSoft(soft);
        const uint16_t expected = static_cast<uint16_t>((fid << 7) | toi);
        lunanet::testing::ValidateCondition(
            kSB1, "Round-trip FID=" + std::to_string(fid) + " TOI=" + std::to_string(toi),
            decoded == static_cast<int>(expected),
            "Decoded 0x" + std::to_string(decoded) +
            ", expected 0x" + std::to_string(expected), reporter);
    }

    // ── Subframe 2 ───────────────────────────────────────────────────────
    {
        lunanet::gateway3::Subframe2Data sb2_data;
        sb2_data.wn = 100; sb2_data.itow = 0; sb2_data.toi = 1;

        auto packed = lunanet::gateway3::PackSubframe2(sb2_data);
        lunanet::testing::ValidateCondition(
            kSB2, "Pack = 1176 bits",
            static_cast<int>(packed.size()) == lunanet::gateway3::kSb2DataBits,
            "Got " + std::to_string(packed.size()), reporter);

        lunanet::gateway2::Crc24Append(packed);
        lunanet::testing::ValidateCondition(
            kSB2, "CRC-24 verifies after append",
            lunanet::gateway2::Crc24Verify(packed),
            "CRC verification failed on packed SB2", reporter);

        if (matrices_loaded) {
            error.clear();
            const auto enc_start = Clock::now();
            const auto sb2 = lunanet::gateway3::BuildSubframe2(sb2_data, matrices.sb2, &error);
            const double enc_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - enc_start).count();

            lunanet::testing::ValidateConditionTimed(
                kSB2, "LDPC encode → 2400 symbols",
                static_cast<int>(sb2.size()) == lunanet::gateway3::kSb2Symbols,
                "Got " + std::to_string(sb2.size()) +
                (error.empty() ? "" : ": " + error), enc_ms, reporter);

            if (!sb2.empty()) {
                bool all_binary = true;
                for (const auto s : sb2) { if (s > 1) { all_binary = false; break; } }
                lunanet::testing::ValidateCondition(
                    kSB2, "All encoded symbols binary", all_binary,
                    "Non-binary symbol found", reporter);
            }
        }
    }

    // ── Subframe 3 ───────────────────────────────────────────────────────
    {
        lunanet::gateway3::Subframe3Data sb3_data;
        sb3_data.type = 1;

        auto packed = lunanet::gateway3::PackSubframe3(sb3_data);
        lunanet::testing::ValidateCondition(
            kSB3, "Pack = 846 bits",
            static_cast<int>(packed.size()) == lunanet::gateway3::kSb3DataBits,
            "Got " + std::to_string(packed.size()), reporter);

        lunanet::gateway2::Crc24Append(packed);
        lunanet::testing::ValidateCondition(
            kSB3, "CRC-24 verifies after append",
            lunanet::gateway2::Crc24Verify(packed),
            "CRC verification failed on packed SB3", reporter);

        if (matrices_loaded) {
            error.clear();
            const auto enc_start = Clock::now();
            const auto sb3 = lunanet::gateway3::BuildSubframe3(sb3_data, matrices.sb34, &error);
            const double enc_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - enc_start).count();

            lunanet::testing::ValidateConditionTimed(
                kSB3, "LDPC encode → 1740 symbols",
                static_cast<int>(sb3.size()) == lunanet::gateway3::kSb3Symbols,
                "Got " + std::to_string(sb3.size()) +
                (error.empty() ? "" : ": " + error), enc_ms, reporter);
        }
    }

    // ── Subframe 4 ───────────────────────────────────────────────────────
    {
        lunanet::gateway3::Subframe4Data sb4_data;
        sb4_data.type = 2;

        auto packed = lunanet::gateway3::PackSubframe4(sb4_data);
        lunanet::testing::ValidateCondition(
            kSB4, "Pack = 846 bits",
            static_cast<int>(packed.size()) == lunanet::gateway3::kSb4DataBits,
            "Got " + std::to_string(packed.size()), reporter);

        lunanet::gateway2::Crc24Append(packed);
        lunanet::testing::ValidateCondition(
            kSB4, "CRC-24 verifies after append",
            lunanet::gateway2::Crc24Verify(packed),
            "CRC verification failed on packed SB4", reporter);

        if (matrices_loaded) {
            error.clear();
            const auto enc_start = Clock::now();
            const auto sb4 = lunanet::gateway3::BuildSubframe4(sb4_data, matrices.sb34, &error);
            const double enc_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - enc_start).count();

            lunanet::testing::ValidateConditionTimed(
                kSB4, "LDPC encode → 1740 symbols",
                static_cast<int>(sb4.size()) == lunanet::gateway3::kSb4Symbols,
                "Got " + std::to_string(sb4.size()) +
                (error.empty() ? "" : ": " + error), enc_ms, reporter);
        }
    }

    // ── Full Frame Assembly ───────────────────────────────────────────────
    if (matrices_loaded) {
        lunanet::gateway3::FrameInput input;
        input.fid = 0; input.toi = 1;
        input.sb2.wn = 100; input.sb2.itow = 0; input.sb2.toi = 1;
        input.sb3.type = 1;
        input.sb4.type = 2;

        error.clear();
        const auto frame_start = Clock::now();
        const auto frame = lunanet::gateway3::AssembleFrame(input, matrices, &error);
        const double frame_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - frame_start).count();

        lunanet::testing::ValidateConditionTimed(
            kFrame, "Full frame = 6000 symbols",
            static_cast<int>(frame.size()) == lunanet::gateway3::kFrameSymbols,
            "Got " + std::to_string(frame.size()) +
            (error.empty() ? "" : ": " + error), frame_ms, reporter);

        lunanet::testing::ValidateCondition(
            kPerf, "Frame assembly < 100ms",
            frame_ms < 100.0,
            std::to_string(frame_ms) + " ms", reporter);

        if (static_cast<int>(frame.size()) == lunanet::gateway3::kFrameSymbols) {
            // Sync and SB1 are not interleaved — verify they sit at frame[0..119] unchanged
            bool sync_intact = true;
            for (int i = 0; i < lunanet::gateway3::kSyncPatternSymbols; ++i) {
                if (frame[i] != sync[i]) { sync_intact = false; break; }
            }
            lunanet::testing::ValidateCondition(
                kFrame, "Sync pattern not interleaved (frame[0..67] intact)",
                sync_intact, "Sync symbols were modified by interleaver", reporter);

            // Per Figure 9, only SB2/SB3/SB4 are interleaved; SB1 must remain in the
            // clear immediately after the sync pattern at frame[68..119].
            const auto sb1_expected = lunanet::gateway3::BuildSubframe1(input.fid, input.toi);
            bool sb1_intact =
                static_cast<int>(sb1_expected.size()) == lunanet::gateway3::kSb1Symbols;
            for (int i = 0; sb1_intact && i < lunanet::gateway3::kSb1Symbols; ++i) {
                if (frame[lunanet::gateway3::kSyncPatternSymbols + i] != sb1_expected[i]) {
                    sb1_intact = false;
                }
            }
            lunanet::testing::ValidateCondition(
                kFrame, "SB1 not interleaved (frame[68..119] intact)",
                sb1_intact, "SB1 symbols were modified by interleaver", reporter);

            bool all_binary = true;
            for (const auto s : frame) { if (s > 1) { all_binary = false; break; } }
            lunanet::testing::ValidateCondition(
                kFrame, "All 6000 frame symbols binary",
                all_binary, "Non-binary symbol found", reporter);

            // Export binary and CSV — then verify hex length and both writes succeed
            const auto tmp = std::filesystem::temp_directory_path();
            const auto bin_path = (tmp / "gw3_test_frame.bin").string();
            const auto csv_path = (tmp / "gw3_test_frame.csv").string();

            error.clear();
            lunanet::testing::ValidateCondition(
                kFrame, "Export binary (750 bytes)",
                lunanet::gateway3::ExportFrameBinary(frame, bin_path, &error),
                error, reporter);

            error.clear();
            lunanet::testing::ValidateCondition(
                kFrame, "Export CSV (6000 lines)",
                lunanet::gateway3::ExportFrameCsv(frame, csv_path, &error),
                error, reporter);

            // Hex string length: 6000 bits → 750 bytes → 1500 hex chars
            const std::string hex = lunanet::gateway3::ExportFrameHex(frame);
            lunanet::testing::ValidateCondition(
                kFrame, "Hex export = 1500 chars (750 packed bytes)",
                hex.size() == 1500,
                "Got " + std::to_string(hex.size()) + " chars", reporter);

            // Frame timing: 6000 symbols @ 500 symbols/sec = 12 seconds
            const double frame_duration_sec = static_cast<double>(frame.size()) / 500.0;
            lunanet::testing::ValidateCondition(
                kFrame, "Frame duration = 12 seconds",
                std::abs(frame_duration_sec - 12.0) < 0.01,
                std::to_string(frame_duration_sec) + " sec", reporter);
        }
    }

    // ── Bit Allocation Validation (per specification tables) ───────────────
    {
        // SB2: Table 14 specifies layout as WN(13) + ITOW(9) + TOI(7) + payload
        lunanet::gateway3::Subframe2Data sb2_data;
        sb2_data.wn = 0x1FFF;      // Max 13-bit value
        sb2_data.itow = 0x1FF;     // Max 9-bit value
        sb2_data.toi = 0x7F;       // Max 7-bit value
        auto sb2_packed = lunanet::gateway3::PackSubframe2(sb2_data);

        lunanet::testing::ValidateCondition(
            kSB2, "Bit allocation = 1176 bits (13+9+7+1147)",
            static_cast<int>(sb2_packed.size()) == lunanet::gateway3::kSb2DataBits,
            "Got " + std::to_string(sb2_packed.size()) + " bits", reporter);

        // Verify first 29 bits match field sizes: WN(13) + ITOW(9) + TOI(7)
        bool wn_ok = true;
        for (int i = 0; i < 13; ++i) {
            if (sb2_packed[i] != 1u) { wn_ok = false; break; }
        }
        lunanet::testing::ValidateCondition(
            kSB2, "WN field (bits 0-12) encodes correctly",
            wn_ok, "WN bits not all set for max value", reporter);

        bool itow_ok = true;
        for (int i = 13; i < 22; ++i) {
            if (sb2_packed[i] != 1u) { itow_ok = false; break; }
        }
        lunanet::testing::ValidateCondition(
            kSB2, "ITOW field (bits 13-21) encodes correctly",
            itow_ok, "ITOW bits not all set for max value", reporter);

        bool toi_ok = true;
        for (int i = 22; i < 29; ++i) {
            if (sb2_packed[i] != 1u) { toi_ok = false; break; }
        }
        lunanet::testing::ValidateCondition(
            kSB2, "TOI field (bits 22-28) encodes correctly",
            toi_ok, "TOI bits not all set for max value", reporter);
    }

    // SB3/SB4: Tables 18-19 specify type field + payload
    {
        // Type field: 4 or 6 bits per frame_config.h (kSb34TypeFieldBits)
        lunanet::gateway3::Subframe3Data sb3_data;
        sb3_data.type = 0xF;  // All bits set for type field
        auto sb3_packed = lunanet::gateway3::PackSubframe3(sb3_data);

        lunanet::testing::ValidateCondition(
            kSB3, "Bit allocation = 846 bits (type + payload)",
            static_cast<int>(sb3_packed.size()) == lunanet::gateway3::kSb3DataBits,
            "Got " + std::to_string(sb3_packed.size()) + " bits", reporter);

        // Type field should occupy first kSb34TypeFieldBits
        bool type_ok = true;
        for (int i = 0; i < lunanet::gateway3::kSb34TypeFieldBits; ++i) {
            if (sb3_packed[i] != 1u) { type_ok = false; break; }
        }
        lunanet::testing::ValidateCondition(
            kSB3, "Type field (bits 0-3 or 0-5) encodes correctly",
            type_ok, "Type field bits not set for max value", reporter);

        // SB4 identical structure to SB3
        lunanet::gateway3::Subframe4Data sb4_data;
        sb4_data.type = 0xF;
        auto sb4_packed = lunanet::gateway3::PackSubframe4(sb4_data);

        lunanet::testing::ValidateCondition(
            kSB4, "Bit allocation = 846 bits (type + payload)",
            static_cast<int>(sb4_packed.size()) == lunanet::gateway3::kSb4DataBits,
            "Got " + std::to_string(sb4_packed.size()) + " bits", reporter);
    }
}

void RunGateway4Tests(const std::filesystem::path& repo_root,
                      lunanet::testing::TestReporter& reporter) {
    namespace g4 = lunanet::gateway4;

    const std::string kBpsk   = "Gateway4/BPSK";
    const std::string kMod    = "Gateway4/DataMod";
    const std::string kIq     = "Gateway4/IQ";
    const std::string kSync   = "Gateway4/Sync";
    const std::string kExport = "Gateway4/Export";
    const std::string kPerf   = "Gateway4/Performance";
    const std::string kE2e    = "EndToEnd/Pipeline";

    using Clock = std::chrono::high_resolution_clock;
    constexpr int kPrn = 1;

    // ── BPSK logic-to-signal mapping (Table 8) ───────────────────────────
    lunanet::testing::ValidateCondition(
        kBpsk, "Logic 0 → +1.0", g4::BpskMap(0) == 1.0f,
        "Got " + std::to_string(g4::BpskMap(0)), reporter);
    lunanet::testing::ValidateCondition(
        kBpsk, "Logic 1 → -1.0", g4::BpskMap(1) == -1.0f,
        "Got " + std::to_string(g4::BpskMap(1)), reporter);
    {
        const auto mapped = g4::BpskModulate({0, 1, 1, 0});
        const std::vector<float> expected = {1.0f, -1.0f, -1.0f, 1.0f};
        lunanet::testing::ValidateCondition(
            kBpsk, "Chip stream maps element-wise",
            mapped == expected, "BPSK vector mapping mismatch", reporter);
    }

    // ── AFS-I data modulation (LSIS-160) ─────────────────────────────────
    const auto gold = lunanet::generate_afs_i(kPrn);
    lunanet::testing::ValidateCondition(
        kMod, "AFS-I primary = 2046 chips",
        static_cast<int>(gold.size()) == g4::kAfsIPrimaryChips,
        "Got " + std::to_string(gold.size()), reporter);

    std::string error;
    const std::vector<uint8_t> data_symbols = {0, 1, 0, 1};
    const auto afs_i = g4::ModulateAfsIData(gold, data_symbols, &error);

    lunanet::testing::ValidateCondition(
        kMod, "Modulated length = symbols × 2046",
        static_cast<int>(afs_i.size()) ==
            static_cast<int>(data_symbols.size()) * g4::kAfsIChipsPerSymbol,
        "Got " + std::to_string(afs_i.size()) +
        (error.empty() ? "" : ": " + error), reporter);

    if (afs_i.size() == data_symbols.size() * static_cast<size_t>(g4::kAfsIPrimaryChips)) {
        // Symbol 0 (bit 0) leaves the code unchanged; symbol 1 (bit 1) inverts it.
        bool sym0_ok = true, sym1_ok = true;
        for (int i = 0; i < g4::kAfsIPrimaryChips; ++i) {
            if (afs_i[i] != gold[i]) { sym0_ok = false; break; }
        }
        for (int i = 0; i < g4::kAfsIPrimaryChips; ++i) {
            if (afs_i[g4::kAfsIPrimaryChips + i] != (gold[i] ^ 1u)) { sym1_ok = false; break; }
        }
        lunanet::testing::ValidateCondition(
            kMod, "Data symbol 0 preserves code epoch", sym0_ok,
            "Symbol-0 epoch differs from primary code", reporter);
        lunanet::testing::ValidateCondition(
            kMod, "Data symbol 1 inverts code epoch", sym1_ok,
            "Symbol-1 epoch is not the inverted primary code", reporter);
    }

    // ── I/Q generation on a short, time-aligned segment ──────────────────
    // Two AFS-I symbols (4092 chips) → AFS-Q must carry 5× that (20460 chips).
    const size_t i_chips = data_symbols.size() * static_cast<size_t>(g4::kAfsIPrimaryChips);
    const size_t q_chips = i_chips * g4::kQOverIChipRatio;
    const auto afs_q = lunanet::generate_afs_q(kPrn, q_chips);

    lunanet::testing::ValidateCondition(
        kIq, "AFS-Q segment = 5 × AFS-I chips",
        afs_q.size() == q_chips,
        "Got " + std::to_string(afs_q.size()) + ", expected " + std::to_string(q_chips),
        reporter);

    // ── Default rate (AFS-I chip rate = 1.023 MHz, workshop interop default) ──
    {
        g4::IqConfig config;  // default Fs = AFS-I chip rate (1.023 MHz)
        error.clear();
        const auto gen_start = Clock::now();
        const auto signal = g4::GenerateIq(afs_i, afs_q, config, &error);
        const double gen_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - gen_start).count();

        // At 1.023 MHz: 1 sample per AFS-I chip → i_chips samples.
        lunanet::testing::ValidateConditionTimed(
            kIq, "Default rate: sample count = AFS-I chips (" + std::to_string(i_chips) + ")",
            signal.i.size() == i_chips && signal.q.size() == i_chips,
            "Got I=" + std::to_string(signal.i.size()) +
            " Q=" + std::to_string(signal.q.size()) +
            (error.empty() ? "" : ": " + error), gen_ms, reporter);

        bool all_pm1 = !signal.i.empty();
        for (const float v : signal.i) { if (v != 1.0f && v != -1.0f) { all_pm1 = false; break; } }
        for (const float v : signal.q) { if (v != 1.0f && v != -1.0f) { all_pm1 = false; break; } }
        lunanet::testing::ValidateCondition(
            kIq, "All I/Q samples are ±1.0", all_pm1,
            "Found a sample outside {-1.0, +1.0}", reporter);

        // At 1.023 MHz: 1 sample per AFS-I chip, so I[n] = BPSK(afs_i[n]).
        bool i_hold_ok = signal.i.size() == i_chips;
        for (size_t n = 0; n < signal.i.size() && i_hold_ok; ++n) {
            if (signal.i[n] != g4::BpskMap(afs_i[n])) i_hold_ok = false;
        }
        lunanet::testing::ValidateCondition(
            kIq, "AFS-I 1:1 sample/chip at default rate", i_hold_ok,
            "AFS-I sample alignment incorrect at 1.023 MHz", reporter);

        // At 1.023 MHz: Q is decimated by 5. Q[n] maps to chip floor(n * 5 / 1).
        // Each output sample picks the Q chip at that time instant.
        bool q_align_ok = signal.q.size() == i_chips;
        for (size_t n = 0; n < signal.q.size() && q_align_ok; ++n) {
            const size_t q_idx = n * g4::kQOverIChipRatio;
            if (signal.q[n] != g4::BpskMap(afs_q[q_idx])) q_align_ok = false;
        }
        lunanet::testing::ValidateCondition(
            kIq, "AFS-Q decimated correctly at default rate", q_align_ok,
            "AFS-Q sample alignment incorrect at 1.023 MHz", reporter);

        // Export formats on the short segment.
        const auto tmp = std::filesystem::temp_directory_path();
        const auto bin_path = (tmp / "gw4_test_iq.bin").string();
        const auto csv_path = (tmp / "gw4_test_iq.csv").string();

        error.clear();
        const bool bin_ok = g4::ExportIqBinary(signal, bin_path, &error);
        lunanet::testing::ValidateCondition(
            kExport, "Export interleaved float32 binary", bin_ok, error, reporter);
        if (bin_ok) {
            std::error_code ec;
            const auto bytes = std::filesystem::file_size(bin_path, ec);
            const uintmax_t expected_bytes = static_cast<uintmax_t>(i_chips) * 2u * sizeof(float);
            lunanet::testing::ValidateCondition(
                kExport, "Binary size = samples × 2 × 4 bytes",
                !ec && bytes == expected_bytes,
                "Got " + std::to_string(bytes) + ", expected " + std::to_string(expected_bytes),
                reporter);
        }

        error.clear();
        lunanet::testing::ValidateCondition(
            kExport, "Export CSV (index,I,Q)",
            g4::ExportIqCsv(signal, csv_path, &error), error, reporter);
    }

    // ── AFS-Q chip rate (5.115 MHz): full-resolution, 1 sample per Q chip ──
    {
        g4::IqConfig q_rate;
        q_rate.sample_rate_hz = g4::kAfsQChipRateHz;  // 5.115 MHz
        error.clear();
        const auto signal_q = g4::GenerateIq(afs_i, afs_q, q_rate, &error);
        lunanet::testing::ValidateCondition(
            kIq, "AFS-Q rate: sample count = Q chips (" + std::to_string(q_chips) + ")",
            signal_q.i.size() == q_chips && signal_q.sample_rate_hz == g4::kAfsQChipRateHz,
            "Got " + std::to_string(signal_q.i.size()) +
            (error.empty() ? "" : ": " + error), reporter);
    }

    // ── Configurable oversample (2× AFS-Q rate = 10.23 MHz) ─────────────
    {
        g4::IqConfig oversampled;
        oversampled.sample_rate_hz = 2 * g4::kAfsQChipRateHz;  // 10.23 MHz
        error.clear();
        const auto signal2 = g4::GenerateIq(afs_i, afs_q, oversampled, &error);
        lunanet::testing::ValidateCondition(
            kIq, "Oversample ×2 doubles Q-rate sample count",
            signal2.i.size() == q_chips * 2 && signal2.sample_rate_hz == 2 * g4::kAfsQChipRateHz,
            "Got " + std::to_string(signal2.i.size()) +
            (error.empty() ? "" : ": " + error), reporter);

        // A rate that is not a multiple of the AFS-I chip rate must be rejected.
        g4::IqConfig invalid;
        invalid.sample_rate_hz = 1000000;  // 1 MHz — not a multiple of 1.023 MHz
        std::string err2;
        const auto bad = g4::GenerateIq(afs_i, afs_q, invalid, &err2);
        lunanet::testing::ValidateCondition(
            kIq, "Invalid sample rate rejected",
            bad.i.empty() && !err2.empty(),
            "Generator accepted an invalid sample rate", reporter);
    }

    // ── Chip rates, symbol rate, and 12-second duration (Table 7, LSIS-220) ──
    lunanet::testing::ValidateCondition(
        kSync, "AFS-I chip rate = 1.023 Mchip/s",
        g4::kAfsIChipRateHz == 1023000, "Wrong AFS-I chip rate", reporter);
    lunanet::testing::ValidateCondition(
        kSync, "AFS-Q chip rate = 5.115 Mchip/s",
        g4::kAfsQChipRateHz == 5115000, "Wrong AFS-Q chip rate", reporter);
    lunanet::testing::ValidateCondition(
        kSync, "AFS-I symbol rate = 500 sym/s",
        g4::kSymbolRateHz == 500, "Wrong symbol rate", reporter);

    // A full frame spans exactly 12 s on both channels.
    const long afs_i_frame_chips =
        static_cast<long>(g4::kFrameSymbols) * g4::kAfsIChipsPerSymbol;  // 12,276,000
    lunanet::testing::ValidateCondition(
        kSync, "AFS-I frame = 12 s (12,276,000 chips)",
        afs_i_frame_chips == static_cast<long>(g4::kAfsIChipRateHz) * g4::kFrameDurationSec,
        "Got " + std::to_string(afs_i_frame_chips) + " chips", reporter);

    // Derive the AFS-Q tiered length from the actual generated code lengths
    // (Table 9): primary × secondary × tertiary = one 12 s tertiary period.
    const auto weil_primary = lunanet::generate_weil_primary(kPrn);
    const auto weil_tertiary = lunanet::generate_weil_tertiary(kPrn);
    const long afs_q_frame_chips = static_cast<long>(weil_primary.size()) *
        g4::kAfsQSecondaryChips * static_cast<long>(weil_tertiary.size());
    lunanet::testing::ValidateCondition(
        kSync, "AFS-Q tiered code = 12 s (61,380,000 chips, tertiary→frame sync)",
        afs_q_frame_chips == static_cast<long>(g4::kAfsQChipRateHz) * g4::kFrameDurationSec &&
            afs_q_frame_chips == g4::kAfsQTieredChips,
        "Got " + std::to_string(afs_q_frame_chips) + " chips", reporter);

    // ── End-to-end integration: Gateway3 frame → Gateway4 I/Q export ─────
    {
        namespace g3 = lunanet::gateway3;
        std::string e2e_error;
        const std::filesystem::path csv_dir = repo_root / "Validation" / "annex3" / "csv";

        g3::FrameMatrices matrices;
        const bool matrices_loaded = g3::LoadFrameMatrices(csv_dir.string(), &matrices, &e2e_error);
        lunanet::testing::ValidateCondition(
            kE2e, "Load frame LDPC matrices", matrices_loaded,
            e2e_error, reporter);

        if (matrices_loaded) {
            g3::FrameInput input;
            input.fid = 0;
            input.toi = 1;
            input.sb2.wn = 100;
            input.sb2.itow = 250;
            input.sb2.toi = 1;
            input.sb3.type = 1;
            input.sb4.type = 2;

            e2e_error.clear();
            const auto frame = g3::AssembleFrame(input, matrices, &e2e_error);
            lunanet::testing::ValidateCondition(
                kE2e, "Assemble full frame (6000 symbols)",
                static_cast<int>(frame.size()) == g3::kFrameSymbols,
                "Got " + std::to_string(frame.size()) +
                (e2e_error.empty() ? "" : ": " + e2e_error), reporter);

            if (static_cast<int>(frame.size()) == g3::kFrameSymbols) {
                const auto gold = lunanet::generate_gold_code(kPrn);
                lunanet::testing::ValidateCondition(
                    kE2e, "Generate PRN Gold code for AFS-I modulation",
                    static_cast<int>(gold.size()) == g4::kAfsIPrimaryChips,
                    "Got " + std::to_string(gold.size()), reporter);

                e2e_error.clear();
                const auto afs_i_full = g4::ModulateAfsIData(gold, frame, &e2e_error);
                const size_t expected_i_chips = frame.size() * static_cast<size_t>(g4::kAfsIPrimaryChips);
                lunanet::testing::ValidateCondition(
                    kE2e, "AFS-I chips = frame symbols × 2046",
                    afs_i_full.size() == expected_i_chips,
                    "Got " + std::to_string(afs_i_full.size()) +
                    ", expected " + std::to_string(expected_i_chips) +
                    (e2e_error.empty() ? "" : ": " + e2e_error), reporter);

                if (afs_i_full.size() == expected_i_chips) {
                    const size_t expected_q_chips =
                        afs_i_full.size() * static_cast<size_t>(g4::kQOverIChipRatio);
                    const auto afs_q_full = lunanet::generate_afs_q(kPrn, expected_q_chips);
                    lunanet::testing::ValidateCondition(
                        kE2e, "AFS-Q chips = 5 × AFS-I chips",
                        afs_q_full.size() == expected_q_chips,
                        "Got " + std::to_string(afs_q_full.size()) +
                        ", expected " + std::to_string(expected_q_chips), reporter);

                    if (afs_q_full.size() == expected_q_chips) {
                        g4::IqConfig e2e_cfg;
                        e2e_cfg.sample_rate_hz = g4::kDefaultSampleRateHz;
                        e2e_error.clear();
                        const auto signal_full =
                            g4::GenerateIq(afs_i_full, afs_q_full, e2e_cfg, &e2e_error);

                        lunanet::testing::ValidateCondition(
                            kE2e, "Generate end-to-end I/Q at 1.023 MHz",
                            signal_full.i.size() == expected_i_chips &&
                                signal_full.q.size() == expected_i_chips,
                            "I=" + std::to_string(signal_full.i.size()) +
                            " Q=" + std::to_string(signal_full.q.size()) +
                            (e2e_error.empty() ? "" : ": " + e2e_error), reporter);

                        if (!signal_full.i.empty() && signal_full.i.size() == signal_full.q.size()) {
                            const auto tmp = std::filesystem::temp_directory_path();
                            const auto bin_path = (tmp / "lunanet_e2e_signal.iq32").string();

                            e2e_error.clear();
                            const bool wrote = g4::ExportIqBinary(signal_full, bin_path, &e2e_error);
                            lunanet::testing::ValidateCondition(
                                kE2e, "Export end-to-end interleaved float32 IQ", wrote,
                                e2e_error, reporter);

                            if (wrote) {
                                std::error_code ec;
                                const auto bytes = std::filesystem::file_size(bin_path, ec);
                                const uintmax_t expected_bytes =
                                    static_cast<uintmax_t>(signal_full.i.size()) * 2u * sizeof(float);
                                lunanet::testing::ValidateCondition(
                                    kE2e, "E2E IQ binary size = samples × 2 × 4 bytes",
                                    !ec && bytes == expected_bytes,
                                    "Got " + std::to_string(bytes) +
                                    ", expected " + std::to_string(expected_bytes), reporter);
                            }
                        }
                    }
                }
            }
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const RunOptions options = ParseRunOptions(argc, argv);
    if (!options.parse_error.empty()) {
        std::cerr << "FAIL: " << options.parse_error << std::endl;
        PrintUsage(std::cerr, argv[0]);
        return 1;
    }
    if (options.show_help) {
        PrintUsage(std::cout, argv[0]);
        return 0;
    }

    lunanet::initialize_engine();

    const char* config_override =
        options.config_override.empty() ? nullptr : options.config_override.c_str();
    const std::string config_path = FindConfigPath(config_override);
    std::string config_error;
    if (!lunanet::load_spreading_code_config(config_path, &config_error)) {
        std::cerr << "FAIL: Could not load spreading code config: " << config_error << std::endl;
        return 1;
    }

    lunanet::testing::TestReporter reporter;
    const bool run_gateway1 =
        options.gateway == GatewaySelection::kAll ||
        options.gateway == GatewaySelection::kGateway1;
    const bool run_gateway2 =
        options.gateway == GatewaySelection::kAll ||
        options.gateway == GatewaySelection::kGateway2;
    const bool run_gateway3 =
        options.gateway == GatewaySelection::kAll ||
        options.gateway == GatewaySelection::kGateway3;
    const bool run_gateway4 =
        options.gateway == GatewaySelection::kAll ||
        options.gateway == GatewaySelection::kGateway4;
    const bool run_gateway5 =
        options.gateway == GatewaySelection::kAll ||
        options.gateway == GatewaySelection::kGateway5;

    std::cout << "Validation scope: " << GatewayScopeName(options.gateway) << std::endl;

    const std::filesystem::path repo_root =
        std::filesystem::path(config_path).parent_path().parent_path();
    const std::filesystem::path annex3_txt_dir = repo_root / "Validation" / "annex3" / "txt";

    if (run_gateway1) {
        RunSmokeTests(reporter);

        std::cout << "Sample PRN 1 Gold HEX[24]: "
                  << lunanet::chips_to_hex(lunanet::generate_gold_code(1), 24) << std::endl;
        std::cout << "Sample PRN 1 Weil Primary HEX[24]: "
                  << lunanet::chips_to_hex(lunanet::generate_weil_primary(1), 24) << std::endl;
        std::cout << "Sample PRN 1 Weil Tertiary HEX[24]: "
                  << lunanet::chips_to_hex(lunanet::generate_weil_tertiary(1), 24) << std::endl;

        RunAnnex3Validation(annex3_txt_dir, reporter);
        RunTable11Validation(config_path, reporter);
        RunPerformanceBenchmarks(reporter);
    }

    if (run_gateway2) {
        RunGateway2Tests(reporter);
        RunLdpcTests(repo_root, reporter);
    }

    if (run_gateway3) {
        RunGateway3Tests(repo_root, reporter);
    }

    if (run_gateway4) {
        RunGateway4Tests(repo_root, reporter);
    }
    //if (run_gateway5) {
      //  RunGateway5Tests(repo_root, reporter);
    //}

    reporter.PrintSummary(std::cout);

    const std::filesystem::path reports_base =
        options.reports_base_override.empty()
            ? (repo_root / "Validation" / "reports")
            : std::filesystem::path(options.reports_base_override);

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &now_t);
#else
    localtime_r(&now_t, &tm_buf);
#endif

    std::ostringstream date_ss;
    date_ss << std::put_time(&tm_buf, "%Y-%m-%d");
    std::ostringstream time_ss;
    time_ss << std::put_time(&tm_buf, "%H-%M-%S");

    const std::filesystem::path report_dir = reports_base / date_ss.str();
    std::string report_stem = time_ss.str();
    if (options.gateway != GatewaySelection::kAll) {
        report_stem += "_" + GatewayScopeName(options.gateway);
    }

    if (!reporter.WriteMarkdownReport(report_dir / (report_stem + ".md"))) {
        std::cerr << "Warning: Failed to write markdown report." << std::endl;
    }
    if (!reporter.WriteJUnitXml(report_dir / (report_stem + ".xml"))) {
        std::cerr << "Warning: Failed to write JUnit XML report." << std::endl;
    }

    std::cout << "Reports written to: " << report_dir.string() << std::endl;
    return reporter.AllPassed() ? 0 : 1;
}
