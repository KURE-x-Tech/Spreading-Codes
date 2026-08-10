# Changelog

## Unreleased

### Added

- **Self-Contained CLI Installation**: CMake installs `goon`, its relocatable shared library, runtime configuration, specification tables, Annex references, and LDPC matrices. Installed commands can run outside the repository working directory.
- **CLI Version Regression Test**: `goon_version_test` verifies that CLI output matches the CMake project version.
- **LSIS-AFS CLI Tool**: New `goon` executable (`codes/lsis_cli.cpp`) conforming to the workshop interop CLI contract. Subcommands: `generate-codes` (210 Gold PRNs -> hex), `encode --format frame` (data -> 6000-byte frame), `encode --format iq32` (full encode -> interleaved float32 I/Q signal), `decode` (I/Q -> CRC-gated payload JSON), and `version`.
- **Gateway 5 Integrated Receiver**: `DecodeAfsIIqSignal`, `DecodeAfsIChipStream`, and `DecodeDespreadSymbols` orchestrate I/Q normalization, Gold-code de-spreading, normalized sync acquisition, frame extraction, BCH/LDPC decode, CRC gating, and CRC stripping.
- **Gateway 5 Decode CLI**: `goon decode` accepts headerless IQ32 or standardized LSISIQ input and emits FID/TOI, acquisition telemetry, LDPC iterations, CRC verdicts, and 1176/846/846-bit Gateway 6 payloads.
- **Gateway 5 Qualification**: Deterministic sync, false-alarm, de-spread SER, full-frame, malformed-input, BER, latency, and Gateway 5-to-6 navigation handoff tests. The reproducible 3 dB campaign observed 0 post-LDPC errors in 299,880 bits with 102/102 CRC-accepted frames.
- **Gateway 5 Guide**: `docs/G5/GATEWAY5_DECODER.md` documents commands, API usage, stage behavior, output contracts, qualification, and operating boundaries.
- **Frame Raw Export**: `ExportFrameRaw()` in `frame_exporter` writes one byte per symbol (0x00/0x01) matching the workshop CI `frame.bin` format (6000 bytes).

### Changed

- **Single Version Source**: Removed the hard-coded `1.1.0` library string. `get_version()` now uses a generated header derived from `project(... VERSION ...)`, so rebuilding updates `goon version`.
- **I/Q Sample Rate Relaxed**: `GenerateIq` now accepts any sample rate that is a positive integer multiple of the AFS-I chip rate (1,023,000 Hz) instead of requiring a multiple of the AFS-Q rate (5,115,000 Hz). Enables the workshop's `--rate 1023000` contract. Rational chip-index mapping used for both channels.
- **Default Sample Rate**: Changed from 5.115 MHz (AFS-Q) to 1.023 MHz (AFS-I) to match the workshop interop convention.
- **Documentation Status Alignment**: Updated top-level and technical docs to reflect the integrated Gateway 5 receiver and its measured qualification envelope.
- **LDPC Shortened Bits**: Restored SB3/SB4 filler positions as strong known-zero priors instead of erasures, reducing noiseless convergence from three iterations to two in the reference case.
- **Knowledge-Bank Referencing**: Added explicit reference to `KURE-x-Tech/Asteria-Knowledge-Base-G5-share` as external design/scaffold guidance (not auto-integrated code).

- **Table 11 Validation (SC-1.6)**: 60-test suite verifying all 12 LNSP node assignments — secondary code cycling (S0→S3), AFS-I/Q PRN identity, and tiered AFS-Q structural correctness (primary ⊕ secondary ⊕ tertiary).
- **BCH(51,8) Encoder/Decoder (FEC-2.1/2.2)**: 8-stage LFSR encoder with generator polynomial 763 (octal), MSB XOR + prepend per LSIS §2.4.2.1. Soft-decision decoder via exhaustive 256-codeword correlation.
- **CRC-24Q (FEC-2.5/2.6)**: Compute, append, and verify functions using polynomial 0x864CFB (CRC-24Q / GPS CNAV).
- **Block Interleaver (FEC-2.7)**: 60×98 write-row/read-column interleaver and deinterleaver for SB2+SB3+SB4 (5880 symbols).
- **LDPC Encoder (FEC-2.3)**: Rate-1/2 encoder using dense GF(2) submatrices (A, B⁻¹, C, D) loaded from Annex 1 CSV files. Supports SB2 (1200→2400) and SB3/SB4 (870→1740) with proper filler and puncturing per LSIS §2.4.3.1.2.
- **Gateway 2 Library**: New `lunanet_gateway2` static library in `codes/gateway2/` linked to test engine.
- **C API Layer**: `codes/c_api.h/.cpp` — extern "C" shim over the C++ API, exported from the DLL with `LUNANET_API` macros for cross-platform ctypes/FFI access.
- **Python Bridge**: `codes/python/lunanet.py` — zero-dependency ctypes wrapper with auto-DLL discovery, type-safe prototypes, and all code generation + FEC functions. Smoke-tested.
- **I/Q Signal Generator**: `codes/python/iq_generator.py` — BPSK(1) baseband signal generator. Maps chips to ±1.0 float32. Outputs separate I/Q binary, interleaved I/Q binary, and CSV formats.
- **Report Viewer GUI**: Tkinter dark-theme viewer (`codes/gateway1/gui/report_viewer.py`) with color-coded pass/fail rows, suite/status filtering, summary table, and auto-discovery of timestamped reports.
- **Timestamped Reports**: Test engine now writes reports to `Validation/reports/YYYY-MM-DD/HH-MM-SS.{md,xml}` for historical tracking.
- **Test Framework**: Modular test engine split into `codes/testing/` with `TestReporter`, `TestValidators`, and `Annex3Loader` modules. Test engine reduced from monolithic 270-line main to a thin ~160-line orchestration harness.
- **Dual Report Output**: Test engine now writes both a markdown summary (`test_results.md`) and JUnit XML (`test_results.xml`) to the reports directory for CI integration.
- **Performance Benchmarks**: New `Performance` test suite validates SC-1.7 requirement (< 1 second per PRN). Measured: ~0.3-0.4 ms per PRN for full generation pipeline.
- **Legendre Caching**: Thread-safe module-level `LegendreCache` in `weil_code_generator.cpp` eliminates ~420 redundant Legendre sequence computations per 210-PRN batch. Public `ClearLegendreCache()` API for explicit memory management.

### Fixed

- **Gold Code Validation**: Corrected LFSR G2 initialization by computationally advancing the sequence `2047 - D_k` steps to correctly realize the telecommunications mathematical delay $G_2(t - D_k)$. LFSR now perfectly matches Annex3 test vectors for all 210 PRNs.
- **Generator Alignment**: Resolved mathematical dissonance by restoring the left-shifting Fibonacci implementation that correctly aligns with the characteristic polynomial specification.
