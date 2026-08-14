# Project Memory

## Current State

- **LSIS-AFS CLI Tool**: `goon` executable (`codes/lsis_cli.cpp`) conforming to the workshop interop CLI contract. Subcommands: `generate-codes` (210 Gold PRNs -> 512-char hex), `encode --format frame` (-> 6000-byte frame.bin), `encode --format iq32` (-> interleaved float32 I/Q signal), and `decode` (I/Q -> CRC-gated JSON plus parsed Gateway 6 subframes).
- **CLI Packaging**: `project(... VERSION ...)` is the single source for `goon version`. `cmake --install <build> --prefix <prefix>` creates a relocatable, self-contained CLI package with runtime config and Annex/LDPC data; building alone does not replace an older command on `PATH`.
- **Validation**: All PRN codes (Gold, Weil Primary, Weil Tertiary) are fully passing 210/210 compliance with Annex3 validation vectors. Table 11 interim code assignments (12 LNSP nodes) validated with 60/60 checks covering secondary code cycling, PRN identity, and tiered AFS-Q construction.
- **Test Framework**: Modular test engine (`codes/testing/`) with structured reporting - outputs console summary, markdown report, and JUnit XML for CI integration. Report totals are build-dependent and should be taken from generated artifacts under `Validation/reports/YYYY-MM-DD/HH-MM-SS.{md,xml}`.
- **Gateway 2 (FEC Encoding)**: Complete. BCH(51,8) encoder/decoder, CRC-24Q, LDPC rate-1/2 encoder (SB2 + SB3/SB4), and 60×98 block interleaver all implemented and tested.
- **Gateway 3 (Frame Assembly)**: Complete. Sync pattern + SB1(BCH) + SB2/SB3/SB4(CRC+LDPC) + 60×98 interleave → 6000 symbols. Raw export (1 byte/symbol) for workshop CI.
- **Gateway 4 (Signal Generation)**: Complete. BPSK modulation, AFS-I data spreading, I/Q baseband generation with rational sample-rate mapping. Default rate 1.023 MHz (workshop contract). Supports any integer multiple of the AFS-I chip rate.
- **Gateway 5 (Integrated Receiver)**: Headerless/standard I/Q import, gain-normalized Gold-code de-spreading, statistically qualified noisy frame sync, confidence-gated BCH, soft deinterleaving, saturated/shortened-bit-safe LDPC, CRC-gated acceptance, CRC stripping, `goon decode`, empirical BER qualification, and a direct four-subframe Gateway 6 handoff are implemented. The documented software profile is qualified at 3 dB full-frame SNR; broader SNR curves and external recordings remain Gateway 7 work. Configure `-DCMAKE_BUILD_TYPE=Release` before running the <1 s decoder qualification on single-config generators.
- **Gateway 6 (Message Parsing)**: SB1 extracts FID/TOI, SB2 extracts WN/ITOW/TOI plus provisional CED/health, SB3 routes dynamic message types with a provisional orbit-almanac profile, and SB4 validates its dynamic type while preserving the LNSP-specific payload. `goon decode` exposes all four under its `subframes` JSON object. ToT remains pending because the LRT epoch is `{LSIS-TBD-2003}`.
- **Python Bridge**: Zero-dependency ctypes wrapper (`codes/python/lunanet.py`) over C-linkage DLL API (`codes/c_api.h`). Exposes all code generators, BCH encoding, and CRC-24 to Python. Smoke-tested.
- **I/Q Generation (Python)**: BPSK(1) baseband signal generator (`codes/python/iq_generator.py`) outputs binary float32 and CSV I/Q files. Mapping: 0→+1.0, 1→-1.0.
- **Report Viewer**: Tkinter GUI (`codes/gateway1/gui/report_viewer.py`) with dark theme, color-coded pass/fail table rows, suite/status filtering, auto-discovery of timestamped reports, and a Mission Console for the real G1-G6 Earth-to-orbit encode/decode workflow.
- **Performance**: Legendre sequence caching eliminates redundant computation in Weil generators. Full PRN generation (Gold + Weil Primary + Weil Tertiary + AFS-Q) completes in < 0.5 ms per PRN - well under the SC-1.7 requirement of < 1 second.
- **Gold Code Generator**: Fixed the `lunanet::gateway1` Gold code generators to match the Annex3 reference specification. The $G_2$ delay is applied by computationally advancing the sequence by $2047 - D_k$ relative to $G_1$, using a standard left-shifting Fibonacci LFSR algorithm.

## Architecture

