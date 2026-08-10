# LunaNet AFS Spreading Codes & Signal Encoder

Implementation of the LunaNet Augmented Forward Signal (AFS) spreading code generators and forward error correction (FEC) encoders, built to the **LSIS-AFS Volume A** specification.

Current status: Gateways 1-5 are implemented in the main pipeline. Gateway 5 provides `goon decode`, CRC-gated payload output, reproducible sync/SER/BER qualification, and a tested Gateway 6 handoff. Gateway 6 parsing is in progress.

---

## Features

### Gateway 1 - Spreading Code Generation

- **Gold Code Generator** - 11-stage Fibonacci LFSR pair (G1/G2), 2046-chip sequences for 210 PRNs. Validated against Annex 3 reference vectors.
- **Weil Primary Code Generator** - Legendre-sequence-based Weil construction over GF(10223) with 7-bit insertion expansion to 10230 chips.
- **Weil Tertiary Code Generator** - Weil construction over GF(1499) producing 1500-chip tertiary codes.
- **Tiered AFS-Q Constructor** - Three-tier modular code: primary ⊕ secondary ⊕ tertiary per LSIS §2.3.
- **Table 11 Node Assignments** - 12 LNSP interim test node configurations with secondary code cycling (S0→S3).

### Gateway 2 - Forward Error Correction

- **BCH(51,8) Encoder/Decoder** - 8-stage LFSR with generator polynomial 763₈. Includes soft-decision decoder via exhaustive 256-codeword correlation.
- **CRC-24Q** - Polynomial `0x864CFB` (GPS CNAV compatible). Compute, append, and verify.
- **LDPC Rate-1/2 Encoder** - Dense GF(2) submatrix encoding (A, B⁻¹, C, D) for SB2 (1200→2400) and SB3/SB4 (870→1740). Matrices loaded from Annex 1 CSV files.
- **Block Interleaver** - 60×98 write-row/read-column interleaver for SB2+SB3+SB4 concatenation (5880 symbols).

### Gateway 3 - Navigation Message Framing

- **Synchronization Pattern Generator** - 68-symbol fixed sequence (0xCCA...A) for frame boundary detection per Table 12.
- **Subframe 1 Builder** - Frame ID and Time of Interval packed and BCH(51,8) encoded to 52 symbols.
- **Subframe 2 Builder** - Clock and Ephemeris data (1176 bits) with CRC-24 and LDPC encoding to 2400 symbols.
- **Subframe 3 Builder** - Variable broadcast messages with type field and payload (846 bits), CRC-24, and LDPC encoding to 1740 symbols.
- **Subframe 4 Builder** - Network access information (846 bits) with identical encoding pipeline to SB3, producing 1740 symbols.
- **Frame Assembler** - Concatenates all subframes with block interleaving, produces 6000-symbol complete frame (12 seconds at 500 sym/s).
- **Frame Export** - Binary (750 bytes), CSV (6000 lines), and hexadecimal (1500 chars) output formats.

### Gateway 4 - Baseband Signal Generation

- **BPSK Modulator** - Logic-level to signal-level mapping per LSIS Table 8: `0 → +1.0`, `1 → -1.0`.
- **AFS-I Data Modulation** - Navigation symbols XOR-modulated onto Gold primary epochs (2046 chips/symbol) per LSIS §2.3.
- **Dual-Channel I/Q Generator** - Time-aligned AFS-I (1.023 Mchip/s) and AFS-Q (5.115 Mchip/s) baseband generation with rational chip-index mapping.
- **Configurable Sample Rate** - Generates I/Q at any positive integer multiple of 1.023 MHz (default: 1.023 MHz workshop profile).
- **Signal Exporters** - Interleaved float32 little-endian binary (`[I0,Q0,I1,Q1,...]`) and CSV (`index,I,Q`) output formats.

### Gateway 5 - Frame Synchronization and Decoding

