# Submission Data Pack

This document is the source of truth for completing the LSIS-AFS competition submission report. It records repository facts, implementation status, reproducible commands, validation evidence, and fields that still require team confirmation. It is intentionally kept separate from the PDF-derived material under `docs/info/`.

## Document Control

- Repository: `KURE-x-Tech/Spreading-Codes`
- Branch reviewed: `main`
- Review date: 2026-08-23
- Project version: `1.2.7` from `CMakeLists.txt`
- Submission filename: `SUBMISSION.md` at repository root
- Current implementation scope: Gateways 1-6, with Gateway 7 qualification and networking work still incomplete

## Team Information

| Field | Repository evidence or required input |
| --- | --- |
| Team name | KURE-x-Tech |
| Members | Confirm the complete member list. A previous draft recorded seven members but did not name them. |
| Institution / affiliation | Kingston University |
| Contact email | Confirm the primary submission contact. |
| Repository URL | `https://github.com/KURE-x-Tech/Spreading-Codes` |

## Implementation Overview

| Component | Current choice |
| --- | --- |
| Primary language | C++17 |
| Supporting language | Python 3 for the ctypes bridge, utilities, and report viewer |
| Build system | CMake 3.16 or newer |
| Third-party runtime libraries | None for the core C++ or Python workflows; C++ uses the standard library and Python uses the standard library modules `ctypes` and `tkinter` |
| Test framework | C++ standalone tests registered with CTest, plus the gateway-scoped `test_engine` report harness and a Python bridge smoke test |
| Tested platforms | The repository documents Windows, macOS, and Linux toolchains. Record the exact OS, compiler, generator, and flags used for the final run. |

## Architecture Summary

The implementation is organized as independent gateway libraries under `codes/`. Gateway 1 generates Gold, Weil primary, Weil tertiary, secondary, and tiered spreading sequences from the checked-in configuration and Annex 3 data. Gateway 2 provides BCH, CRC-24Q, LDPC encoding, and the 60x98 block interleaver. Gateway 3 builds SB1-SB4 and assembles the 6000-symbol navigation frame. Gateway 4 spreads and modulates the frame and exports I/Q data.

Gateway 5 is the receive-side inverse path. It imports raw IQ32 or standardized LSISIQ files, acquires the known PRN, de-spreads AFS-I symbols, detects the normalized sync pattern, extracts the frame, performs soft BCH and LDPC decoding, validates all three CRCs, and releases only fully accepted frames. Gateway 6 consumes the CRC-stripped payloads and parses SB1-SB4 into structured results, including relative SB2 time-of-transmission seconds. The `goon` CLI is the operational entry point; the C API and Python `ctypes` wrapper provide scripting and FFI access.

The design keeps the core signal operations deterministic and testable. Runtime reference data is loaded from `config/`, `docs/spec_tables/`, and `Validation/annex3/`. The receiver preserves soft values until FEC decisions, uses normalized correlation for gain-invariant acquisition, and gates the Gateway 5-to-6 handoff on CRC acceptance.

## Gateway Status

| Gateway | Status | Evidence and qualification note |
| --- | --- | --- |
| 0 - Design and architecture | Complete | Modular gateway layout, CMake targets, public interfaces, runtime data paths, and test strategy are documented in [ARCHITECTURE.md](ARCHITECTURE.md). |
| 1 - Spreading code generation | Complete | Gold, Weil primary, Weil tertiary, tiered codes, and Table 11 assignments are implemented. The Annex 3 suite covers all 210 PRNs for the relevant code families. |
| 2 - Forward error correction | Complete for implemented scope | BCH, CRC-24Q, LDPC encoding, soft BCH/LDPC decoding, and interleaving/deinterleaving are implemented and tested. Qualification is reported through Gateway 5 tests and the BER benchmark. |
| 3 - Navigation message framing | Complete | Sync pattern, SB1-SB4 builders, CRC/LDPC processing, interleaving, 6000-symbol assembly, and binary/CSV/hex export are implemented. |
| 4 - Baseband signal generation | Complete for the implemented signal path | BPSK, AFS-I data spreading, AFS-Q generation, rational chip-index mapping, and IQ32/CSV export are implemented. The documented workshop profile uses AFS-I at 1.023 MHz. |
| 5 - Frame synchronization and decoding | Complete for the qualified operating envelope | `goon decode` and the integrated receiver are implemented. Qualification uses known PRN, AFS-I, integer chip timing, integer-multiple sample rate, AWGN, and the measured SNR points in [VALIDATION.md](VALIDATION.md). |
| 6 - Message parsing | Partial | All four parser modules, relative ToT computation, JSON emission, and the Gateway 5 handoff are implemented. Absolute timestamp conversion remains pending because the LSIS LRT epoch is unresolved; SB2/SB3/SB4 message semantics remain provisional where the source specification is incomplete. |
| 7 - Integration and validation | Partial | End-to-end encode-to-IQ-to-decode-to-parse tests and deterministic receiver qualification exist. Multi-node networking, external interoperability, broader BER curves, and full compliance closure remain outstanding. |
| 8 - Documentation and examples | Complete for the current implementation scope | Root README, reproduction runbook, gateway documentation, source-linked evidence, and this submission data pack are present. |

