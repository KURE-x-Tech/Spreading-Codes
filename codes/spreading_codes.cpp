#include "spreading_codes.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#include "gateway1/gold_code_generator.h"
#include "gateway1/spreading_config.h"
#include "gateway1/tiered_code_generator.h"
#include "gateway1/weil_code_generator.h"
#include "lunanet/version.h"

namespace lunanet {

namespace {

struct EngineState {
    bool initialized = false;
    bool config_loaded = false;
    gateway1::SpreadingSpecTables tables;
    gateway1::Annex3Paths annex3;
    std::string last_error;
};

EngineState g_state;

void SetError(const std::string& message) {
    g_state.last_error = message;
}

bool EnsureConfigLoaded() {
    if (!g_state.config_loaded) {
        SetError("Spreading code config not loaded");
        return false;
    }
    return true;
}

std::string Trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(start, end - start);
}

int HexToNibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    return -1;
}

}  // namespace

void initialize_engine() {
    g_state = EngineState{};
    g_state.initialized = true;
}

const char* hello_moon() {
    return "Hello Moon";
}

const char* get_version() {
    return build_info::kVersion;
}

const char* get_last_error() {
    return g_state.last_error.c_str();
}

bool load_spreading_code_config(const std::string& config_path, std::string* error_message) {
    std::string local_error;
    std::string* error_target = (error_message != nullptr) ? error_message : &local_error;

    gateway1::SpreadingSpecTables tables;
    gateway1::Annex3Paths annex3;
    if (!gateway1::LoadSpreadingConfig(config_path, &tables, &annex3, error_target)) {
        SetError(*error_target);
        return false;
    }

    g_state.tables = std::move(tables);
    g_state.annex3 = std::move(annex3);
    g_state.config_loaded = true;
    g_state.last_error.clear();
    return true;
}

const char* get_annex3_gold_path() {
    return g_state.annex3.gold.c_str();
}

const char* get_annex3_weil_primary_path() {
    return g_state.annex3.weil_primary.c_str();
}

const char* get_annex3_weil_tertiary_path() {
    return g_state.annex3.weil_tertiary.c_str();
}

size_t get_afs_q_max_chips() {
    return g_state.tables.afs_q_max_chips;
}

std::vector<uint8_t> generate_gold_code(int prn) {
    if (!EnsureConfigLoaded()) {
        return {};
    }
    std::string error;
    std::vector<uint8_t> code = gateway1::GenerateGoldCode(prn, g_state.tables, &error);
    if (code.empty() && !error.empty()) {
        SetError(error);
    }
    return code;
}

std::vector<uint8_t> generate_legendre_sequence(int prime) {
    return gateway1::GenerateLegendreSequence(prime);
}

std::vector<uint8_t> generate_weil_primary(int prn) {
    if (!EnsureConfigLoaded()) {
        return {};
    }
    std::string error;
    std::vector<uint8_t> code = gateway1::GenerateWeilPrimary(prn, g_state.tables, &error);
    if (code.empty() && !error.empty()) {
        SetError(error);
    }
    return code;
}

std::vector<uint8_t> generate_weil_tertiary(int prn) {
    if (!EnsureConfigLoaded()) {
        return {};
    }
    std::string error;
    std::vector<uint8_t> code = gateway1::GenerateWeilTertiary(prn, g_state.tables, &error);
    if (code.empty() && !error.empty()) {
        SetError(error);
    }
    return code;
}

std::vector<uint8_t> generate_afs_i(int prn) {
    if (!EnsureConfigLoaded()) {
        return {};
    }
    std::string error;
    std::vector<uint8_t> code = gateway1::GenerateAfsI(prn, g_state.tables, &error);
    if (code.empty() && !error.empty()) {
        SetError(error);
    }
    return code;
}

std::vector<uint8_t> generate_afs_q(int prn, size_t max_chips) {
    if (!EnsureConfigLoaded()) {
        return {};
    }

    const size_t effective_limit =
        (max_chips > 0) ? max_chips : g_state.tables.afs_q_max_chips;

    std::string error;
    std::vector<uint8_t> code = gateway1::GenerateAfsQ(prn, g_state.tables, effective_limit, &error);
    if (code.empty() && !error.empty()) {
        SetError(error);
    }
    return code;
}