- **Integrated Receiver** - `DecodeAfsIIqSignal()` orchestrates I/Q normalization, Gold-code de-spreading, sync detection, frame extraction, BCH/LDPC decoding, CRC gating, and CRC stripping.
- **Robust Synchronization** - Normalized matched correlation with PSR, peak-to-RMS, and normalized-peak confidence gates recovers noisy, offset frames.
- **Soft FEC Decode** - Exhaustive BCH correlation and real-Annex-matrix normalized min-sum LDPC decoding preserve soft decisions and report convergence telemetry.
- **Gateway 6 Contract** - Accepted frames expose CRC-stripped SB2/SB3/SB4 payloads of 1176/846/846 bits; a cross-gateway test parses SB2 time fields and an SB3 almanac directly.
- **Decode CLI** - `goon decode` accepts headerless IQ32 or standardized LSISIQ input and emits JSON payloads plus acquisition/FEC telemetry.
- **Qualification** - Sync: 9960/10000 at 0.1 dB with a 99.4819% one-sided 95% lower bound; de-spread SER: 0/1000 at 0.1 dB; empirical post-LDPC BER: 0/299,880 bits at 3 dB with 102/102 CRC-accepted frames; worst measured three-subframe decode: 70.8 ms.
- **Usage and Limits** - See [docs/G5/GATEWAY5_DECODER.md](docs/G5/GATEWAY5_DECODER.md).

### Cross-Language Bridge

- **C API** - `extern "C"` DLL shim (`c_api.h`) exporting all generators and FEC functions for FFI access.
- **Python Bridge** - Zero-dependency ctypes wrapper (`lunanet.py`) with auto-DLL discovery and type-safe prototypes.
- **I/Q Signal Generator** - BPSK(1) baseband generator outputting float32 binary and CSV. Mapping: `0 → +1.0`, `1 → -1.0`.

### Gateway 5 Knowledge Bank

- External reference and scaffold notes for Gateway 5 are maintained in:
  - <https://github.com/KURE-x-Tech/Asteria-Knowledge-Base-G5-share>
- This knowledge bank is documentation/scaffold guidance and is not automatically integrated into this repository build.

---

## Project Structure

```text
Spreading-Codes/
├── CMakeLists.txt                  # Build system (C++17, MSVC/GCC/Clang)
├── config/
│   └── spreading_codes_config.ini  # Runtime configuration
├── codes/
│   ├── spreading_codes.h/.cpp      # Public C++ API
│   ├── c_api.h/.cpp                # C-linkage DLL exports
│   ├── test_engine.cpp             # Orchestration harness (gateway-scoped suites)
│   ├── gateway1/                   # Spreading code generators
│   │   ├── gold_code_generator.*   #   Gold code (2046 chips)
│   │   ├── weil_code_generator.*   #   Weil primary/tertiary
│   │   ├── tiered_code_generator.* #   AFS-Q three-tier construction
│   │   ├── spreading_config.*      #   Table loading & configuration
│   │   └── gui/                    #   Tkinter report viewer
│   ├── gateway2/                   # FEC encoding
│   │   ├── bch_codec.*             #   BCH(51,8) encoder + soft decoder
│   │   ├── crc24.*                 #   CRC-24Q
│   │   ├── ldpc_encoder.*          #   LDPC rate-1/2 encoder
│   │   └── interleaver.*           #   60×98 block interleaver
│   ├── gateway3/                   # Frame assembly
│   │   ├── sync_pattern.*          #   68-symbol sync generator
│   │   ├── subframe1_builder.*     #   SB1 BCH encoder
│   │   ├── subframe2_builder.*     #   SB2 LDPC encoder
│   │   ├── subframe3_builder.*     #   SB3 LDPC encoder
│   │   ├── subframe4_builder.*     #   SB4 LDPC encoder
│   │   ├── frame_assembler.*       #   Frame composition & interleaving
│   │   ├── frame_exporter.*        #   Binary/CSV/Hex output
│   │   └── frame_config.h          #   Configuration parameters
│   ├── gateway4/                   # Baseband signal generation
│   │   ├── bpsk_modulator.*        #   BPSK mapping + AFS-I data modulation
│   │   ├── iq_generator.*          #   Time-aligned I/Q sample generation
│   │   ├── signal_exporter.*       #   float32 binary + CSV exporters
│   │   └── signal_config.h         #   Chip-rate and frame timing constants
│   ├── gateway5/                   # Integrated AFS-I receiver and decoder
│   │   ├── frame_decoder.*         #   I/Q-to-payload orchestration + telemetry
│   │   ├── sync_detector.*         #   Normalized frame acquisition
│   │   ├── despreader.*            #   Gold-code acquisition/integrate-dump
│   │   ├── bch_soft_decoder.*      #   SB1 exhaustive soft decoder
│   │   ├── deinterleaver.*         #   60x98 soft deinterleaver
│   │   ├── ldpc_decoder.*          #   SB2-SB4 normalized min-sum decoder
│   │   └── crc_validator.*         #   CRC-24Q frame acceptance gate
│   ├── testing/                    # Test framework
│   │   ├── test_reporter.*         #   Markdown + JUnit XML output
│   │   ├── test_validators.*       #   Validation primitives
│   │   └── test_annex3_loader.*    #   Annex 3 hex reference parser
│   └── python/                     # Python bridge layer
│       ├── lunanet.py              #   ctypes wrapper
│       ├── iq_generator.py         #   BPSK(1) I/Q signal generation
│       └── test_bridge.py          #   Bridge smoke test
├── docs/                           # Spec tables, FAQ, requirements
├── Validation/
│   ├── annex3/                     # Reference vectors (txt + csv)
│   └── reports/                    # Timestamped test reports
└── CHANGELOG.md
```