## Build and Run Instructions

### Prerequisites

- CMake 3.16 or newer
- A C++17 compiler: MSVC 2019+, GCC 9+, or Clang 10+
- Git
- Python 3 only for the bridge smoke test, I/Q utility, or report viewer

No third-party package installation is required for the core C++ pipeline.

### Configure and Build

Single-configuration generators (Ninja, Make, Unix Makefiles):

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
```

Visual Studio build:

```powershell
ctest --test-dir build-submission-vs -C Release --output-on-failure
.\build-submission-vs\bin\Release\test_engine.exe config\spreading_codes_config.ini
.\build-submission-vs\bin\Release\gateway5_ber_benchmark.exe
```

The clean run should be used to populate the final test totals. Do not copy counts from an older report if the test inventory has changed.

### Key Examples

Generate all code families:

```bash
./build-submission/bin/goon generate-codes --codes all --output Validation/generated/submission_codes
```

Generate a complete frame:

```bash
./build-submission/bin/goon encode \
  --format frame --prn 7 --fid 2 --toi 73 --wn 1234 --itow 256 \
  --output Validation/generated/submission_frame.bin
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

For Visual Studio, use `build-submission-vs/bin/Release/goon.exe` in the equivalent commands.

## Validation Evidence

| Area | Current evidence |
| --- | --- |
| Annex 3 spreading codes | `Validation/annex3/txt/` reference vectors and Gateway 1 validation tests. The 210-PRN Gold, Weil primary, and Weil tertiary comparisons are part of the gateway-scoped report harness. |
| Table 10 and Table 11 | `docs/spec_tables/table_10_secondary_codes.csv`, `docs/spec_tables/table_11_code_assignments.csv`, and Gateway 1 assignment tests. |
| Frame structure | `docs/spec_tables/table_12_sync_pattern.csv`, `docs/spec_tables/table_14_frame_structure.csv`, `codes/gateway3/`, and the Gateway 3 standalone test. |
| Receiver qualification | [GATEWAY5_DECODER.md](../G5/GATEWAY5_DECODER.md) records 9960/10000 sync detections at 0.1 dB, a 99.4819% one-sided 95% lower bound, 16/10000 false alarms, 0/1000 de-spread symbol errors, 0/299,880 post-LDPC bit errors at 3 dB, 102/102 CRC-accepted frames, at most 10 LDPC iterations, and 70.8 ms worst measured three-subframe decode latency. |
| Gateway 6 handoff | `codes/gateway5_gateway6_handoff_test` and the parser tests verify accepted SB1-SB4 output reaches the four parser modules without mutation, including the computed relative SB2 time of transmission. |
| Generated reports | Fresh `test_engine` runs write Markdown and JUnit XML under `Validation/reports/YYYY-MM-DD/`. Include the final run paths in `SUBMISSION.md`. |

## Performance Table

Use the values below only where they are backed by a fresh final run. Replace `TBD` with measured values for the submission hardware.

| Metric | Current evidence | Target |
| --- | --- | --- |
| Code generation per PRN | README reports under 0.5 ms for the full generation pipeline; confirm with a timed final run | < 1 s |
| Frame encoding per frame | Component and frame assembly checks target under 100 ms; record the clean-run measurement | < 100 ms |
| Frame decoding per frame | Worst measured three-subframe BER trial: 70.8 ms | < 1 s |
| Real-time factor | Compute `12 seconds / end-to-end signal processing seconds`; final measurement required | > 1x |
| BER at 0 dB | The recorded qualification point is 3 dB with 0/299,880 post-LDPC bit errors; a 0 dB value is not currently evidenced | < 10^-5 |
| Frame sync reliability | 99.60% observed at 0.1 dB; one-sided 95% lower bound 99.4819% | > 99% |
| Test coverage | No coverage tool result is checked in; report executable test totals separately and do not label them as source coverage | > 90% |

## Test Summary Guidance

The repository has multiple test layers, so the final report should distinguish them:

