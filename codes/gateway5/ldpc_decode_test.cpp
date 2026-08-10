#include "gateway2/ldpc_encoder.h"
#include "gateway5/ldpc_decoder.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

// Named to match DecodeLdpcMinSum's own defaults (see ldpc_decoder.h) so the
// round-trip test's intent is explicit rather than a bare magic-number call.
constexpr int kMaxIterations = 50;
constexpr double kAlpha = 0.75;

struct MatrixBundle {
    lunanet::gateway2::LdpcMatrices enc;
    lunanet::gateway2::BinaryMatrix b;
};

bool LoadSb2Matrices(const std::filesystem::path& csv_dir,
                     MatrixBundle* out,
                     std::string* error) {
    if (!lunanet::gateway2::LoadLdpcMatrices(
            (csv_dir / "004h_lunanet_sf2_ldpc_submatrix_a_mat.csv").string(),
            (csv_dir / "004i_lunanet_sf2_ldpc_submatrix_b_inv_mat.csv").string(),
            (csv_dir / "004f_lunanet_sf2_ldpc_submatrix_c_mat.csv").string(),
            (csv_dir / "004g_lunanet_sf2_ldpc_submatrix_d_mat.csv").string(),
            &out->enc,
            error)) {
        return false;
    }

    if (!lunanet::gateway2::LoadBinaryMatrixCsv(
            (csv_dir / "004j_lunanet_sf2_ldpc_submatrix_b_mat.csv").string(),
            &out->b,
            error)) {
        return false;
    }

    return true;
}

bool LoadSb34Matrices(const std::filesystem::path& csv_dir,
                      MatrixBundle* out,
                      std::string* error) {
    if (!lunanet::gateway2::LoadLdpcMatrices(
            (csv_dir / "004a_lunanet_sf3_ldpc_submatrix_a_mat.csv").string(),
            (csv_dir / "004b_lunanet_sf3_ldpc_submatrix_b_inv_mat.csv").string(),
            (csv_dir / "004d_lunanet_sf3_ldpc_submatrix_c_mat.csv").string(),
            (csv_dir / "004e_lunanet_sf3_ldpc_submatrix_d_mat.csv").string(),
            &out->enc,
            error)) {
        return false;
    }

    if (!lunanet::gateway2::LoadBinaryMatrixCsv(
            (csv_dir / "004c_lunanet_sf3_ldpc_submatrix_b_mat.csv").string(),
            &out->b,
            error)) {
        return false;
    }

    return true;
}

std::vector<uint8_t> MakeRandomBits(int n, std::mt19937* rng) {
    std::vector<uint8_t> bits(static_cast<std::size_t>(n), 0u);
    std::uniform_int_distribution<int> d(0, 1);
    for (int i = 0; i < n; ++i) {
        bits[static_cast<std::size_t>(i)] = static_cast<uint8_t>(d(*rng));
    }
    return bits;
}

std::vector<double> MapBitsToNoiselessLlrs(const std::vector<uint8_t>& bits) {
    std::vector<double> llrs;
    llrs.reserve(bits.size());
    for (const uint8_t b : bits) {
        llrs.push_back((b == 0u) ? +8.0 : -8.0);
    }
    return llrs;
}

bool RunRoundTripCase(const char* tag,
                      const MatrixBundle& matrices,
                      const lunanet::gateway2::LdpcParams& params,
                      std::mt19937* rng) {
    std::string error;
    const auto src = MakeRandomBits(params.data_bits, rng);

    const auto encoded = lunanet::gateway2::LdpcEncode(src, matrices.enc, params, &error);
    if (encoded.empty()) {
        std::cerr << "FAIL [" << tag << "]: LdpcEncode failed: " << error << "\n";
        return false;
    }

    const auto rx_llrs = MapBitsToNoiselessLlrs(encoded);

    const auto decoded = lunanet::gateway5::DecodeLdpcMinSum(
        rx_llrs,
        matrices.enc,
        matrices.b,
        params,
        kMaxIterations,
        kAlpha,
        &error);

    if (!decoded.converged) {
        std::cerr << "FAIL [" << tag << "]: decoder did not converge in " << kMaxIterations << " iterations"
                  << " (syndrome=" << decoded.syndrome_weight << ")\n";
        return false;
    }

    if (decoded.decoded_data_bits.size() != src.size()) {
        std::cerr << "FAIL [" << tag << "]: decoded size mismatch\n";
        return false;
    }

    for (std::size_t i = 0; i < src.size(); ++i) {
        if (decoded.decoded_data_bits[i] != src[i]) {
            std::cerr << "FAIL [" << tag << "]: bit mismatch at " << i << "\n";
            return false;
        }
    }

    std::cout << "PASS: " << tag << " noiseless round-trip converged in "
              << decoded.iterations << " iterations\n";
    return true;
}

