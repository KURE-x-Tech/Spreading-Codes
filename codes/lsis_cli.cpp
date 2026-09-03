// LSIS-AFS CLI tool for interoperability testing.
//
// Subcommands:
//   generate-codes  — Generate all 210 Gold primary spreading codes as hex.
//   encode          — Encode a navigation frame and optionally modulate to I/Q.
//   decode          — Decode a navigation frame from an I/Q signal file.
//   version         — Print version string.
//
// Conforms to the CLI contract specified in the CCSDS 235.1 & LSIS-AFS
// Mid-Project Workshop programme.

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
// Avoid Windows min/max macro collisions with std::min/std::max.
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include "spreading_codes.h"
#include "gateway1/spreading_config.h"
#include "gateway3/frame_assembler.h"
#include "gateway3/frame_exporter.h"
#include "gateway4/bpsk_modulator.h"
#include "gateway4/iq_generator.h"
#include "gateway4/signal_config.h"
#include "gateway4/signal_exporter.h"
#include "gateway5/frame_decoder.h"
#include "gateway6/subframe1_parser.h"
#include "gateway6/subframe2_parser.h"
#include "gateway6/subframe3_parser.h"
#include "gateway6/subframe4_parser.h"

namespace
{

    namespace fs = std::filesystem;

    fs::path g_executable_dir;

    // ── Argument helpers ────────────────────────────────────────────────────────

    struct Args
    {
        int argc;
        char **argv;
        int pos = 0;
        bool parse_error = false;
    };

    bool HasNext(const Args &a) { return a.pos < a.argc; }

    const char *Peek(const Args &a)
    {
        return (a.pos < a.argc) ? a.argv[a.pos] : nullptr;
    }

    const char *Next(Args &a)
    {
        return (a.pos < a.argc) ? a.argv[a.pos++] : nullptr;
    }

    bool Match(Args &a, const char *flag)
    {
        if (a.pos < a.argc && std::strcmp(a.argv[a.pos], flag) == 0)
        {
            ++a.pos;
            return true;
        }
        return false;
    }

    bool GetString(Args &a, const char *flag, std::string &out)
    {
        if (a.pos + 1 < a.argc && std::strcmp(a.argv[a.pos], flag) == 0)
        {
            out = a.argv[a.pos + 1];
            a.pos += 2;
            return true;
        }
        return false;
    }

    bool GetInt(Args &a, const char *flag, int &out)
    {
        std::string s;
        if (GetString(a, flag, s))
        {
            errno = 0;
            char *end = nullptr;
            const long value = std::strtol(s.c_str(), &end, 10);
            if (errno == ERANGE || end == s.c_str() || *end != '\0' ||
                value < static_cast<long>(std::numeric_limits<int>::min()) ||
                value > static_cast<long>(std::numeric_limits<int>::max()))
            {
                std::cerr << "error: invalid integer for " << flag << ": " << s << "\n";
                a.parse_error = true;
                return true;
            }
            out = static_cast<int>(value);
            return true;
        }
        return false;
    }

    bool GetDouble(Args &a, const char *flag, double &out)
    {
        std::string s;
        if (GetString(a, flag, s))
        {
            errno = 0;
            char *end = nullptr;
            const double value = std::strtod(s.c_str(), &end);
            if (errno == ERANGE || end == s.c_str() || *end != '\0' ||
                !std::isfinite(value))
            {
                std::cerr << "error: invalid number for " << flag << ": " << s << "\n";
                a.parse_error = true;
                return true;
            }
            out = value;
            return true;
        }
        return false;
    }

    // ── Config path resolution ──────────────────────────────────────────────────

