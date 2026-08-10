# Reproduce Build, Run, and Validation

This runbook covers the supported Windows, macOS, and Linux workflows for
building the project, running the `goon` command-line application, and executing
the Gateway 1-6 validation tests.

Run every command from the repository root. The CLI resolves its default
configuration and Annex 3 data relative to the repository, so running it from a
different directory may require explicit `--config` and `--csv-dir` arguments.

## Prerequisites

- CMake 3.16 or newer
- A C++17 compiler
- Git

Platform toolchains:

- Windows: Visual Studio 2019 or newer with the Desktop development with C++
  workload, or Ninja with an MSVC/Clang environment
- macOS: Xcode Command Line Tools (`xcode-select --install`)
- Linux: GCC 9 or newer or Clang 10 or newer, plus Make or Ninja

Check the tools before configuring:

```bash
cmake --version
c++ --version
```

On Windows, run the compiler check from a Developer PowerShell if using MSVC:

```powershell
cmake --version
cl
```

## Quick Start

### macOS or Linux

```bash
cd /path/to/Spreading-Codes
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bin/goon version
ctest --test-dir build --output-on-failure
```

Expected application outputs:

- macOS: `build/bin/goon`, `build/bin/test_engine`, and
  `build/lib/liblunanet_spreading_codes.dylib`
- Linux: `build/bin/goon`, `build/bin/test_engine`, and
  `build/lib/liblunanet_spreading_codes.so`

### Windows with Visual Studio

Visual Studio is a multi-configuration generator, so include `--config Release`
when building and `-C Release` when running CTest.

```powershell
cd <path>\Spreading-Codes
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
.\build\bin\Release\goon.exe version
ctest --test-dir build -C Release --output-on-failure
```

Expected application outputs:

- `build/bin/Release/goon.exe`
- `build/bin/Release/test_engine.exe`
- `build/bin/Release/lunanet_spreading_codes.dll`

### Windows with Ninja

Run these commands from a Developer PowerShell so Ninja can locate the selected
compiler.

```powershell
cd <path>\Spreading-Codes
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
.\build\bin\goon.exe version
ctest --test-dir build --output-on-failure
```

Do not reuse one build directory with a different CMake generator. Use a new
directory such as `build-vs` or `build-ninja` when switching generators.

## Run the Software

The main application is `goon`. It can generate spreading-code families,
assemble a 6000-symbol navigation frame, or generate an interleaved float32 I/Q
signal.

Set a convenient executable variable for the examples below.

```bash
# macOS or Linux
GOON=./build/bin/goon
```

```powershell
# Visual Studio build; use .\build\bin\goon.exe for Ninja
$GOON = ".\build\bin\Release\goon.exe"
```

Display the complete built-in help or version:

```bash
"$GOON" --help
"$GOON" version
```

```powershell
& $GOON --help
& $GOON version
```

### Generate Spreading Codes

Generate all supported code families into a directory:

```bash
"$GOON" generate-codes --codes all --output Validation/generated/cli_codes
```

```powershell
& $GOON generate-codes --codes all --output Validation\generated\cli_codes
```

Generate only the Gold family as one text file:

```bash
"$GOON" generate-codes --codes gold --output Validation/generated/gold_codes.txt
```

### Assemble a Navigation Frame

```bash
"$GOON" encode \
  --format frame \
  --prn 1 --fid 0 --toi 42 --wn 100 --itow 250 \
  --config config/spreading_codes_config.ini \
  --csv-dir Validation/annex3/csv \
  --output Validation/generated/example_frame.bin
```

PowerShell uses a backtick for line continuation:

```powershell
& $GOON encode `
  --format frame `
  --prn 1 --fid 0 --toi 42 --wn 100 --itow 250 `
  --config config\spreading_codes_config.ini `
  --csv-dir Validation\annex3\csv `
  --output Validation\generated\example_frame.bin
```

### Generate an I/Q Signal

Replace `frame` with `iq32` to run frame assembly, spreading, modulation, and I/Q
export end to end. A full 12-second signal is much larger than a frame file.

```bash
"$GOON" encode \
  --format iq32 \
  --prn 1 --fid 0 --toi 42 --wn 100 --itow 250 \
  --rate 1023000 \
  --config config/spreading_codes_config.ini \
  --csv-dir Validation/annex3/csv \
  --output Validation/iq_output/example_signal.iq32