std::map<int, std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> generate_all_spreading_codes() {
    std::map<int, std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> output;
    if (!EnsureConfigLoaded()) {
        return output;
    }

    for (int prn = 1; prn <= MAX_PRNS; ++prn) {
        std::vector<uint8_t> afs_i = generate_afs_i(prn);
        std::vector<uint8_t> afs_q = generate_afs_q(prn, g_state.tables.afs_q_max_chips);
        if (afs_i.empty() || afs_q.empty()) {
            return {};
        }
        output.emplace(prn, std::make_pair(std::move(afs_i), std::move(afs_q)));
    }

    return output;
}

std::string chips_to_hex(const std::vector<uint8_t>& chips, size_t num_chips) {
    const size_t chip_count = (num_chips == 0) ? chips.size() : std::min(num_chips, chips.size());
    std::string hex;
    hex.reserve((chip_count + 3U) / 4U);

    uint8_t nibble = 0;
    for (size_t i = 0; i < chip_count; ++i) {
        nibble = static_cast<uint8_t>((nibble << 1) | (chips[i] & 1U));
        if ((i + 1) % 4 == 0) {
            hex.push_back("0123456789ABCDEF"[nibble & 0x0F]);
            nibble = 0;
        }
    }

    const size_t remainder = chip_count % 4;
    if (remainder != 0) {
        nibble = static_cast<uint8_t>(nibble << (4 - remainder));
        hex.push_back("0123456789ABCDEF"[nibble & 0x0F]);
    }

    return hex;
}

std::string vector_to_hex(const std::vector<uint8_t>& chips, size_t num_chips) {
    return chips_to_hex(chips, num_chips);
}

std::vector<uint8_t> hex_to_vector(const std::string& hex, size_t num_chips) {
    std::vector<uint8_t> chips;
    chips.reserve(hex.size() * 4U);

    for (const char ch : hex) {
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            continue;
        }

        const int value = HexToNibble(ch);
        if (value < 0) {
            continue;
        }

        for (int bit = 3; bit >= 0; --bit) {
            chips.push_back(static_cast<uint8_t>((value >> bit) & 1));
        }
    }

    if (num_chips > 0 && chips.size() > num_chips) {
        chips.erase(chips.begin(), chips.begin() + (chips.size() - num_chips));
    }

    return chips;
}

bool load_reference_hex_file(const std::string& path,
                             size_t expected_chips,
                             std::vector<std::vector<uint8_t>>& out,
                             std::string* error_message) {
    std::ifstream input(path);
    if (!input) {
        if (error_message != nullptr) {
            *error_message = "Failed to open reference file: " + path;
        }
        return false;
    }

    out.assign(MAX_PRNS, {});

    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const size_t comma = trimmed.find(',');
        if (comma == std::string::npos) {
            continue;
        }

        const std::string prn_text = Trim(trimmed.substr(0, comma));
        const std::string hex_text = Trim(trimmed.substr(comma + 1));

        char* end_ptr = nullptr;
        const long prn = std::strtol(prn_text.c_str(), &end_ptr, 10);
        if (end_ptr == prn_text.c_str() || *end_ptr != '\0') {
            continue;
        }
        if (prn < 1 || prn > MAX_PRNS) {
            continue;
        }

        std::vector<uint8_t> chips = hex_to_vector(hex_text, expected_chips);
        if (expected_chips > 0 && chips.size() < expected_chips) {
            if (error_message != nullptr) {
                *error_message = "Reference row shorter than expected for PRN " + std::to_string(prn);
            }
            return false;
        }

        out[static_cast<size_t>(prn - 1)] = std::move(chips);
    }

    for (int prn = 1; prn <= MAX_PRNS; ++prn) {
        if (out[prn - 1].empty()) {
            if (error_message != nullptr) {
                *error_message = "Missing reference row for PRN " + std::to_string(prn);
            }
            return false;
        }
    }

    return true;
}

}  // namespace lunanet