- `codes/testing/test_reporter.h/.cpp` - TestReporter with pass/fail/skip tracking, markdown + JUnit XML output.
- `codes/testing/test_validators.h/.cpp` - Reusable validation primitives (Annex3 suite, length, bounds, equality, Table11).
- `codes/testing/test_annex3_loader.h/.cpp` - Annex3 hex reference file parser.
- `codes/test_engine.cpp` - Orchestration harness with 9 test suites.
- `codes/gateway1/weil_code_generator.cpp` - Thread-safe `LegendreCache` (module-level static) for Weil generators.
- `codes/gateway1/gui/report_viewer.py` - Tkinter report viewer GUI (dark theme, summary + detail tables).
- `codes/gateway1/gui/mission_console.py` - Operator-facing G1-G6 Mission Console using the Python bridge and local `goon` CLI.
- `codes/gateway1/gui/report_parser.py` - JUnit XML / markdown parser (no GUI dependency).
- `codes/gateway2/bch_codec.h/.cpp` - BCH(51,8) encoder with LFSR + soft-decision decoder.
- `codes/gateway2/crc24.h/.cpp` - CRC-24Q compute/append/verify (polynomial 0x864CFB).
- `codes/gateway2/interleaver.h/.cpp` - 60×98 block interleaver/deinterleaver.
- `codes/gateway2/ldpc_encoder.h/.cpp` - LDPC rate-1/2 encoder with dense GF(2) matrix ops and CSV loader.
- `codes/gateway5/frame_synchronizer.h/.cpp` - Stage-1 sync reference symbol construction from the fixed SP pattern.
- `codes/gateway5/frame_decoder.h/.cpp` - Integrated I/Q-to-payload receiver orchestration and telemetry.
- `codes/gateway5/despreader.h/.cpp` - Gain-normalized Gold-code acquisition and integrate-and-dump.
- `codes/gateway5/sync_detector.h/.cpp` - Normalized matched-correlation frame acquisition.
- `codes/gateway5/bch_soft_decoder.h/.cpp` - Confidence-gated SB1 soft decoder.
- `codes/gateway5/ldpc_decoder.h/.cpp` - Saturated normalized min-sum SB2-SB4 decoder.
- `codes/gateway5/crc_validator.h/.cpp` - CRC-24Q frame acceptance gate.
- `codes/gateway5/symbol_extractor.h/.cpp` - Frame region slicing (SP/SB1/interleaved) + LLR conversion helper.
- `codes/gateway6/subframe1_parser.h/.cpp` - Subframe 1 parser for FID (0..3) and TOI (0..99) per Table 13 & Table 14.
- `codes/gateway6/subframe2_parser.h/.cpp` - Subframe 2 parser for WN, ITOW, TOI, and CED/Health.
- `codes/gateway6/subframe3_parser.h/.cpp` - Subframe 3 variable-data router and almanac decoder.
- `codes/gateway6/subframe4_parser.h/.cpp` - Subframe 4 dynamic network-access type and payload parser.
- `codes/c_api.h/.cpp` - C-linkage DLL shim for ctypes/FFI access.
- `codes/python/lunanet.py` - Zero-dependency Python wrapper over the C API.
- `codes/python/iq_generator.py` - BPSK(1) I/Q signal generator (float32 binary + CSV export).
- `codes/lsis_cli.cpp` - Workshop CLI tool: generate-codes, encode, decode, version.

## Active Tasks (Completed)

- Exhaustive validation of the Gold code LFSR configuration.
- Resolution of Gold Code reference validation failures (210/210 passing now).
- Complete synchronization of C++ output with Annex3 documentation for `006_GoldCode2046hex210prns.txt`.
- Modularized test engine into `codes/testing/` with split validation, reporting, and harness layers.
- Added Legendre sequence caching in Weil generators - eliminates ~420 redundant computations per batch.
- Performance benchmark suite validating SC-1.7 (< 1 second per PRN).
- Tkinter report viewer GUI with dark theme, color-coded results, suite/status filtering.
- Timestamped report output to `Validation/reports/YYYY-MM-DD/HH-MM-SS.*`.

- Table 11 interim code assignment validation (SC-1.6) — 60/60 tests covering all 12 LNSP nodes.
- BCH(51,8) encoder/decoder (FEC-2.1/2.2) — 10/10 tests, round-trip verified.
- CRC-24Q compute/verify (FEC-2.5/2.6) — 4/4 tests.
- Block interleaver 60×98 (FEC-2.7) — 4/4 tests.
- LDPC rate-1/2 encoder (FEC-2.3) — SB2 (1200→2400) and SB3/SB4 (870→1740), 12/12 tests.
- Python ctypes bridge — C API DLL shim + Python wrapper, smoke-tested.
- BPSK(1) I/Q generator — binary float32 and CSV export, verified ±1.0 mapping.

## Next Steps

- Gateway 6 completion: Time of Transmission calculation once the LRT epoch is defined upstream.
- Gateway 7: BER-vs-SNR curves below/above the qualified 3 dB profile and external cross-team recordings.
- Full round-trip encode -> decode -> parse interoperability at future workshops.
- Docker containerisation for the CLI tool (alternative to bare executable for GitLab CI).