---

## Build

For platform-specific reproducible build and validation commands (Windows + Linux), see `Reproduce.md`.

**Requirements:** CMake 3.16+, C++17 compiler (MSVC 2019+, GCC 9+, Clang 10+).

```bash
cmake -B build -S .
cmake --build build --config Release
```

Outputs:

- `build/bin/Release/lunanet_spreading_codes.dll` - shared library with C API
- `build/bin/Release/test_engine.exe` - validation harness

---

## Validation

Run the full configured validation scope:

```bash
# Multi-config (Visual Studio)
./build/bin/Release/test_engine config/spreading_codes_config.ini

# Single-config (Ninja/Unix)
./build/bin/test_engine config/spreading_codes_config.ini
```

Run modular CTest targets:

```bash
# Run all gateway validation targets
ctest --test-dir build --output-on-failure

# Run specific gateway targets
ctest --test-dir build -R gateway1_validation --output-on-failure
ctest --test-dir build -R gateway2_validation --output-on-failure
ctest --test-dir build -R gateway3_validation --output-on-failure
ctest --test-dir build -R gateway4_validation --output-on-failure
```

Run only one gateway validation scope:

```bash
# Multi-config (Visual Studio)
./build/bin/Release/test_engine config/spreading_codes_config.ini --gateway gateway1
./build/bin/Release/test_engine config/spreading_codes_config.ini --gateway gateway2
./build/bin/Release/test_engine config/spreading_codes_config.ini --gateway gateway3
./build/bin/Release/test_engine config/spreading_codes_config.ini --gateway gateway4

# Single-config (Ninja/Unix)
./build/bin/test_engine config/spreading_codes_config.ini --gateway gateway1
./build/bin/test_engine config/spreading_codes_config.ini --gateway gateway2
./build/bin/test_engine config/spreading_codes_config.ini --gateway gateway3
./build/bin/test_engine config/spreading_codes_config.ini --gateway gateway4
```

Run the Gateway 5 component, full-frame, and Gateway 6 handoff tests:

```bash
ctest --test-dir build -R '^gateway5_' --output-on-failure

# Longer deterministic BER qualification
./build/bin/gateway5_ber_benchmark
```

Gateway 4 validation includes an **EndToEnd/Pipeline** integration scope:
frame assembly (Gateway 3) -> AFS-I modulation -> AFS-Q generation -> I/Q generation -> IQ32 export checks.

Test totals evolve as suites are added. Treat the generated report artifacts under `Validation/reports/` as the source of truth for exact counts in your build.

Reports are written to `Validation/reports/YYYY-MM-DD/HH-MM-SS.{md,xml}` for full runs, or `Validation/reports/YYYY-MM-DD/HH-MM-SS_gatewayX.{md,xml}` for gateway-filtered runs.

