#include "gateway5/symbol_extractor.h"

#include <cmath>
#include <stdexcept>

namespace lunanet::gateway5 {

ExtractedFrame ExtractFrameSymbols(const std::vector<double>& received,
                                   std::size_t frame_offset) {
    // Verify the input is long enough to hold a full 6000-symbol frame.
    if (frame_offset > received.size() ||
        received.size() - frame_offset < static_cast<std::size_t>(kFrameSymbols)) {
        return {};
    }

    const auto begin = received.cbegin() +
        static_cast<std::vector<double>::difference_type>(frame_offset);

    ExtractedFrame frame;
    frame.sp.assign(begin, begin + kSpSymbolsStage2);
    frame.sb1.assign(begin + kSpSymbolsStage2,
                     begin + kSpSymbolsStage2 + kSb1SymbolsStage2);
    frame.interleaved.assign(begin + kInterleavedStart,
                             begin + kFrameSymbols);

    return frame;
}

std::vector<double> ComputeLlr(const std::vector<double>& soft_values,
                                double noise_variance) {
    if (!std::isfinite(noise_variance) || noise_variance <= 0.0) {
        throw std::invalid_argument("noise_variance must be finite and > 0");
    }

    const double scale = 2.0 / noise_variance;
    if (!std::isfinite(scale)) {
        throw std::overflow_error("noise_variance is too small to produce finite LLRs");
    }

    std::vector<double> llrs;
    llrs.reserve(soft_values.size());
    for (const double r : soft_values) {
        if (!std::isfinite(r)) {
            throw std::invalid_argument("soft_values contains a non-finite value");
        }
        const double llr = scale * r;
        if (!std::isfinite(llr)) {
            throw std::overflow_error("LLR calculation overflowed");
        }
        llrs.push_back(llr);
    }
    return llrs;
}

}  // namespace lunanet::gateway5
