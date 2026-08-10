#include "gateway2/ldpc_encoder.h"
#include "gateway2/crc24.h"
#include "gateway5/crc_validator.h"
#include "gateway5/ldpc_decoder.h"

#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

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

std::vector<uint8_t> RandomBits(int n, std::mt19937* rng) {
    std::vector<uint8_t> out(static_cast<std::size_t>(n), 0u);
    std::uniform_int_distribution<int> d(0, 1);
    for (int i = 0; i < n; ++i) {
        out[static_cast<std::size_t>(i)] = static_cast<uint8_t>(d(*rng));
    }
    return out;
}

std::vector<double> BitsToStrongLlrs(const std::vector<uint8_t>& bits) {
    std::vector<double> llrs;
    llrs.reserve(bits.size());
    for (const uint8_t b : bits) {
        llrs.push_back((b == 0u) ? +8.0 : -8.0);
    }
    return llrs;
}

bool DecodeToSystematic(const std::vector<uint8_t>& systematic_bits,
                        const MatrixBundle& matrices,
                        const lunanet::gateway2::LdpcParams& params,
                        std::vector<uint8_t>* out_systematic,
                        std::string* error) {
    const auto encoded = lunanet::gateway2::LdpcEncode(systematic_bits, matrices.enc, params, error);
    if (encoded.empty()) {
        if (error && error->empty()) {
            *error = "LdpcEncode returned empty output.";
        }
        return false;
    }

    const auto llrs = BitsToStrongLlrs(encoded);

    const auto decoded = lunanet::gateway5::DecodeLdpcMinSum(
        llrs,
        matrices.enc,
        matrices.b,
        params,
        50,
        0.75,
        error);

    if (!decoded.converged) {
        if (error) {
            *error = "LDPC decoder failed to converge";
        }
        return false;
    }

    *out_systematic = decoded.decoded_data_bits;
    return true;
}

bool TestKnownCrcVector() {
    constexpr uint32_t kExpected = 0xCDE703u;
    const uint32_t got = lunanet::gateway5::ComputeCrc24QOverBytesMsbFirst("123456789");
    if (got != kExpected) {
        std::cerr << "FAIL [CRC vector]: got 0x" << std::hex << got
                  << ", expected 0x" << kExpected << std::dec << "\n";
        return false;
    }

    return true;
}

bool TestRejectsWrongLengthSubframe() {
    // Feed an obviously wrong-length systematic vector (not 1200 or 870 bits)
    // and confirm ValidateSubframeCrc rejects it gracefully with an error
    // message rather than reading out of bounds or crashing.
    const std::vector<uint8_t> bad(10, 0u);

    const auto verdict_sb2 = lunanet::gateway5::ValidateSubframeCrc(
        bad, lunanet::gateway5::SubframeCrcType::Sb2);
    if (verdict_sb2.valid || verdict_sb2.error.empty()) {
        std::cerr << "FAIL [wrong-length subframe]: SB2 should reject with an error message\n";
        return false;
    }

    const auto verdict_sb3 = lunanet::gateway5::ValidateSubframeCrc(
        bad, lunanet::gateway5::SubframeCrcType::Sb3);
    if (verdict_sb3.valid || verdict_sb3.error.empty()) {
        std::cerr << "FAIL [wrong-length subframe]: SB3 should reject with an error message\n";
        return false;
    }

    return true;
}

bool TestRejectsNonBinarySubframe() {
    std::vector<uint8_t> bad(1200, 0u);
    bad[117] = 2u;
    const auto verdict = lunanet::gateway5::ValidateSubframeCrc(
        bad, lunanet::gateway5::SubframeCrcType::Sb2);
    if (verdict.valid || verdict.error.empty()) {
        std::cerr << "FAIL [non-binary subframe]: expected rejection\n";
        return false;
    }
    return true;
}

bool TestFrameCrcGate(const MatrixBundle& sb2,
                      const MatrixBundle& sb34,
                      std::mt19937* rng) {
    std::string error;

    std::vector<uint8_t> sb2_s;
    std::vector<uint8_t> sb3_s;
    std::vector<uint8_t> sb4_s;

        std::vector<uint8_t> sb2_data = RandomBits(1176, rng);
        lunanet::gateway2::Crc24Append(sb2_data);  // 1176 -> 1200

        std::vector<uint8_t> sb3_data = RandomBits(846, rng);
        lunanet::gateway2::Crc24Append(sb3_data);  // 846 -> 870

        std::vector<uint8_t> sb4_data = RandomBits(846, rng);
        lunanet::gateway2::Crc24Append(sb4_data);  // 846 -> 870

        if (!DecodeToSystematic(
            sb2_data,
            sb2,
            lunanet::gateway2::kLdpcSb2,
            &sb2_s,
            &error)) {
        std::cerr << "FAIL [SB2 decode path]: " << error << "\n";
        return false;
    }

        if (!DecodeToSystematic(
            sb3_data,
            sb34,
            lunanet::gateway2::kLdpcSb34,
            &sb3_s,
            &error)) {
        std::cerr << "FAIL [SB3 decode path]: " << error << "\n";
        return false;
    }

        if (!DecodeToSystematic(
            sb4_data,
            sb34,
            lunanet::gateway2::kLdpcSb34,
            &sb4_s,
            &error)) {
        std::cerr << "FAIL [SB4 decode path]: " << error << "\n";
        return false;
    }

    // SB3/SB4 must be filler-stripped by Stage 5 output (870, not 880).
    if (sb3_s.size() != 870u || sb4_s.size() != 870u) {
        std::cerr << "FAIL [filler strip]: SB3/SB4 systematic length mismatch\n";
        return false;
    }

    const auto frame = lunanet::gateway5::ValidateFrameCrc(sb2_s, sb3_s, sb4_s);
    if (!frame.sb2.valid || !frame.sb3.valid || !frame.sb4.valid || !frame.frame_accepted) {
        std::cerr << "FAIL [frame gate]: expected all CRC passes\n";
        return false;
    }

    // Flip one protected bit in SB3 data+spare portion and ensure frame reject.
    // NOTE: index 0 assumes the Stage 5 systematic output has no prepended
    // filler/metadata ahead of the data+spare field; update this index if
    // that layout changes.
    std::vector<uint8_t> sb3_bad = sb3_s;
    sb3_bad[0] ^= 1u;

    const auto bad_frame = lunanet::gateway5::ValidateFrameCrc(sb2_s, sb3_bad, sb4_s);
    if (bad_frame.sb3.valid || bad_frame.frame_accepted) {
        std::cerr << "FAIL [frame reject]: corrupted SB3 should fail CRC and reject frame\n";
        return false;
    }

    return true;
}

}  // namespace

int main() {
    bool ok = true;

    if (TestKnownCrcVector()) {
        std::cout << "PASS: CRC24Q(\"123456789\") = 0xCDE703\n";
    } else {
        ok = false;
    }

    if (TestRejectsWrongLengthSubframe()) {
        std::cout << "PASS: wrong-length subframe is rejected gracefully\n";
    } else {
        ok = false;
    }

    if (TestRejectsNonBinarySubframe()) {
        std::cout << "PASS: non-binary systematic data is rejected\n";
    } else {
        ok = false;
    }

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

    std::mt19937 rng(0xC0DEC24U);
    if (TestFrameCrcGate(sb2, sb34, &rng)) {
        std::cout << "PASS: per-subframe CRC verdicts and frame-level gate behave correctly\n";
    } else {
        ok = false;
    }

    return ok ? 0 : 1;
}