### Report Viewer GUI

```bash
python codes/gateway1/gui/report_viewer.py
```

Dark-themed Tkinter viewer with color-coded pass/fail rows, suite/status filtering, and auto-discovery of timestamped reports.

---

## Python Usage

```python
from codes.python.lunanet import LunaNet

ln = LunaNet("config/spreading_codes_config.ini")

# Generate spreading codes
gold = ln.generate_gold(1)           # 2046-chip Gold code
weil_p = ln.generate_weil_primary(1) # 10230-chip Weil primary
weil_t = ln.generate_weil_tertiary(1)# 1500-chip Weil tertiary
afs_q = ln.generate_afs_q(1)        # Tiered AFS-Q code

# FEC encoding
bch = ln.bch_encode(0x0A5)          # BCH(51,8) → 52 symbols
crc = ln.crc24([1, 0, 1, 1])       # CRC-24Q → 24-bit value
```

### I/Q Signal Generation

```bash
python codes/python/iq_generator.py \
    --config config/spreading_codes_config.ini \
    --prn 1 \
    --output Validation/iq_output \
    --format both
```

Outputs `prn001_afs_i.bin`, `prn001_afs_q.bin`, `prn001_iq_interleaved.bin`, and `prn001_iq.csv`.

---

## Performance

Full PRN generation pipeline (Gold + Weil Primary + Weil Tertiary + AFS-Q) completes in **< 0.5 ms per PRN**, well under the SC-1.7 requirement of < 1 second. Complete frame assembly (6000 symbols) completes in **< 100 milliseconds**.

| Operation                   | Time     |
| --------------------------- | -------- |
| Gold code (2046 chips)      | ~0.05 ms |
| Weil Primary (10230 chips)  | ~0.1 ms  |
| AFS-Q tiered (1 epoch)      | ~0.3 ms  |
| LDPC SB2 encode (1200→2400) | < 100 ms |
| Frame assembly (6000 sym)   | < 100 ms |

---

## Specification Compliance

| Requirement                          | Status | Tests    |
| ------------------------------------ | ------ | -------- |
| SC-1.1 Gold code generation          | PASS   | 210/210  |
| SC-1.2 Weil primary generation       | PASS   | 210/210  |
| SC-1.3 Weil tertiary generation      | PASS   | 210/210  |
| SC-1.6 Table 11 node assignments     | PASS   | 60/60    |
| SC-1.7 Performance (< 1s/PRN)        | PASS   | 3/3      |
| FEC-2.1 BCH(51,8) encoder            | PASS   | 10/10    |
| FEC-2.3 LDPC rate-1/2 encoder        | PASS   | 12/12    |
| FEC-2.5 CRC-24Q                      | PASS   | 4/4      |
| FEC-2.7 Block interleaver            | PASS   | 4/4      |
| FM-2.5 Frame assembly (Figure 9)     | PASS   | 10/10    |
| FM-2.6 Subframe bit allocations      | PASS   | 8/8      |
| FM-2.7 Frame timing (12 seconds)     | PASS   | verified |
| SG-4.3 BPSK(1) I/Q mapping           | PASS   | verified |

---

## Roadmap

- [x] Gateway 1 - Spreading code generation (210 PRNs, all types)
- [x] Gateway 1C - Table 11 interim code validation
- [x] Gateway 2 - FEC encoding (BCH, CRC-24, LDPC, interleaver)
- [x] Gateway 3 - Frame assembly (sync + SB1 + interleaved SB2-4)
- [x] Python bridge layer (ctypes + C API)
- [x] BPSK(1) I/Q signal generation
- [x] Gateway 4 - Baseband signal generation (BPSK modulation, I/Q samples)
- [x] Gateway 5 - Frame synchronization, FEC decode, CRC gate, CLI, and qualification
- [ ] Gateway 6 - Message parsing pipeline (SB2/SB3 and Gateway 5 handoff implemented)
- [x] End-to-end pipeline validation

---

## License

Part of the KURE-x-Tech LunaNet competition entry.
