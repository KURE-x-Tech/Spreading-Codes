#include "gateway5/crc_validator.h"

#include "gateway2/crc24.h"
#include "gateway3/subframe2_builder.h"
#include "gateway3/subframe3_builder.h"
#include "gateway3/subframe4_builder.h"

namespace lunanet::gateway5 {
namespace {

int ExpectedTotalBits(SubframeCrcType type) {
    switch (type) {
        case SubframeCrcType::Sb2:
            return lunanet::gateway3::kSb2TotalBits;  // 1200
        case SubframeCrcType::Sb3:
            return lunanet::gateway3::kSb3TotalBits;  // 870 (filler stripped)
        case SubframeCrcType::Sb4:
            return lunanet::gateway3::kSb4TotalBits;  // 870 (filler stripped)
    }
    return -1;
}

uint32_t ReadCrc24MsbFirst(const std::vector<uint8_t>& bits, int start_index) {
    uint32_t crc = 0;
    for (int i = 0; i < 24; ++i) {
        crc = (crc << 1) | static_cast<uint32_t>(bits[static_cast<std::size_t>(start_index + i)] & 1u);
    }
    return crc;
}

}  // namespace

SubframeCrcVerdict ValidateSubframeCrc(const std::vector<uint8_t>& decoded_systematic_bits,
                                       SubframeCrcType type) {
    SubframeCrcVerdict verdict;
    verdict.total_bits = static_cast<int>(decoded_systematic_bits.size());

    const int expected_total = ExpectedTotalBits(type);
    if (verdict.total_bits != expected_total) {
        verdict.error = "Unexpected systematic length: got " + std::to_string(verdict.total_bits) +
            ", expected " + std::to_string(expected_total);
        return verdict;
    }

    for (std::size_t i = 0; i < decoded_systematic_bits.size(); ++i) {
        if (decoded_systematic_bits[i] > 1u) {
            verdict.error = "Non-binary systematic value at bit " + std::to_string(i);
            return verdict;
        }
    }

    verdict.data_bits = verdict.total_bits - 24;
    if (verdict.data_bits < 0) {
        verdict.error = "Subframe too short for CRC split";
        return verdict;
    }

    std::vector<uint8_t> data_only(
        decoded_systematic_bits.begin(),
        decoded_systematic_bits.begin() + verdict.data_bits);

    verdict.computed_crc = lunanet::gateway2::Crc24Compute(data_only);
    verdict.received_crc = ReadCrc24MsbFirst(decoded_systematic_bits, verdict.data_bits);
    verdict.valid = (verdict.computed_crc == verdict.received_crc);

    return verdict;
}

FrameCrcVerdict ValidateFrameCrc(const std::vector<uint8_t>& sb2_systematic_bits,
                                 const std::vector<uint8_t>& sb3_systematic_bits,
                                 const std::vector<uint8_t>& sb4_systematic_bits) {
    FrameCrcVerdict out;
    out.sb2 = ValidateSubframeCrc(sb2_systematic_bits, SubframeCrcType::Sb2);
    out.sb3 = ValidateSubframeCrc(sb3_systematic_bits, SubframeCrcType::Sb3);
    out.sb4 = ValidateSubframeCrc(sb4_systematic_bits, SubframeCrcType::Sb4);
    out.frame_accepted = out.sb2.valid && out.sb3.valid && out.sb4.valid;
    return out;
}

uint32_t ComputeCrc24QOverBytesMsbFirst(const std::string& ascii_bytes) {
    std::vector<uint8_t> bits;
    bits.reserve(ascii_bytes.size() * 8u);

    for (const unsigned char ch : ascii_bytes) {
        for (int b = 7; b >= 0; --b) {
            bits.push_back(static_cast<uint8_t>((ch >> b) & 1u));
        }
    }

    return lunanet::gateway2::Crc24Compute(bits);
}

}  // namespace lunanet::gateway5
