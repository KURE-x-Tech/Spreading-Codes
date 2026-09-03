#include "gateway6/subframe4_parser.h"
#include "gateway3/subframe4_builder.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

    bool TestRoundTrip(uint8_t type)
    {
        lunanet::gateway3::Subframe4Data input;
        input.type = type;
        input.payload_bits = {1u, 0u, 1u, 1u, 0u};
        const auto packed = lunanet::gateway3::PackSubframe4(input);

        lunanet::gateway6::Subframe4Data output;
        std::string error;
        if (!lunanet::gateway6::ParseSubframe4(packed, &output, &error))
        {
            std::cerr << "FAIL [round-trip]: " << error << "\n";
            return false;
        }
        if (output.type != type || output.raw_payload.size() != lunanet::gateway6::kSb4PayloadBits ||
            output.network_access_payload != output.raw_payload || !output.requires_lnsp_sisicd ||
            output.raw_payload[0] != 1u || output.raw_payload[1] != 0u ||
            output.raw_payload[2] != 1u || output.raw_payload[3] != 1u ||
            output.raw_payload[4] != 0u)
        {
            std::cerr << "FAIL [round-trip]: type or payload mismatch\n";
            return false;
        }
        return true;
    }

    bool TestRejectsInvalidInput()
    {
        lunanet::gateway6::Subframe4Data output;
        std::string error;
        if (lunanet::gateway6::ParseSubframe4(
                std::vector<uint8_t>(lunanet::gateway6::kSb4DataBits - 1, 0u),
                &output, &error) ||
            error.empty())
        {
            return false;
        }

        std::vector<uint8_t> non_binary(lunanet::gateway6::kSb4DataBits, 0u);
        non_binary[10] = 2u;
        error.clear();
        return !lunanet::gateway6::ParseSubframe4(non_binary, &output, &error) && !error.empty();
    }

} // namespace

int main()
{
    const bool ok = TestRoundTrip(0) &&
                    TestRoundTrip(static_cast<uint8_t>((1u << lunanet::gateway6::kSb4TypeFieldBits) - 1u)) &&
                    TestRejectsInvalidInput();
    std::cout << (ok ? "PASS" : "FAIL") << ": Gateway 6 SB4 parser\n";
    return ok ? 0 : 1;
}