    fs::path NativeExecutablePath()
    {
#ifdef _WIN32
        std::vector<wchar_t> buffer(32768u, L'\0');
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length > 0 && length < buffer.size())
        {
            return fs::path(std::wstring(buffer.data(), length));
        }
#elif defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        if (size > 0)
        {
            std::vector<char> buffer(size, '\0');
            if (_NSGetExecutablePath(buffer.data(), &size) == 0)
            {
                return fs::path(buffer.data());
            }
        }
#elif defined(__linux__)
        std::vector<char> buffer(4096u, '\0');
        while (buffer.size() <= 1024u * 1024u)
        {
            const ssize_t length = readlink(
                "/proc/self/exe", buffer.data(), buffer.size());
            if (length < 0)
                break;
            if (static_cast<std::size_t>(length) < buffer.size())
            {
                return fs::path(std::string(buffer.data(), static_cast<std::size_t>(length)));
            }
            buffer.resize(buffer.size() * 2u);
        }
#endif
        return {};
    }

    void InitializeExecutableDir(const char *argv0)
    {
        fs::path executable = NativeExecutablePath();
        if (executable.empty() && argv0 != nullptr && *argv0 != '\0')
        {
            executable = fs::path(argv0);
        }
        if (executable.empty())
            return;

        std::error_code error;
        executable = fs::absolute(executable, error);
        if (error)
            return;
        const fs::path canonical = fs::weakly_canonical(executable, error);
        g_executable_dir = (error ? executable : canonical).parent_path();
    }

    std::string FindResourcePath(const fs::path &source_relative,
                                 const fs::path &install_relative,
                                 const std::string &fallback,
                                 bool directory)
    {
        fs::path search = g_executable_dir;
        for (int depth = 0; !search.empty() && depth < 6; ++depth)
        {
            const std::array<fs::path, 2> candidates = {
                search / install_relative,
                search / source_relative,
            };
            for (const fs::path &candidate : candidates)
            {
                std::error_code error;
                const bool found = directory
                                       ? fs::is_directory(candidate, error)
                                       : fs::is_regular_file(candidate, error);
                if (found && !error)
                    return candidate.string();
            }
            if (!search.has_parent_path() || search == search.parent_path())
                break;
            search = search.parent_path();
        }
        return fallback;
    }

    std::string FindConfigPath(const std::string &explicit_path)
    {
        if (!explicit_path.empty())
            return explicit_path;
        return FindResourcePath(
            fs::path("config") / "spreading_codes_config.ini",
            fs::path("share") / "lunanet" / "config" / "spreading_codes_config.ini",
            "config/spreading_codes_config.ini",
            false);
    }

    std::string FindAnnex3CsvDir(const std::string &explicit_path)
    {
        if (!explicit_path.empty())
            return explicit_path;
        return FindResourcePath(
            fs::path("Validation") / "annex3" / "csv",
            fs::path("share") / "lunanet" / "Validation" / "annex3" / "csv",
            "Validation/annex3/csv",
            true);
    }

    // ── Hex string to bytes ─────────────────────────────────────────────────────

    std::vector<uint8_t> ParseHexPayload(const std::string &hex)
    {
        std::vector<uint8_t> bits;
        for (char c : hex)
        {
            int v = -1;
            if (c >= '0' && c <= '9')
                v = c - '0';
            else if (c >= 'A' && c <= 'F')
                v = 10 + (c - 'A');
            else if (c >= 'a' && c <= 'f')
                v = 10 + (c - 'a');
            else
                continue;
            for (int b = 3; b >= 0; --b)
            {
                bits.push_back(static_cast<uint8_t>((v >> b) & 1));
            }
        }
        return bits;
    }

    enum class CodeFamily
    {
        Gold,
        WeilPrimary,
        WeilTertiary,
        All,
    };

    bool ParseCodeFamily(const std::string &text, CodeFamily &out)
    {
        if (text.empty() || text == "gold")
        {
            out = CodeFamily::Gold;
            return true;
        }
        if (text == "weil-primary" || text == "weil_primary" || text == "weil")
        {
            out = CodeFamily::WeilPrimary;
            return true;
        }
        if (text == "weil-tertiary" || text == "weil_tertiary" || text == "tertiary")
        {
            out = CodeFamily::WeilTertiary;
            return true;
        }
        if (text == "all")
        {
            out = CodeFamily::All;
            return true;
        }
        return false;
    }

    bool WriteCodeFamily(std::ostream &out_stream, CodeFamily family, std::string &error)
    {
        for (int prn = 1; prn <= lunanet::MAX_PRNS; ++prn)
        {
            std::vector<uint8_t> code;
            switch (family)
            {
            case CodeFamily::Gold:
                code = lunanet::generate_gold_code(prn);
                break;
            case CodeFamily::WeilPrimary:
                code = lunanet::generate_weil_primary(prn);
                break;
            case CodeFamily::WeilTertiary:
                code = lunanet::generate_weil_tertiary(prn);
                break;
            case CodeFamily::All:
                error = "Internal error: CodeFamily::All is not valid for WriteCodeFamily";
                return false;
            }

            if (code.empty())
            {
                error = "Failed to generate code for PRN " + std::to_string(prn) +
                        ": " + lunanet::get_last_error();
                return false;
            }

            out_stream << lunanet::chips_to_hex(code, 0) << "\n";
        }

        return true;
    }

    const char *CodeFamilyFileName(CodeFamily family)
    {
        switch (family)
        {
        case CodeFamily::Gold:
            return "gold_codes.txt";
        case CodeFamily::WeilPrimary:
            return "weil_primary_codes.txt";
        case CodeFamily::WeilTertiary:
            return "weil_tertiary_codes.txt";
        case CodeFamily::All:
            return "all_codes.txt";
        }
        return "codes.txt";
    }

    // ── Subcommand: generate-codes ──────────────────────────────────────────────

    int CmdGenerateCodes(Args &a)
    {
        std::string output;
        std::string config;
        std::string codes = "gold";

        while (HasNext(a))
        {
            if (GetString(a, "--output", output))
                continue;
            if (GetString(a, "--config", config))
                continue;
            if (GetString(a, "--codes", codes))
                continue;
            std::cerr << "error: unknown option: " << Peek(a) << "\n";
            return 1;
        }

        config = FindConfigPath(config);
        lunanet::initialize_engine();
        std::string err;
        if (!lunanet::load_spreading_code_config(config, &err))
        {
            std::cerr << "error: failed to load config: " << err << "\n";
            return 1;
        }

        CodeFamily family;
        if (!ParseCodeFamily(codes, family))
        {
            std::cerr << "error: invalid --codes value: " << codes << "\n"
                      << "       expected one of: gold, weil-primary, weil-tertiary, all\n";
            return 1;
        }

        if (family == CodeFamily::All)
        {
            namespace fs = std::filesystem;
            const fs::path output_dir = output.empty() ? fs::path(".") : fs::path(output);

            std::error_code fs_error;
            bool dir_exists = fs::exists(output_dir, fs_error);
            if (fs_error)
            {
                std::cerr << "error: cannot check output path: "
                          << output_dir.string() << ": " << fs_error.message() << "\n";
                return 1;
            }
            if (dir_exists)
            {
                bool is_dir = fs::is_directory(output_dir, fs_error);
                if (fs_error)
                {
                    std::cerr << "error: cannot check output path: "
                              << output_dir.string() << ": " << fs_error.message() << "\n";
                    return 1;
                }
                if (!is_dir)
                {
                    std::cerr << "error: for --codes all, --output must be a directory: "
                              << output_dir.string() << "\n";
                    return 1;
                }
            }
            else
            {
                fs::create_directories(output_dir, fs_error);
                if (fs_error)
                {
                    std::cerr << "error: cannot create output directory: "
                              << output_dir.string() << ": " << fs_error.message() << "\n";
                    return 1;
                }
            }

            for (CodeFamily current_family : {CodeFamily::Gold, CodeFamily::WeilPrimary, CodeFamily::WeilTertiary})
            {
                const fs::path file_path = output_dir / CodeFamilyFileName(current_family);
                std::ofstream file_stream(file_path);
                if (!file_stream)
                {
                    std::cerr << "error: cannot open output file: " << file_path.string() << "\n";
                    return 1;
                }
                if (!WriteCodeFamily(file_stream, current_family, err))
                {
                    std::cerr << "error: " << err << "\n";
                    return 1;
                }
            }
            return 0;
        }

        std::ostream *out_stream = &std::cout;
        std::ofstream file_stream;
        if (!output.empty())
        {
            file_stream.open(output);
            if (!file_stream)
            {
                std::cerr << "error: cannot open output file: " << output << "\n";
                return 1;
            }
            out_stream = &file_stream;
        }

        if (!WriteCodeFamily(*out_stream, family, err))
        {
            std::cerr << "error: " << err << "\n";
            return 1;
        }

        return 0;
    }

    // ── Subcommand: encode ──────────────────────────────────────────────────────

    void PrintEncodeUsage()
    {
        std::cerr
            << "usage: goon encode --format <frame|iq32> --prn <N>\n"
            << "       --fid <N> --toi <N> --wn <N> --itow <N>\n"
            << "       [--ced <hex>] [--rate <hz>] [--output <path>]\n"
            << "       [--config <path>] [--csv-dir <path>]\n";
    }

    int CmdEncode(Args &a)
    {
        std::string format;
        int prn = -1, fid = -1, toi = -1, wn = -1, itow = -1;
        int rate = lunanet::gateway4::kAfsIChipRateHz; // default: 1.023 MHz
        std::string ced_hex;
        std::string output;
        std::string codes;
        std::string config;
        std::string csv_dir;

        while (HasNext(a))
        {
            if (GetString(a, "--format", format))
                continue;
            if (GetInt(a, "--prn", prn))
                continue;
            if (GetInt(a, "--fid", fid))
                continue;
            if (GetInt(a, "--toi", toi))
                continue;
            if (GetInt(a, "--wn", wn))
                continue;
            if (GetInt(a, "--itow", itow))
                continue;
            if (GetInt(a, "--rate", rate))
                continue;
            if (GetString(a, "--ced", ced_hex))
                continue;
            if (GetString(a, "--output", output))
                continue;
            if (GetString(a, "--codes", codes))
            {
                if (codes != "gold")
                {
                    std::cerr << "error: encode only supports Gold codes; got: " << codes << "\n";
                    return 1;
                }
                // gold is the only supported value; accept and continue
                continue;
            }
            if (GetString(a, "--config", config))
                continue;
            if (GetString(a, "--csv-dir", csv_dir))
                continue;
            std::cerr << "error: unknown option: " << Peek(a) << "\n";
            PrintEncodeUsage();
            return 1;
        }

        if (a.parse_error)
        {
            return 1;
        }

        if (format.empty())
        {
            std::cerr << "error: --format is required (frame or iq32)\n";
            PrintEncodeUsage();
            return 1;
        }
        if (format != "frame" && format != "iq32")
        {
            std::cerr << "error: --format must be 'frame' or 'iq32', got: " << format << "\n";
            return 1;
        }
        if (prn < 1 || prn > lunanet::MAX_PRNS)
        {
            std::cerr << "error: --prn must be 1–" << lunanet::MAX_PRNS << "\n";
            return 1;
        }
        if (fid < 0 || fid > 3)
        {
            std::cerr << "error: --fid must be 0–3\n";
            return 1;
        }
        if (toi < 0 || toi > 99)
        {
            std::cerr << "error: --toi must be 0–99\n";
            return 1;
        }
        if (wn < 0 || wn > 8191)
        {
            std::cerr << "error: --wn must be 0–8191\n";
            return 1;
        }
        if (itow < 0 || itow > 511)
        {
            std::cerr << "error: --itow must be 0–511\n";
            return 1;
        }

        // ── Initialize engine and load LDPC matrices ───────────────────────────
        config = FindConfigPath(config);
        lunanet::initialize_engine();
        std::string err;
        if (!lunanet::load_spreading_code_config(config, &err))
        {
            std::cerr << "error: failed to load config: " << err << "\n";
            return 1;
        }

        csv_dir = FindAnnex3CsvDir(csv_dir);
        lunanet::gateway3::FrameMatrices matrices;
        if (!lunanet::gateway3::LoadFrameMatrices(csv_dir, &matrices, &err))
        {
            std::cerr << "error: failed to load LDPC matrices from " << csv_dir
                      << ": " << err << "\n";
            return 1;
        }

        // ── Build frame input ──────────────────────────────────────────────────
        lunanet::gateway3::FrameInput frame_input;
        frame_input.fid = static_cast<uint8_t>(fid);
        frame_input.toi = static_cast<uint8_t>(toi);

        frame_input.sb2.wn = static_cast<uint16_t>(wn);
        frame_input.sb2.itow = static_cast<uint16_t>(itow);
        frame_input.sb2.toi = static_cast<uint8_t>(toi);
        if (!ced_hex.empty())
        {
            frame_input.sb2.payload_bits = ParseHexPayload(ced_hex);
        }
        // SB3 and SB4 default to type=0, empty payload (spare-filled by builders).
        frame_input.sb3.type = 0;
        frame_input.sb4.type = 0;

        // ── Assemble frame ─────────────────────────────────────────────────────
        std::cerr << "Assembling frame: PRN=" << prn << " FID=" << fid
                  << " TOI=" << toi << " WN=" << wn << " ITOW=" << itow << "\n";

        auto frame = lunanet::gateway3::AssembleFrame(frame_input, matrices, &err);
        if (frame.empty())
        {
            std::cerr << "error: frame assembly failed: " << err << "\n";
            return 1;
        }

        std::cerr << "Frame assembled: " << frame.size() << " symbols\n";

        // ── Output frame ───────────────────────────────────────────────────────
        if (format == "frame")
        {
            if (output.empty())
                output = "frame.bin";
            if (!lunanet::gateway3::ExportFrameRaw(frame, output, &err))
            {
                std::cerr << "error: frame export failed: " << err << "\n";
                return 1;
            }
            std::cerr << "Frame written to " << output << " ("
                      << frame.size() << " bytes)\n";
            return 0;
        }

        // ── format == "iq32": full signal generation pipeline ──────────────────
        if (output.empty())
            output = "signal.iq32";

        // Step 1: Generate spreading codes for this PRN.
        auto gold_code = lunanet::generate_gold_code(prn);
        if (gold_code.empty())
        {
            std::cerr << "error: Gold code generation failed: "
                      << lunanet::get_last_error() << "\n";
            return 1;
        }

        // AFS-I: data-modulate the frame symbols onto the Gold primary code.
        // Each of the 6000 symbols gets XOR'd across one full 2046-chip epoch.
        auto afs_i_chips = lunanet::gateway4::ModulateAfsIData(gold_code, frame, &err);
        if (afs_i_chips.empty())
        {
            std::cerr << "error: AFS-I modulation failed: " << err << "\n";
            return 1;
        }
        std::cerr << "AFS-I: " << afs_i_chips.size() << " chips ("
                  << afs_i_chips.size() / lunanet::gateway4::kAfsIPrimaryChips
                  << " symbols × " << lunanet::gateway4::kAfsIPrimaryChips << " chips)\n";

        // AFS-Q: generate tiered pilot code for the same time span.
        // The Q channel runs at 5× the I chip rate, so we need 5× the I chips.
        const size_t afs_q_chip_count =
            afs_i_chips.size() * static_cast<size_t>(lunanet::gateway4::kQOverIChipRatio);
        auto afs_q_chips = lunanet::generate_afs_q(prn, afs_q_chip_count);
        if (afs_q_chips.empty())
        {
            std::cerr << "error: AFS-Q code generation failed: "
                      << lunanet::get_last_error() << "\n";
            return 1;
        }
        std::cerr << "AFS-Q: " << afs_q_chips.size() << " chips\n";

        // Step 2: Generate complex baseband I/Q.
        lunanet::gateway4::IqConfig iq_config;
        iq_config.sample_rate_hz = rate;
        auto signal = lunanet::gateway4::GenerateIq(afs_i_chips, afs_q_chips, iq_config, &err);
        if (signal.i.empty())
        {
            std::cerr << "error: I/Q generation failed: " << err << "\n";
            return 1;
        }
        std::cerr << "I/Q signal: " << signal.i.size() << " samples at "
                  << signal.sample_rate_hz << " Hz\n";

        // Step 3: Export as interleaved float32 LE binary.
        if (!lunanet::gateway4::ExportIqBinary(signal, output, &err))
        {
            std::cerr << "error: I/Q export failed: " << err << "\n";
            return 1;
        }

        const size_t file_bytes = signal.i.size() * 2 * sizeof(float);
        std::cerr << "Signal written to " << output << " ("
                  << file_bytes << " bytes, "
                  << (file_bytes / (1024 * 1024)) << " MB)\n";
        return 0;
    }

    // ── Subcommand: decode ──────────────────────────────────────────────────────

    void PrintDecodeUsage()
    {
        std::cerr
            << "usage: goon decode --input <path> [--input-format <raw|standard>]\n"
            << "       [--prn <N>] [--rate <hz>] [--noise-variance <value>]\n"
            << "       [--psr-threshold <value>] [--peak-rms-threshold <value>]\n"
            << "       [--normalized-peak-threshold <value>]\n"
            << "       [--lock-threshold <value>]\n"
            << "       [--max-iterations <1-50>] [--alpha <value>]\n"
            << "       [--output <path>] [--config <path>] [--csv-dir <path>]\n";
    }

    std::string BitsToHex(const std::vector<uint8_t> &bits)
    {
        static constexpr char kHex[] = "0123456789abcdef";
        std::string out;
        out.reserve((bits.size() + 3u) / 4u);
        for (std::size_t start = 0; start < bits.size(); start += 4u)
        {
            uint8_t nibble = 0u;
            for (std::size_t offset = 0; offset < 4u; ++offset)
            {
                nibble <<= 1u;
                if (start + offset < bits.size())
                {
                    nibble |= static_cast<uint8_t>(bits[start + offset] & 1u);
                }
            }
            out.push_back(kHex[nibble]);
        }
        return out;
    }

    std::size_t HexPaddingBits(const std::vector<uint8_t> &bits)
    {
        return (4u - (bits.size() % 4u)) % 4u;
    }

    void WriteDecodeJson(std::ostream &out,
                         int prn,
                         const lunanet::gateway5::FrameDecodeResult &decoded,
                         const lunanet::gateway6::Subframe1Data &sb1,
                         const lunanet::gateway6::Subframe2Data &sb2,
                         const lunanet::gateway6::Subframe3Data &sb3,
                         const lunanet::gateway6::Subframe4Data &sb4)
    {
        out << std::boolalpha << std::fixed << std::setprecision(3)
            << "{\n"
            << "  \"accepted\": " << decoded.accepted << ",\n"
            << "  \"prn\": " << prn << ",\n"
            << "  \"fid\": " << static_cast<int>(sb1.fid) << ",\n"
            << "  \"toi\": " << static_cast<int>(sb1.toi) << ",\n"
            << "  \"code_phase\": " << decoded.code_phase << ",\n"
            << "  \"frame_offset\": " << decoded.sync.frame_offset << ",\n"
            << "  \"lock_correlation\": " << decoded.lock_correlation << ",\n"
            << "  \"sync_psr\": " << decoded.sync.psr << ",\n"
            << "  \"bch_normalized_correlation\": "
            << decoded.sb1_bch.normalized_correlation << ",\n"
            << "  \"bch_normalized_margin\": " << decoded.sb1_bch.normalized_margin << ",\n"
            << "  \"decode_ms\": " << decoded.elapsed_ms << ",\n"
            << "  \"subframes\": {\n"
            << "    \"sb1\": {\"fid\": " << static_cast<int>(sb1.fid)
            << ", \"toi\": " << static_cast<int>(sb1.toi) << "},\n"
            << "    \"sb2\": {\"wn\": " << sb2.wn
            << ", \"itow\": " << sb2.itow
            << ", \"toi\": " << static_cast<int>(sb2.toi)
            << ", \"time_of_transmission_seconds\": "
            << sb2.time_of_transmission_seconds
            << ", \"ced\": {\"provisional_layout\": " << sb2.ced.provisional_layout
            << ", \"af0\": " << sb2.ced.af0
            << ", \"af1\": " << sb2.ced.af1
            << ", \"raw_bit_count\": " << sb2.ced.raw_bits.size()
            << ", \"raw_hex\": \"" << BitsToHex(sb2.ced.raw_bits) << "\"}"
            << ", \"health\": {\"provisional_layout\": " << sb2.health.provisional_layout
            << ", \"status\": " << static_cast<int>(sb2.health.status)
            << ", \"raw_bit_count\": " << sb2.health.raw_bits.size()
            << ", \"raw_hex\": \"" << BitsToHex(sb2.health.raw_bits) << "\"}"
            << ", \"time_conversions\": {\"provisional_layout\": "
            << sb2.time_conversions.provisional_layout
            << ", \"raw_bit_count\": " << sb2.time_conversions.raw_bits.size()
            << ", \"raw_hex\": \"" << BitsToHex(sb2.time_conversions.raw_bits) << "\"}"
            << ", \"spare_bit_count\": " << sb2.spare_bits.size() << "},\n"
            << "    \"sb3\": {\"type\": " << static_cast<int>(sb3.type)
            << ", \"payload_bit_count\": " << sb3.raw_payload.size() << "},\n"
            << "    \"sb4\": {\"type\": " << static_cast<int>(sb4.type)
            << ", \"payload_bit_count\": " << sb4.raw_payload.size()
            << ", \"network_access\": {\"requires_lnsp_sisicd\": "
            << sb4.requires_lnsp_sisicd
            << ", \"raw_bit_count\": " << sb4.network_access_payload.size()
            << ", \"raw_hex\": \"" << BitsToHex(sb4.network_access_payload) << "\"}}\n"
            << "  },\n"
            << "  \"ldpc_iterations\": {\"sb2\": " << decoded.sb2_ldpc.iterations
            << ", \"sb3\": " << decoded.sb3_ldpc.iterations
            << ", \"sb4\": " << decoded.sb4_ldpc.iterations << "},\n"
            << "  \"crc_valid\": {\"sb2\": " << decoded.crc.sb2.valid
            << ", \"sb3\": " << decoded.crc.sb3.valid
            << ", \"sb4\": " << decoded.crc.sb4.valid << "},\n"
            << "  \"sb2_bit_count\": " << decoded.sb2_payload.size() << ",\n"
            << "  \"sb3_bit_count\": " << decoded.sb3_payload.size() << ",\n"
            << "  \"sb4_bit_count\": " << decoded.sb4_payload.size() << ",\n"
            << "  \"sb2_hex_padding_bits\": " << HexPaddingBits(decoded.sb2_payload) << ",\n"
            << "  \"sb3_hex_padding_bits\": " << HexPaddingBits(decoded.sb3_payload) << ",\n"
            << "  \"sb4_hex_padding_bits\": " << HexPaddingBits(decoded.sb4_payload) << ",\n"
            << "  \"sb2_hex\": \"" << BitsToHex(decoded.sb2_payload) << "\",\n"
            << "  \"sb3_hex\": \"" << BitsToHex(decoded.sb3_payload) << "\",\n"
            << "  \"sb4_hex\": \"" << BitsToHex(decoded.sb4_payload) << "\"\n"
            << "}\n";
    }

    int CmdDecode(Args &a)
    {
        std::string input;
        std::string input_format = "raw";
        std::string output;
        std::string config_path;
        std::string csv_dir;
        int prn = -1;
        int rate = lunanet::gateway4::kAfsIChipRateHz;
        int max_iterations = 50;
        bool prn_was_set = false;
        bool rate_was_set = false;
        double noise_variance = 1.0;
        double psr_threshold = lunanet::gateway5::kDefaultSyncPsrThreshold;
        double peak_rms_threshold = lunanet::gateway5::kDefaultSyncPeakToRmsThreshold;
        double normalized_peak_threshold =
            lunanet::gateway5::kDefaultSyncNormalizedPeakThreshold;
        double lock_threshold = 0.5;
        double alpha = 0.75;

        while (HasNext(a))
        {
            if (GetString(a, "--input", input))
                continue;
            if (GetString(a, "--input-format", input_format))
                continue;
            if (GetInt(a, "--prn", prn))
            {
                prn_was_set = true;
                continue;
            }
            if (GetInt(a, "--rate", rate))
            {
                rate_was_set = true;
                continue;
            }
            if (GetDouble(a, "--noise-variance", noise_variance))
                continue;
            if (GetDouble(a, "--psr-threshold", psr_threshold))
                continue;
            if (GetDouble(a, "--peak-rms-threshold", peak_rms_threshold))
                continue;
            if (GetDouble(a, "--normalized-peak-threshold", normalized_peak_threshold))
                continue;
            if (GetDouble(a, "--lock-threshold", lock_threshold))
                continue;
            if (GetInt(a, "--max-iterations", max_iterations))
                continue;
            if (GetDouble(a, "--alpha", alpha))
                continue;
            if (GetString(a, "--output", output))
                continue;
            if (GetString(a, "--config", config_path))
                continue;
            if (GetString(a, "--csv-dir", csv_dir))
                continue;
            std::cerr << "error: unknown option: " << Peek(a) << "\n";
            PrintDecodeUsage();
            return 1;
        }

        if (a.parse_error)
        {
            return 1;
        }

        if (input.empty())
        {
            std::cerr << "error: --input is required\n";
            PrintDecodeUsage();
            return 1;
        }
        if (input_format != "raw" && input_format != "standard")
        {
            std::cerr << "error: --input-format must be 'raw' or 'standard'\n";
            return 1;
        }

        std::string error;
        lunanet::gateway4::IqSignal signal;
        if (input_format == "standard")
        {
            lunanet::gateway4::IqFileHeader header;
            if (!lunanet::gateway4::ImportIqBinaryStandard(input, &signal, &header, &error))
            {
                std::cerr << "error: failed to import standardized I/Q: " << error << "\n";
                return 1;
            }
            if (!prn_was_set)
            {
                prn = static_cast<int>(header.prn);
            }
            else if (header.prn != static_cast<uint32_t>(prn))
            {
                std::cerr << "error: --prn does not match standardized I/Q metadata\n";
                return 1;
            }
            if (rate_was_set && signal.sample_rate_hz != rate)
            {
                std::cerr << "error: --rate does not match standardized I/Q metadata\n";
                return 1;
            }
        }
        else
        {
            if (!lunanet::gateway4::ImportIqBinary(input, rate, &signal, &error))
            {
                std::cerr << "error: failed to import headerless IQ32: " << error << "\n";
                return 1;
            }
        }

        if (prn < 1 || prn > lunanet::gateway1::kMaxPrns)
        {
            std::cerr << "error: --prn must be 1-" << lunanet::gateway1::kMaxPrns
                      << " (required for raw IQ32)\n";
            return 1;
        }

        config_path = FindConfigPath(config_path);
        lunanet::gateway1::SpreadingSpecTables spreading_tables;
        lunanet::gateway1::Annex3Paths annex3_paths;
        if (!lunanet::gateway1::LoadSpreadingConfig(
                config_path, &spreading_tables, &annex3_paths, &error))
        {
            std::cerr << "error: failed to load spreading config: " << error << "\n";
            return 1;
        }

        csv_dir = FindAnnex3CsvDir(csv_dir);
        lunanet::gateway5::DecoderMatrices matrices;
        if (!lunanet::gateway5::LoadDecoderMatrices(csv_dir, &matrices, &error))
        {
            std::cerr << "error: failed to load decoder matrices: " << error << "\n";
            return 1;
        }

        lunanet::gateway5::FrameDecoderConfig decoder_config;
        decoder_config.prn = prn;
        decoder_config.symbol_noise_variance = noise_variance;
        decoder_config.sync_psr_threshold = psr_threshold;
        decoder_config.sync_peak_to_rms_threshold = peak_rms_threshold;
        decoder_config.sync_normalized_peak_threshold = normalized_peak_threshold;
        decoder_config.lock_threshold = lock_threshold;
        decoder_config.max_ldpc_iterations = max_iterations;
        decoder_config.ldpc_alpha = alpha;

        const auto decoded = lunanet::gateway5::DecodeAfsIIqSignal(
            signal, spreading_tables, matrices, decoder_config);
        if (!decoded.accepted)
        {
            std::cerr << "error: frame decode failed: " << decoded.error << "\n"
                      << "  code_phase=" << decoded.code_phase
                      << " lock=" << decoded.lock_correlation
                      << " frame_offset=" << decoded.sync.frame_offset
                      << " psr=" << decoded.sync.psr
                      << " elapsed_ms=" << decoded.elapsed_ms << "\n";
            return 2;
        }

        lunanet::gateway6::Subframe1Data sb1;
        if (!lunanet::gateway6::ParseSubframe1(
                static_cast<uint16_t>(decoded.sb1_value), &sb1, &error))
        {
            std::cerr << "error: Gateway 6 SB1 parsing failed: " << error << "\n";
            return 2;
        }

        lunanet::gateway6::Subframe2Data sb2;
        if (!lunanet::gateway6::ParseSubframe2(decoded.sb2_payload, &sb2, &error))
        {
            std::cerr << "error: Gateway 6 SB2 parsing failed: " << error << "\n";
            return 2;
        }

        lunanet::gateway6::Subframe3Data sb3;
        if (!lunanet::gateway6::ParseSubframe3(decoded.sb3_payload, &sb3, &error))
        {
            std::cerr << "error: Gateway 6 SB3 parsing failed: " << error << "\n";
            return 2;
        }

        lunanet::gateway6::Subframe4Data sb4;
        if (!lunanet::gateway6::ParseSubframe4(decoded.sb4_payload, &sb4, &error))
        {
            std::cerr << "error: Gateway 6 SB4 parsing failed: " << error << "\n";
            return 2;
        }

        if (output.empty())
        {
            WriteDecodeJson(std::cout, prn, decoded, sb1, sb2, sb3, sb4);
        }
        else
        {
            std::ofstream out(output);
            if (!out)
            {
                std::cerr << "error: failed to open decode output: " << output << "\n";
                return 1;
            }
            WriteDecodeJson(out, prn, decoded, sb1, sb2, sb3, sb4);
            if (!out)
            {
                std::cerr << "error: failed to write decode output: " << output << "\n";
                return 1;
            }
        }

        return 0;
    }

    // ── Usage ───────────────────────────────────────────────────────────────────

    void PrintUsage(const char *prog)
    {
        std::cerr
            << "LSIS-AFS CLI tool v" << lunanet::get_version() << "\n\n"
            << "usage: " << prog << " <command> [options]\n\n"
            << "commands:\n"
            << "  generate-codes  Generate 210 spreading codes (Gold/Weil/Tertiary)\n"
            << "  encode          Encode navigation frame / generate I/Q signal\n"
            << "  decode          Decode a navigation frame from an I/Q signal\n"
            << "  version         Print version\n\n"
            << "examples:\n"
            << "  " << prog << " generate-codes --output codes.txt\n"
            << "  " << prog << " generate-codes --codes gold --output gold_codes.txt\n"
            << "  " << prog << " generate-codes --codes all  --output generated/\n"
            << "  " << prog << " encode --format frame --prn 1 --fid 0 --toi 42 --wn 100 --itow 250\n"
            << "  " << prog << " encode --format iq32  --prn 1 --fid 0 --toi 42 --wn 100 --itow 250\n"
            << "  " << prog << " decode --input signal.iq32 --prn 1\n";
    }

} // namespace

// ── Entry point ─────────────────────────────────────────────────────────────

int main(int argc, char **argv)
{
    InitializeExecutableDir(argc > 0 ? argv[0] : nullptr);

    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    Args a{argc, argv, 1};
    const char *cmd = Next(a);

    if (std::strcmp(cmd, "version") == 0)
    {
        std::cout << "goon " << lunanet::get_version() << "\n";
        return 0;
    }

    if (std::strcmp(cmd, "generate-codes") == 0)
    {
        return CmdGenerateCodes(a);
    }

    if (std::strcmp(cmd, "encode") == 0)
    {
        return CmdEncode(a);
    }

    if (std::strcmp(cmd, "decode") == 0)
    {
        return CmdDecode(a);
    }

    if (std::strcmp(cmd, "help") == 0 || std::strcmp(cmd, "--help") == 0 ||
        std::strcmp(cmd, "-h") == 0)
    {
        PrintUsage(argv[0]);
        return 0;
    }

    std::cerr << "error: unknown command: " << cmd << "\n";
    PrintUsage(argv[0]);
    return 1;
}
