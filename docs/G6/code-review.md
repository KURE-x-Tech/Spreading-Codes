# Findings

| ID | Requirements | Code found | Status |
| --- | --- | --- | --- |
| MP-6.1 | SB1 parser (FID+TOI) | `codes/gateway6/subframe1_parser.h/.cpp` + test (`codes/gateway6/subframe1_parser_test.cpp`) — parses FID (0..3) and TOI (0..99) per Table 13 & 14 from bit vectors and 9-bit words with boundary validation | Implemented |
| MP-6.2 | SB2 parser (WN+ITOW) | `codes/gateway6/subframe2_parser.h/.cpp` + test — extracts WN, ITOW, TOI, relative ToT, provisional raw CED and Time Conversions blocks, and health fields | Implemented |
| MP-6.3 | SB3 variable-data router (Table 19) | `codes/gateway6/subframe3_parser.h/.cpp` + test - routes to `OrbitAlmanacData`, unknown types pass through raw | Implemented |
| MP-6.4 | SB4 network-access parser (Table 20) | `codes/gateway6/subframe4_parser.h/.cpp` + test — validates the dynamic type field, extracts the raw network-access payload, and marks semantic decoding as requiring the LNSP SISICD | Implemented |
| MP-6.5 | Time of Transmission calc (WN, ITOW, TOI -> ToT) | `codes/gateway6/subframe2_parser.h/.cpp` computes relative seconds as `WN*604800 + ITOW*1200 + TOI*12`; `goon decode` emits it under `subframes.sb2.time_of_transmission_seconds` | Implemented |

The `goon decode` CLI routes accepted Gateway 5 output through all four parsers and emits the result under `subframes`. The spec itself marks the LRT start epoch as `{LSIS-TBD-2003}` (undefined), so MP-6.5 intentionally produces relative time-since-epoch rather than an absolute UTC/GPS-style timestamp.