bool TestPunctureRestoreLengths(const MatrixBundle& sb2,
                                const MatrixBundle& sb34) {
    std::string error;

    std::vector<double> rx_sb2(static_cast<std::size_t>(lunanet::gateway2::kLdpcSb2.output_symbols), +1.0);
    const auto full_sb2 = lunanet::gateway5::RestorePuncturedCodewordLlrs(
        rx_sb2,
        lunanet::gateway2::kLdpcSb2,
        sb2.enc.a.rows,
        sb2.enc.c.rows,
        &error);
    if (full_sb2.empty()) {
        std::cerr << "FAIL [restore SB2]: " << error << "\n";
        return false;
    }

    std::vector<double> rx_sb34(static_cast<std::size_t>(lunanet::gateway2::kLdpcSb34.output_symbols), +1.0);
    const auto full_sb34 = lunanet::gateway5::RestorePuncturedCodewordLlrs(
        rx_sb34,
        lunanet::gateway2::kLdpcSb34,
        sb34.enc.a.rows,
        sb34.enc.c.rows,
        &error);
    if (full_sb34.empty()) {
        std::cerr << "FAIL [restore SB34]: " << error << "\n";
        return false;
    }

    const int expected_sb2 = lunanet::gateway2::kLdpcSb2.info_bits + sb2.enc.a.rows + sb2.enc.c.rows;
    const int expected_sb34 = lunanet::gateway2::kLdpcSb34.info_bits + sb34.enc.a.rows + sb34.enc.c.rows;

    if (static_cast<int>(full_sb2.size()) != expected_sb2 ||
        static_cast<int>(full_sb34.size()) != expected_sb34) {
        std::cerr << "FAIL [restore sizes]: full-codeword length mismatch\n";
        return false;
    }

    for (int i = 0; i < lunanet::gateway2::kLdpcSb34.puncture_z2; ++i) {
        if (full_sb34[static_cast<std::size_t>(i)] != 0.0) {
            std::cerr << "FAIL [restore puncture]: punctured bit is not an erasure\n";
            return false;
        }
    }
    for (int i = lunanet::gateway2::kLdpcSb34.data_bits;
         i < lunanet::gateway2::kLdpcSb34.info_bits;
         ++i) {
        if (full_sb34[static_cast<std::size_t>(i)] !=
            lunanet::gateway5::kShortenedZeroLlr) {
            std::cerr << "FAIL [restore filler]: shortened zero has no known-bit prior\n";
            return false;
        }
    }

    return true;
}

bool TestRejectsWrongSizeLlrs(const MatrixBundle& sb2) {
    // Feed a deliberately too-short LLR vector (not a full output_symbols
    // frame) and confirm the decoder fails gracefully with an error message
    // instead of crashing or silently producing garbage output.
    std::string error;
    const std::vector<double> bad_llrs(10, 1.0);

    const auto decoded = lunanet::gateway5::DecodeLdpcMinSum(
        bad_llrs,
        sb2.enc,
        sb2.b,
        lunanet::gateway2::kLdpcSb2,
        kMaxIterations,
        kAlpha,
        &error);

    if (decoded.converged || !decoded.decoded_data_bits.empty()) {
        std::cerr << "FAIL [wrong-size llrs]: expected graceful rejection, got a result\n";
        return false;
    }
    if (error.empty()) {
        std::cerr << "FAIL [wrong-size llrs]: expected an error message to be set\n";
        return false;
    }
    return true;
}

