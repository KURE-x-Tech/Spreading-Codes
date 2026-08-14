# Findings

| ID | Requirements | Code found | Status |
| --- | --- | --- | --- |
| MP-6.1 | SB1 parser (FID+TOI) | `codes/gateway6/subframe1_parser.h/.cpp` + test (`codes/gateway6/subframe1_parser_test.cpp`) — parses FID (0..3) and TOI (0..99) per Table 13 & 14 from bit vectors and 9-bit words with boundary validation | Implemented |
| MP-6.2 | SB2 parser (WN+ITOW) | `codes/gateway6/subframe2_parser.h/.cpp` + test - routes to `OrbitAlmanacData` | Implemented |
| MP-6.3 | SB3 variable-data router (Table 19) | `codes/gateway6/subframe3_parser.h/.cpp` + test - routes to `OrbitAlmanacData`, unknown types pass through raw | Implemented |
| MP-6.4 | SB4 network-access parser (Table 20) | No file exists. Only `codes/gateway3/subframe4_builder.h/.cpp` (encoder side) is present - nothing under `codes/gateway6/` parses SB4 | Missing |
| MP-6.5 | Time of Transmission calc (WN, ITOW, TOI → ToT) | No calculator function anywhere in the codebase | Missing |

The spec itself marks the LRT start epoch as `{LSIS-TBD-2003}` (undefined) so even once a ToT calculator is written, it can only produce time-since-epoch, not an absolute time, until that TBD is resolved upstream or told otherwise.