- `test_engine` emits gateway-scoped functional, compliance, and performance cases in Markdown and JUnit XML.
- Standalone CTest targets cover Gateway 3 and Gateway 4 exporters, Gateway 5 receiver stages and qualification, Gateway 6 parsers, and the Gateway 5-to-6 handoff.
- `codes/python/test_bridge.py` is a Python smoke test for the C API bridge.

Populate the submission table from the fresh CTest and `test_engine` output:

| Category | Total | Passing | Failing | Skipped |
| --- | ---: | ---: | ---: | ---: |
| Unit tests | TBD | TBD | TBD | TBD |
| Integration tests | TBD | TBD | TBD | TBD |
| Compliance tests | TBD | TBD | TBD | TBD |
| Performance tests | TBD | TBD | TBD | TBD |

## Interoperability

No external partner interoperability result is recorded in the repository. The local encode/decode round trip is not external interoperability and should be reported separately. Add partner name, signal format, command, result, and artifact path only after a cross-implementation test has been completed.

## Known Limitations

- Absolute Time of Transmission can only be reported as relative seconds until the LSIS LRT epoch marked `{LSIS-TBD-2003}` is defined.
- SB2 clock/ephemeris details, SB3 message coverage, and SB4 network-access semantics are provisional where the detailed LSIS/LNSP message contract is incomplete.
- The receiver qualification assumes known PRN, AFS-I, integer chip timing, an integer-multiple sample rate, and AWGN. External recordings, fading, interference, multi-PRN acquisition, and fractional timing are not qualified.
- No multi-node mesh networking, routing, acknowledgments, retry policy, or packet-level SISICD is implemented.
- No external interoperability result or checked-in BER-versus-SNR curve is available.
- A source-code coverage percentage has not been measured with a coverage tool.

## Innovation and Extras

- Normalized, confidence-gated receiver acquisition and CRC-gated release of payloads.
- Real Annex LDPC matrices and soft-decision processing through the receive path.
- Deterministic rational chip-index mapping for time-aligned I/Q generation.
- Thread-safe Legendre caching for repeated Weil sequence generation.
- C API plus zero-dependency Python `ctypes` bridge.
- Markdown/JUnit report generation and a Tkinter report viewer with an operator-oriented Mission Console.

## File Manifest

| Path | Purpose |
| --- | --- |
| `CMakeLists.txt` | C++17 build graph, libraries, executables, and CTest registration |
| `codes/gateway1/` | Spreading-code generation and configuration |
| `codes/gateway2/` | BCH, CRC-24Q, LDPC encoding, and interleaving |
| `codes/gateway3/` | Frame builders, assembly, and export |
| `codes/gateway4/` | BPSK, spreading, I/Q generation, and signal export |
| `codes/gateway5/` | Receiver acquisition, synchronization, soft FEC decoding, and CRC gate |
| `codes/gateway6/` | SB1-SB4 parsing and Gateway 5 handoff test |
| `codes/testing/` | Test engine, validators, report writers, and Annex 3 loader |
| `codes/python/` | Python bridge, I/Q utility, and smoke test |
| `config/spreading_codes_config.ini` | Runtime paths and specification configuration |
| `docs/res/` | Reproduction, architecture, validation, and submission evidence documents |
| `docs/spec_tables/` | Machine-readable specification constants and layouts |
| `Validation/annex3/` | Annex 3 reference vectors and LDPC source data |
| `Validation/reports/` | Generated Markdown and JUnit XML validation reports |
| `README.md` | Public overview and quick start |
| `SUBMISSION.md` | Final competition report to be produced from this data pack |

## Self-Assessment Starting Point

This is an evidence-based starting point, not a final score. The scoring table must be completed after the final clean run and team review.

| Category | Suggested score | Rationale |
| --- | ---: | --- |
| Correctness (40) | TBD | Strong verified coverage for Gateways 1-5 and parser-level Gateway 6; unresolved specification-dependent fields remain. |
| Performance (20) | TBD | Receiver latency and sync/BER qualification are measured; real-time factor and several target-specific values still need final measurement. |
| Completeness (20) | TBD | Gateways 1-6 are substantially implemented, including Gateway 6 relative ToT; Gateway 7 interoperability/networking and absolute timestamp conversion remain incomplete. |
| Code quality (10) | TBD | Modular C++17 libraries, CTest targets, report generation, C API, and Python bridge. |
| Innovation and extras (10) | TBD | Receiver confidence gating, real-matrix soft decoding, rational I/Q mapping, caching, and tooling. |
| **Total (100)** | **TBD** | Complete after evidence and team metadata are finalized. |