bool TestRejectsUnsafeInputs(const MatrixBundle& sb2) {
    std::string error;
    std::vector<double> llrs(
        static_cast<std::size_t>(lunanet::gateway2::kLdpcSb2.output_symbols), 1.0);
    llrs[5] = std::numeric_limits<double>::quiet_NaN();
    auto decoded = lunanet::gateway5::DecodeLdpcMinSum(
        llrs, sb2.enc, sb2.b, lunanet::gateway2::kLdpcSb2, 50, 0.75, &error);
    if (decoded.converged || error.empty()) {
        std::cerr << "FAIL [non-finite LLR]: expected rejection\n";
        return false;
    }

    llrs[5] = 1.0;
    error.clear();
    decoded = lunanet::gateway5::DecodeLdpcMinSum(
        llrs, sb2.enc, sb2.b, lunanet::gateway2::kLdpcSb2, 51, 0.75, &error);
    if (decoded.converged || error.empty()) {
        std::cerr << "FAIL [iteration limit]: expected rejection\n";
        return false;
    }

    auto bad_params = lunanet::gateway2::kLdpcSb2;
    --bad_params.info_bits;
    error.clear();
    decoded = lunanet::gateway5::DecodeLdpcMinSum(
        llrs, sb2.enc, sb2.b, bad_params, 50, 0.75, &error);
    if (decoded.converged || error.empty()) {
        std::cerr << "FAIL [parameter mismatch]: expected rejection\n";
        return false;
    }

    auto bad_b = sb2.b;
    bad_b.data.pop_back();
    error.clear();
    decoded = lunanet::gateway5::DecodeLdpcMinSum(
        llrs, sb2.enc, bad_b, lunanet::gateway2::kLdpcSb2, 50, 0.75, &error);
    if (decoded.converged || error.empty()) {
        std::cerr << "FAIL [matrix storage]: expected rejection\n";
        return false;
    }

    const std::vector<double> huge_finite_llrs(
        static_cast<std::size_t>(lunanet::gateway2::kLdpcSb2.output_symbols),
        std::numeric_limits<double>::max());
    error.clear();
    decoded = lunanet::gateway5::DecodeLdpcMinSum(
        huge_finite_llrs,
        sb2.enc,
        sb2.b,
        lunanet::gateway2::kLdpcSb2,
        50,
        0.75,
        &error);
    if (!decoded.converged ||
        !std::all_of(decoded.decoded_data_bits.begin(),
                     decoded.decoded_data_bits.end(),
                     [](const uint8_t bit) { return bit == 0u; })) {
        std::cerr << "FAIL [message saturation]: huge finite LLRs were unstable\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;
    std::string error;

    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::filesystem::path csv_dir = repo_root / "Validation" / "annex3" / "csv";

    MatrixBundle sb2;
    MatrixBundle sb34;

    if (!LoadSb2Matrices(csv_dir, &sb2, &error)) {
        std::cerr << "FAIL: load SB2 matrices: " << error << "\n";
        return 1;
    }
    if (!LoadSb34Matrices(csv_dir, &sb34, &error)) {
        std::cerr << "FAIL: load SB3/SB4 matrices: " << error << "\n";
        return 1;
    }

    if (TestPunctureRestoreLengths(sb2, sb34)) {
        std::cout << "PASS: puncture-restore preserves erasures and shortened-zero priors\n";
    } else {
        ok = false;
    }

    if (TestRejectsWrongSizeLlrs(sb2)) {
        std::cout << "PASS: wrong-size LLR vector is rejected gracefully\n";
    } else {
        ok = false;
    }

    if (TestRejectsUnsafeInputs(sb2)) {
        std::cout << "PASS: unsafe LDPC values, limits, params, and storage are rejected\n";
    } else {
        ok = false;
    }

    std::mt19937 rng(0x5A17U);

    if (!RunRoundTripCase("SB2", sb2, lunanet::gateway2::kLdpcSb2, &rng)) {
        ok = false;
    }
    if (!RunRoundTripCase("SB3/SB4", sb34, lunanet::gateway2::kLdpcSb34, &rng)) {
        ok = false;
    }

    return ok ? 0 : 1;
}