```

The accepted ranges are PRN 1-210, FID 0-3, TOI 0-99, WN 0-8191, and ITOW
0-511. Run `goon --help` for the command summary.

## Run Validation

### Recommended: CTest

CTest knows the correct working directory and executable path for all configured
tests. Run the complete suite after every clean build:

```bash
ctest --test-dir build --output-on-failure
```

For a Visual Studio build, add the selected configuration:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

List the tests without running them:

```bash
ctest --test-dir build -N
```

Run one gateway or one exact test with a regular-expression filter:

```bash
ctest --test-dir build -R '^gateway1_' --output-on-failure
ctest --test-dir build -R '^gateway2_' --output-on-failure
ctest --test-dir build -R '^gateway3_' --output-on-failure
ctest --test-dir build -R '^gateway4_' --output-on-failure
ctest --test-dir build -R '^gateway5_' --output-on-failure
ctest --test-dir build -R '^gateway6_' --output-on-failure
ctest --test-dir build -R '^gateway6_subframe3_parser_test$' --output-on-failure
```

Add `-C Release` to these commands for a Visual Studio build.

The Gateway 5 CTest group covers frame synchronization, sync detection,
despreading, symbol extraction, BCH decoding, deinterleaving, LDPC decoding,
CRC validation, complete frame decoding, and the Gateway 5-to-6 navigation
handoff. Gateway 6 also has focused Subframe 2 and Subframe 3 parser tests.

The Gateway 5 CTest filter includes the longer deterministic BER qualification.
Run it directly when only the benchmark report is needed:

```bash
./build/bin/gateway5_ber_benchmark
```

For Visual Studio, use `build\bin\Release\gateway5_ber_benchmark.exe`.

The benchmark decodes 102 real-matrix SB2/SB3/SB4 sets at 3 dB. Its fixed seed
and 299,880 decoded bits make the empirical BER and CRC-acceptance result
reproducible. It does not treat correlated bits within an LDPC codeword as
independent statistical trials.

### Decode a Navigation Signal

Generate and decode the headerless workshop IQ32 format:

```bash
./build/bin/goon encode \
  --format iq32 --prn 7 --fid 2 --toi 73 --wn 1234 --itow 256 \
  --output signal.iq32

./build/bin/goon decode \
  --input signal.iq32 --input-format raw --prn 7 --rate 1023000 \
  --output decoded.json
```

`decoded.json` contains FID/TOI, acquisition and LDPC telemetry, CRC verdicts,
and CRC-stripped SB2/SB3/SB4 payloads. See `docs/G5/GATEWAY5_DECODER.md` for the
standardized input format, API usage, stage behavior, and operating limits.

### Validation Report Engine

`test_engine` runs the report-producing Gateway 1-4 validation suites. CTest is
still required for the standalone Gateway 3-6 component tests.

```bash
./build/bin/test_engine config/spreading_codes_config.ini
./build/bin/test_engine config/spreading_codes_config.ini --gateway gateway1
./build/bin/test_engine config/spreading_codes_config.ini --gateway gateway2
./build/bin/test_engine config/spreading_codes_config.ini --gateway gateway3
./build/bin/test_engine config/spreading_codes_config.ini --gateway gateway4
```

For Windows, use `build\bin\Release\test_engine.exe` with Visual Studio or
`build\bin\test_engine.exe` with Ninja.

Reports are written to:

- `Validation/reports/YYYY-MM-DD/HH-MM-SS.md`
- `Validation/reports/YYYY-MM-DD/HH-MM-SS.xml`

Gateway-filtered runs add `_gatewayX` to the filename.

### Run Component Executables Directly

CTest is preferred, but a single-config macOS/Linux build can run a component
test directly:

```bash
./build/bin/gateway5_crc_validation_test
./build/bin/gateway5_frame_decoder_test
./build/bin/gateway5_gateway6_handoff_test
./build/bin/gateway6_subframe2_parser_test
./build/bin/gateway6_subframe3_parser_test
```

For Windows, add `.exe` and use either `build\bin\Release\` for Visual Studio or
`build\bin\` for Ninja.

## Clean Reproduction

For a release-candidate check, configure a fresh build tree rather than relying
on incremental artifacts:

```bash
cmake -S . -B build-clean -DCMAKE_BUILD_TYPE=Release
cmake --build build-clean --parallel
ctest --test-dir build-clean --output-on-failure
```

Use the Visual Studio generator, `--config Release`, and `-C Release` equivalents
from the Windows quick start when creating a clean Visual Studio build.

## Troubleshooting

- `CTest` reports no tests: rerun `cmake -S . -B build`, then check
  `ctest --test-dir build -N`.
- Executable not found: multi-configuration generators normally use
  `build/bin/Release/`; single-configuration generators use `build/bin/`.
- CMake reports a generator mismatch: configure a new build directory instead
  of reusing one created by another generator.
- Configuration or Annex 3 data cannot be found: run from the repository root,
  or pass `--config config/spreading_codes_config.ini` and
  `--csv-dir Validation/annex3/csv` to `goon encode`.
- Shared-library loading fails: build the executable and library in the same
  build tree and run from the repository root.
- Git on a macOS external volume reports `non-monotonic index` for a
  `.git/objects/pack/._pack-*` file: remove only those AppleDouble sidecar files;
  do not remove the corresponding `pack-*` files.
