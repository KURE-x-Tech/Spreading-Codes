# Implementation Architecture

## Scope

This document describes the implementation that should be summarized in the competition submission. The PDF-derived architecture and requirements documents under `docs/info/` remain reference material; this document reflects the current repository.

## Processing Flow

```text
configuration + Annex reference data
                |
                v
Gateway 1: spreading-code generation
                |
                v
Gateway 2: BCH / CRC-24Q / LDPC / interleaving
                |
                v
Gateway 3: SB1-SB4 builders and 6000-symbol frame
                |
                v
Gateway 4: AFS-I / AFS-Q spreading, BPSK, I/Q export
                |
                v
Gateway 5: import, acquisition, sync, soft FEC, CRC gate
                |
                v
Gateway 6: SB1-SB4 parsing and structured JSON output
```

## Modules

### Gateway 1: Spreading Codes

`codes/gateway1/` contains the Gold LFSR generator, Legendre-backed Weil primary and tertiary generators, tiered AFS-Q construction, and configuration/table loading. The public convenience API is in `codes/spreading_codes.*`; the C ABI is in `codes/c_api.*`.

Reference data is not hard-coded into the algorithm. Paths and table selections come from `config/spreading_codes_config.ini`, while Annex 3 vectors and LDPC source data remain under `Validation/annex3/`.

### Gateway 2: Forward Error Correction

`codes/gateway2/` implements BCH encoding, CRC-24Q, dense GF(2) LDPC encoding, and the 60x98 block interleaver. Gateway 5 contains the receive-side soft BCH decoder, LDPC decoder, CRC validator, and inverse interleaver because those operations depend on received soft values and receiver state.

The frame payload geometry is:

| Region | Data | Protected/encoded output |
| --- | ---: | ---: |
| SB1 | 9 bits | 52 symbols through BCH |
| SB2 | 1176 bits | 1200 bits after CRC, 2400 LDPC symbols |
| SB3 | 846 bits | 870 bits after CRC, 1740 LDPC symbols |
| SB4 | 846 bits | 870 bits after CRC, 1740 LDPC symbols |

### Gateway 3: Framing

`codes/gateway3/` builds the fixed synchronization pattern, SB1 timing header, SB2 clock/ephemeris payload, SB3 variable data, and SB4 network-access payload. The frame assembler leaves the 68-symbol sync and 52-symbol SB1 regions in place and interleaves the remaining 5880 symbols. Exporters provide binary, CSV, and hexadecimal representations.

The resulting frame contains 6000 symbols. At 500 symbols per second, its transmission duration is 12 seconds.

### Gateway 4: Signal Generation

`codes/gateway4/` maps logic 0 to +1.0 and logic 1 to -1.0 for BPSK. AFS-I uses Gold-code epochs at 1.023 Mchip/s; AFS-Q uses the tiered path at 5.115 Mchip/s. I/Q output is interleaved little-endian float32 pairs.

The sample generator uses a shared integer sample index and rational chip-index mapping. This avoids floating-point phase accumulation and keeps the channels aligned for supported sample rates, which are positive integer multiples of 1.023 MHz.

### Gateway 5: Receiver

`codes/gateway5/` is the integrated receive path:

1. Validate and import raw IQ32 or standardized LSISIQ data.
2. Select and validate the AFS-I channel.
3. Acquire the known PRN by normalized Gold-code correlation.
4. Integrate and dump chip epochs to produce soft navigation symbols.
5. Locate the 68-symbol sync pattern with normalized correlation and confidence gates.
6. Extract SP, SB1, and the interleaved SB2-SB4 region.
7. Decode SB1 with soft BCH correlation.
8. Soft-deinterleave and decode SB2-SB4 with normalized min-sum LDPC.
9. Require all three CRC-24Q checks to pass.
10. Strip CRC fields and hand accepted payloads to Gateway 6.

The receiver rejects malformed input, ambiguous acquisition, invalid FEC results, and partially valid frames rather than returning untrusted navigation data.

### Gateway 6: Message Parsing

`codes/gateway6/` parses the accepted CRC-stripped payloads. It extracts SB1 FID/TOI, SB2 WN/ITOW/TOI plus relative time-of-transmission seconds, currently supported health, and provisional raw CED and Time Conversions blocks. It routes SB3 dynamic messages including the provisional orbit-almanac profile, and validates/labels SB4 dynamic network-access payloads while preserving the raw network-access bits for an LNSP SISICD decoder.

Absolute ToT is intentionally not fabricated: the source specification leaves the LRT start epoch unresolved. The parser therefore exposes `time_of_transmission_seconds` as a relative LSIS epoch offset without claiming an absolute UTC/GPS-style timestamp.

## Interfaces

- `goon`: C++ CLI for code generation, frame encoding, I/Q generation, and IQ32 decoding.
- C++ libraries: `lunanet_gateway2` through `lunanet_gateway6` and the public `lunanet_spreading_codes` library.
- C API: stable `extern "C"` exports for FFI consumers.
- Python: zero-dependency `ctypes` wrapper, BPSK/I/Q utility, and report viewer.
- Validation outputs: Markdown and JUnit XML reports under `Validation/reports/`.

## Design Decisions

- Keep gateway responsibilities separate so each stage can be tested independently.
- Preserve floating-point soft decisions until BCH/LDPC decoding.
- Use specification data files for reference vectors, matrices, constants, and field layouts.
- Gate cross-gateway handoff on complete CRC validation.
- Use deterministic integer indexing where signal timing must remain aligned.
- Keep core dependencies to the C++17 and Python standard libraries.

## Current Boundaries

The current implementation is a complete, qualified single-PRN AFS-I receiver path within its documented operating envelope. It is not a complete multi-node LunaNet network implementation. Routing, packet semantics, external interoperability, broader channel models, and specification-dependent message meanings require additional contracts or reference information.
