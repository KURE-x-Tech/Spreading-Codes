# LSIS-AFS Competition - Submission Report

---

## Team Information

| Field | Details |
| --- | --- |
| Team Name | KURE-x-Tech |
| Members | Jay Bell, Ashley Farnworth, Matt Trollip, Koki Ohira, Vakaris Steponavicius, Nabeel Naqvi, Emily Brown, Trish Fratangelo, |
| Institution / Affiliation | Kingston University |
| Contact Email | <jordanbell321@gmail.com> |
| Repository URL | [KURE-x-Tech/Spreading-Codes](https://github.com/KURE-x-Tech/Spreading-Codes) |

---

## Implementation Overview

### Language & Technology Stack

| Component | Choice |
| --- | --- |
| Primary Language | C++17 |
| Build System | CMake 3.16 or newer |
| Key Libraries | C++ standard library; Python standard library (`ctypes`, `tkinter`) for bridge and tooling |
| Test Framework | CTest, standalone C++ tests, and the `test_engine` Markdown/JUnit report harness |
| Platform(s) Tested | Windows, macOS, and Linux toolchains are documented; record the exact final-run configuration before submission. |

### Architecture Summary

The implementation is organized as gateway libraries under `codes/`. Gateway 1 generates Gold, Weil primary, Weil tertiary, secondary, and tiered AFS-Q spreading sequences using checked-in configuration, specification tables, and Annex 3 data. Gateway 2 implements BCH, CRC-24Q, LDPC encoding, and the 60x98 block interleaver. Gateway 3 builds SB1-SB4 and assembles the 6000-symbol navigation frame. Gateway 4 applies AFS-I data spreading and BPSK modulation, generates time-aligned AFS-Q, and exports I/Q samples.

Gateway 5 implements the receive-side path: I/Q import, PRN acquisition, AFS-I despreading, normalized frame synchronization, soft BCH/LDPC decoding, CRC validation, and accepted-payload handoff. Gateway 6 parses CRC-stripped SB1-SB4 payloads into structured navigation fields. The `goon` CLI is the operational interface, with a C API and zero-dependency Python `ctypes` bridge for integration. The receiver keeps soft values through FEC decoding and releases data to Gateway 6 only after all CRC checks succeed.

---

## Build & Run Instructions

### Prerequisites

- CMake 3.16 or newer.
- A C++17 compiler: Visual Studio 2019+, GCC 9+, or Clang 10+.
- Git.
- Python 3 only for the bridge smoke test, I/Q utility, or report viewer.

No third-party runtime library is required for the core C++ pipeline.

### Build

Single-configuration generators:

```bash
cmake -S . -B build-submission -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-submission --parallel
```

Windows Visual Studio:

```powershell
cmake -S . -B build-submission-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-submission-vs --config Release --parallel
```

### Run Tests

Single-configuration build:

```bash
ctest --test-dir build-submission --output-on-failure
./build-submission/bin/test_engine config/spreading_codes_config.ini
./build-submission/bin/gateway5_ber_benchmark
./build-submission/bin/gateway7_validation_test
```

Visual Studio build:

```powershell
ctest --test-dir build-submission-vs -C Release --output-on-failure
.\build-submission-vs\bin\Release\test_engine.exe config\spreading_codes_config.ini
.\build-submission-vs\bin\Release\gateway5_ber_benchmark.exe
.\build-submission-vs\bin\Release\gateway7_validation_test.exe
```

### Run Examples

Generate all supported code families:

```bash
./build-submission/bin/goon generate-codes --codes all --output Validation/generated/submission_codes
```

Generate and decode a headerless IQ32 signal:

```bash
./build-submission/bin/goon encode \
  --format iq32 --prn 7 --fid 2 --toi 73 --wn 1234 --itow 256 \
  --rate 1023000 --output Validation/iq_output/submission.iq32

./build-submission/bin/goon decode \
  --input Validation/iq_output/submission.iq32 --input-format raw \
  --prn 7 --rate 1023000 --output Validation/generated/submission_decoded.json
```

---

## Gateway Status

| Gateway | Status | Notes |
| --- | --- | --- |
| 0 - Design & Architecture | Complete | Modular gateway layout, CMake targets, data paths, public interfaces, and test strategy are documented. |
| 1 - Spreading Code Generation | Complete | Gold, Weil primary, Weil tertiary, secondary, tiered codes, and Table 11 assignments are implemented and Annex 3-tested. |
| 2 - Forward Error Correction | Complete | BCH, CRC-24Q, LDPC encoding and soft decoding, plus interleaving/deinterleaving, are implemented and tested. |
| 3 - Navigation Message Framing | Complete | Sync pattern, SB1-SB4 builders, CRC/LDPC processing, interleaving, and 6000-symbol assembly are implemented. |
| 4 - Baseband Signal Generation | Complete | BPSK, AFS-I data spreading, AFS-Q generation, chip-index mapping, and IQ32/CSV export are implemented. |
| 5 - Frame Sync & Decoding | Complete | `goon decode` implements the qualified known-PRN, AFS-I, integer-timing receive envelope. |
| 6 - Message Parsing | Partial | SB1-SB4 parsers and relative ToT computation are implemented; absolute timestamp conversion and some specification-dependent semantics remain provisional. |
| 7 - Integration & Validation | Partial | End-to-end round-trip and deterministic BER qualification exist; external interoperability and complete compliance closure remain outstanding. |
| 8 - Documentation & Examples | Complete | README, reproduction instructions, gateway documentation, CLI examples, and validation guidance are available. |

---

## Validation Results

### Gateway 1: Spreading Codes

| Check | Pass/Fail | Evidence |
| --- | --- | --- |
| Gold codes match Annex3 (all 210 PRNs) | Pass | Gateway 1 Annex 3 report suite and `Validation/annex3/` reference vectors. |
| Weil primary codes match Annex3 (all 210 PRNs) | Pass | Gateway 1 Annex 3 report suite and `Validation/annex3/` reference vectors. |
| Weil tertiary codes match Annex3 (all 210 PRNs) | Pass | Gateway 1 Annex 3 report suite and `Validation/annex3/` reference vectors. |
| Secondary codes match Table 10 | Pass | `docs/spec_tables/table_10_secondary_codes.csv` and Gateway 1 validation. |
| Tiered codes maintain coherency | Pass | Gateway 1 tiered-generation and Table 11 assignment tests. |
| Code lengths match Table 9 | Pass | Gateway 1 smoke tests validate Gold, Weil primary, and Weil tertiary lengths. |
| Generation time < 1s per PRN | Pass | Gateway 1 performance suite checks the threshold. |

### Gateway 2: Forward Error Correction

| Check | Pass/Fail | Evidence |
| --- | --- | --- |
| BCH encoder produces valid 52-symbol codewords | Pass | Gateway 2 BCH tests and Gateway 5 soft BCH decoder tests. |
| LDPC encoder produces valid codewords | Pass | Gateway 2 real-matrix LDPC encoder tests. |
| LDPC puncturing handled correctly | Pass | Gateway 5 restores punctured positions and validates shortened SB3/SB4 bits. |
| CRC-24 matches specification | Pass | Gateway 2 CRC-24Q tests. |
| Interleaver pattern validated | Pass | Gateway 2 60x98 interleaver tests include round-trip identity. |
| Round-trip encode/decode recovers data | Pass | Gateway 5 decode and Gateway 5-to-6 handoff tests. |
| Encoding time < 100ms per frame | TBD | Populate from the final clean-run report. |
| Decoding time < 1s per frame | Pass | 70.8 ms worst measured three-subframe decode at 3 dB. |
| BER < 10^-5 at SNR > 0 dB | Pass at 3 dB | 0/299,880 post-LDPC bit errors and 102/102 accepted frames. |

### Gateway 3: Navigation Message Framing

| Check | Pass/Fail | Evidence |
| --- | --- | --- |
| Frame structure matches Figure 9 | Pass | `codes/gateway3/` frame assembly tests and Gateway 4 end-to-end pipeline test. |
| Symbol counts: 68 + 52 + 5880 = 6000 | Pass | Frame assembly validation verifies the 6000-symbol frame. |
| Bit allocations match spec tables | Pass | Subframe builder tests validate SB1-SB4 packed allocations. |
| Frame duration is 12 seconds | Pass | 6000 symbols at 500 symbols/s. |

### Gateway 4: Baseband Signal Generation

| Check | Pass/Fail | Evidence |
| --- | --- | --- |
| I/Q samples correctly formatted | Pass | IQ32 binary export is validated as interleaved float32 I/Q; CSV export is also tested. |
| AFS-I chip rate: 1.023 Mchip/s | Pass | Gateway 4 validation asserts 1,023,000 chips/s. |
| AFS-Q chip rate: 5.115 Mchip/s | Pass | Gateway 4 validation asserts 5,115,000 chips/s. |
| Symbol rate: 500 symbols/s (AFS-I) | Pass | Gateway 4 validation asserts 500 symbols/s. |
| Code synchronization correct | Pass | Gateway 4 validates AFS-I/AFS-Q alignment and one 12-second code period. |
| Signal duration: 12 seconds | Pass | Full frame is 12,276,000 AFS-I chips and 61,380,000 AFS-Q chips. |

### Gateway 5: Frame Sync & Decoding

| Check | Pass/Fail | Evidence |
| --- | --- | --- |
| Frame sync detection > 99% at SNR > 0 dB | Pass | 9,960/10,000 detections at 0.1 dB; one-sided 95% lower bound 99.4819%. |
| Decoders recover original data correctly | Pass | 0/299,880 post-LDPC bit errors at 3 dB. |
| CRC validation catches errors | Pass | CRC validation tests reject corrupted and non-binary subframes. |
| LDPC converges in < 50 iterations | Pass | At 3 dB, maximum observed iteration count was 10; cap is 50. |
| Decode time < 1s per frame | Pass | 70.8 ms worst measured three-subframe decode at 3 dB. |

### Gateway 6: Message Parsing

| Check | Pass/Fail | Evidence |
| --- | --- | --- |
| All subframes parse correctly | Pass | Dedicated SB1, SB2, SB3, and SB4 parser tests. |
| WN, ITOW, TOI fields extracted | Pass | SB1/SB2 parser tests and decoded JSON output. |
| Time of transmission calculated accurately | Partial | Relative SB2 ToT is calculated; absolute conversion awaits the unresolved LSIS LRT epoch. |
| All message types handled | Partial | Parser modules exist for all subframes; some message semantics remain provisional where the source specification is incomplete. |

### Gateway 7: Integration & Validation

| Check | Pass/Fail | Evidence |
| --- | --- | --- |
| Round-trip recovers data with 100% accuracy | Pass for qualified local path | `goon encode` to IQ32 to `goon decode` to Gateway 6 parsing is tested with CRC-gated payload release. |
| All 12 interim test codes working | Pass | Table 11 assignment validation is part of the Gateway 1 suite. |
| Process 12s frames in < 1 second | Pass for qualified FEC decode | 70.8 ms worst measured three-subframe decode; full signal generation/acquisition end-to-end timing is TBD. |
| All "shall" requirements verified | Partial | Implemented requirements are validated; full compliance closure is not yet recorded. |

---

## Performance Benchmarks

| Metric | Your Result | Target |
| --- | --- | --- |
| Code generation (per PRN) | Pass threshold check; fresh timing TBD | < 1 second |
| Frame encoding (per frame) | TBD - final clean-run measurement required | < 100 ms |
| Frame decoding (per frame) | 70.8 ms worst measured three-subframe decode at 3 dB | < 1 second |
| Real-time factor | TBD - final end-to-end timing required | > 1x |
| BER at SNR 0 dB | Not recorded; 0/299,880 at 3 dB | < 10^-5 |
| Frame sync reliability | 99.60% observed at 0.1 dB; 99.4819% one-sided 95% lower bound | > 99% |
| Test coverage | No source-coverage measurement recorded | > 90% |

The Gateway 5 qualification uses a fixed seed and 102 real-matrix SB2/SB3/SB4 sets at 3 dB. Gateway 7 uses 102 trials per SNR point with real LDPC matrices and AWGN, recording BER, CRC-accepted frames, iterations, and worst frame time. The recorded Gateway 7 result reaches zero bit errors and 102/102 accepted frames at 1.7 dB across 299,880 decoded bits. Hardware, OS, compiler, and final build flags must be recorded with the final clean run.

---

## Interoperability (optional)

No external partner interoperability result is recorded in the repository. The local encode/decode round trip is reported above as integration testing and is not external interoperability.

---

## Test Summary

| Category | Total | Passing | Failing | Skipped |
| --- | --- | --- | --- | --- |
| Unit tests | TBD | TBD | TBD | TBD |
| Integration tests | TBD | TBD | TBD | TBD |
| Compliance tests | TBD | TBD | TBD | TBD |
| Performance tests | TBD | TBD | TBD | TBD |

Populate these totals from the final clean `ctest` and `test_engine` outputs. The project keeps standalone CTest targets separate from report-engine test cases, so historical aggregate counts should not be reused when the inventory changes.

---

## Known Limitations

- Absolute Time of Transmission is reported as relative seconds until the LSIS LRT epoch is defined.
- Some SB2 clock/ephemeris details, SB3 routing, and SB4 network-access semantics remain provisional where the detailed message contract is incomplete.
- Receiver qualification assumes known PRN, AFS-I, integer chip timing, integer-multiple sample rates, and AWGN.
- External recordings, fading, interference, multi-PRN acquisition, and fractional timing are not qualified.
- No external interoperability result or source-code coverage percentage is recorded.

---

## Innovation & Extras (optional)

- Normalized, confidence-gated acquisition and CRC-gated payload release.
- Real Annex LDPC matrices with soft-decision processing through the receiver path.
- Deterministic rational chip-index mapping for time-aligned I/Q generation.
- Thread-safe Legendre caching for repeated Weil sequence generation.
- C API, zero-dependency Python `ctypes` bridge, Markdown/JUnit reports, and a Tkinter report viewer with an operator-oriented Mission Console.

---

## File Manifest

| Path | Description |
| --- | --- |
| `README.md` | Project overview, setup, and quick start. |
| `SUBMISSION.md` | Competition submission report using the supplied template. |
| `codes/gateway1/` | Spreading-code generation and configuration. |
| `codes/gateway2/` | BCH, CRC-24Q, LDPC encoding, and interleaving. |
| `codes/gateway3/` | Frame builders, assembly, and export. |
| `codes/gateway4/` | BPSK, spreading, I/Q generation, and signal export. |
| `codes/gateway5/` | Receiver acquisition, synchronization, soft FEC decoding, and CRC gate. |
| `codes/gateway6/` | SB1-SB4 parsing and Gateway 5 handoff test. |
| `codes/gateway7/` | Deterministic BER/SNR validation. |
| `codes/testing/` | Test engine, validators, report writers, and Annex 3 loader. |
| `config/` | Runtime configuration. |
| `docs/` | Gateway documentation, specification tables, and submission material. |
| `Validation/` | Annex 3 reference vectors, generated artifacts, and validation reports. |

---

## Self-Assessment

| Category (Points) | Self-Score | Justification |
| --- | --- | --- |
| Correctness (40) | TBD/40 | Complete after final clean-run evidence review. |
| Performance (20) | TBD/20 | Complete after final timing measurements are recorded. |
| Completeness (20) | TBD/20 | Gateway 6 and Gateway 7 limitations require team review. |
| Code Quality (10) | TBD/10 | Complete after submission review. |
| Innovation & Extras (10) | TBD/10 | Complete after submission review. |
| **Total** | **TBD/100** | Final team assessment required. |

---

## Additional Notes (optional)

Fresh validation artifacts under `Validation/reports/` should be included or referenced with the final submission so the reported test counts and timing figures are traceable to the final build environment.
