# GitHub Copilot Instructions for Spreading-Codes (LunaNetGateway)

This repository implements the LunaNet Signal-in-Space (LSIS-AFS) communications and signal processing pipeline in modern C++17, with supporting Python bindings, test harnesses, and the `goon` workshop interop CLI.

---

## 1. Architecture & Gateway Pipeline

The signal chain is organized into discrete gateway stages under [codes/](../codes/):

- **Gateway 1 (Spreading Codes)**: [codes/gateway1/](../codes/gateway1/) - Gold code LFSR generators (2046-chip), Weil primary/tertiary sequences with cached Legendre evaluations, tiered AFS-Q secondary code generation, and Table 11 interim code assignments.
- **Gateway 2 (FEC Encoding)**: [codes/gateway2/](../codes/gateway2/) - BCH(51,8) LFSR encoding, CRC-24Q (poly `0x864CFB`), LDPC rate-1/2 dense GF(2) generator matrices (SB2: 1200→2400, SB3/SB4: 870→1740), and 60×98 block interleaver.
- **Gateway 3 (Frame Assembly)**: [codes/gateway3/](../codes/gateway3/) - 6000-symbol frame assembly (Sync Pattern + SB1 + SB2/SB3/SB4 interleaved), payload export.
- **Gateway 4 (Signal Generation)**: [codes/gateway4/](../codes/gateway4/) - BPSK baseband modulation, AFS-I data spreading, rational sample-rate mapping (default 1.023 MHz), and interleaved float32 I/Q generation (`.iq32`).
- **Gateway 5 (Integrated Receiver)**: [codes/gateway5/](../codes/gateway5/) - Gain-normalized Gold-code despreading, matched-correlation frame sync detector, confidence-gated BCH soft decoding, 60×98 soft deinterleaving, saturated min-sum LDPC decoder, and CRC-24Q validation gate.
- **Gateway 6 (Message Parsing)**: [codes/gateway6/](../codes/gateway6/) - Subframe parsing and field extraction (SB1 header/counters, SB2 ephemeris/clock, SB3 broadcast/almanac/alerts, SB4 auxiliary).
- **CLI & Interop**: [codes/lsis_cli.cpp](../codes/lsis_cli.cpp) - The `goon` CLI supporting `generate-codes`, `encode`, `decode`, and `version`.
- **Test Framework**: [codes/testing/](../codes/testing/) and [codes/test_engine.cpp](../codes/test_engine.cpp) - Modular test engine with Markdown and JUnit XML report outputs.

---

## 2. C++ & Signal Processing Conventions

- **Language Standard**: C++17 (`set(CMAKE_CXX_STANDARD 17)`). Strict const-correctness, RAII, and boundary-checked buffers.
- **Soft Decisions vs. Hard Bits**:
     - In receiver/demodulator stages (Gateway 5), maintain signed `double`/`float` soft log-likelihood ratios (LLRs) or correlation metrics throughout the pipeline.
     - Do NOT hard-slice to binary bits (`0`/`1`) before BCH or LDPC decoding.
- **Interleaver / Deinterleaver Geometry**:
     - Interleaver (Gateway 2): 60 rows × 98 columns (5880 symbols). Write column-wise, read row-wise.
     - Deinterleaver (Gateway 5): Exact inverse. Write row-wise into 60×98, read column-wise.
     - Subframe split after deinterleave: SB2 = 2400 symbols, SB3 = 1740 symbols, SB4 = 1740 symbols.
     - SP (60 symbols) and SB1 (60 symbols) remain un-interleaved.
- **Bit & Byte Ordering**:
     - CRC-24Q generator polynomial: `0x864CFB` with init `0x000000`.
     - Bit-packing must follow LSIS-AFS spec conventions: MSB-first unless explicitly indicated by the subframe descriptor.
- **No Heavy External Dependencies**: Core C++ libraries rely solely on standard library (`<vector>`, `<cmath>`, `<cstdint>`, `<algorithm>`, `<fstream>`). Keep runtime dependency footprint minimal.

---

## 3. Terminal CLI & Build Workflow

Always use terminal CLI commands for configuring, building, and running tests:

### Standard Build Workflow

```bash
# Configure (Release mode recommended for DSP performance)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build targets in parallel
cmake --build build --parallel

# Run all registered CTest suites
ctest --test-dir build --output-on-failure
```

### CLI Tool (`goon`) Conventions

- Binary location: `build/bin/goon` (or `build/bin/Release/goon.exe` on Windows).
- Single Version Source: `project(LunaNetGateway VERSION x.y.z)` in [CMakeLists.txt](../CMakeLists.txt) configures `generated/lunanet/version.h`.
- When updating or testing CLI functionality, verify with:
     ```bash
     ./build/bin/goon version
     ./build/bin/goon --help
     ```
- Remember that `./build/bin/goon` runs the locally compiled binary. A system-installed `goon` requires `cmake --install build --prefix <prefix>`.

---

## 4. Testing & Verification Rules

1. **Mandatory Round-Trip Identity Tests**:
      - Any encoder/decoder or interleaver/deinterleaver pair MUST include a bit-exact round-trip identity test (e.g. `Deinterleave(Interleave(0..5879)) == 0..5879`).
2. **Target & CTest Registration**:
      - New standalone tests must be added as executable targets in [CMakeLists.txt](../CMakeLists.txt), set `RUNTIME_OUTPUT_DIRECTORY` to `${CMAKE_BINARY_DIR}/bin`, and register with `add_test(NAME <target_name> COMMAND <target_name>)`.
3. **Annex3 Compliance**:
      - PRN generation must maintain 210/210 compliance with Annex3 validation vectors under `Validation/annex3/`.
4. **Test Engine Reports**:
      - Full validation reports are emitted to `Validation/reports/YYYY-MM-DD/HH-MM-SS.{md,xml}` when running `test_engine`.

---

## 5. Specification & Documentation Grounding

- When working on message parsing (Gateway 6) or framing (Gateway 3/5), consult the reference tables and specifications in [docs/](../docs/) and [docs/spec_tables/](../docs/spec_tables/) rather than guessing field alignments or bit lengths.
- Keep [docs/Reproduce.md](../docs/Reproduce.md) and [MEMORY.md](../MEMORY.md) synchronized when introducing new CLI subcommands, pipeline stages, or build prerequisites.
