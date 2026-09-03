# Validation and Evidence Guide

This document explains how to collect the evidence referenced by the submission data pack. It separates executable test results from measured qualification results and from requirements that are not yet supported by a checked-in artifact.

## Evidence Sources

| Evidence | Source |
| --- | --- |
| Gold, Weil, and assignment validation | `test_engine`, `codes/testing/`, `Validation/annex3/`, and `docs/spec_tables/table_10_secondary_codes.csv` / `table_11_code_assignments.csv` |
| Frame structure and encoding | `codes/gateway3/`, `gateway3_frame_exporter_test`, and `docs/spec_tables/table_12_sync_pattern.csv` / `table_14_frame_structure.csv` |
| Signal generation and export | `codes/gateway4/`, `gateway4_signal_exporter_test`, and `goon encode --format iq32` |
| Receiver behavior | `codes/gateway5/`, Gateway 5 CTest targets, and [GATEWAY5_DECODER.md](../G5/GATEWAY5_DECODER.md) |
| Message parsing and handoff | `codes/gateway6/`, parser tests, and `gateway5_gateway6_handoff_test`, including relative SB2 ToT preservation, provisional SB2 field-block extraction, and SB4 network-access raw payload extraction |
| Automated report artifacts | `Validation/reports/YYYY-MM-DD/` after running `test_engine` |

## Reproducible Test Sequence

Use a fresh build directory so the result cannot be confused with an older CMake cache:

```bash
cmake -S . -B build-validation -DCMAKE_BUILD_TYPE=Release
cmake --build build-validation --parallel
ctest --test-dir build-validation --output-on-failure
./build-validation/bin/test_engine config/spreading_codes_config.ini
./build-validation/bin/gateway5_ber_benchmark
```

For Visual Studio, use `--config Release` with the build and CTest commands, and use `build-validation/bin/Release/` for executables.

Run focused checks when investigating one gateway:

```bash
ctest --test-dir build-validation -R '^gateway3_' --output-on-failure
ctest --test-dir build-validation -R '^gateway4_' --output-on-failure
ctest --test-dir build-validation -R '^gateway5_' --output-on-failure
ctest --test-dir build-validation -R '^gateway6_' --output-on-failure
```

Run the Python bridge smoke test after building the shared library:

```bash
python3 codes/python/test_bridge.py
```

## Qualification Results

The current repository documentation records the following Gateway 5 qualification results from a Release build dated 2026-08-10:

| Measure | Result |
| --- | --- |
| Sync detection | 9960/10000 at 0.1 dB |
| One-sided 95% lower bound for sync | 99.4819% |
| No-frame false alarms | 16/10000; one-sided 95% upper bound 0.2406% |
| De-spread symbol errors | 0/1000 at 0.1 dB |
| Post-LDPC BER | 0/299,880 decoded bits at 3 dB |
| CRC acceptance | 102/102 frames in the BER campaign |
| LDPC iterations | Maximum 10 in the BER campaign; decoder limit is 50 |
| Decode latency | Worst measured three-subframe BER trial: 70.8 ms |

These results qualify the documented operating envelope: known PRN, AFS-I input, integer chip timing, integer-multiple sample rate, and AWGN. They do not establish performance for external recordings, fractional timing, fading, interference, or multi-PRN acquisition.

## How to Report Test Counts

The project has more than one test accounting system:

- CTest reports pass/fail at the executable-test level.
- `test_engine` reports individual validation cases and writes Markdown and JUnit XML.
- The Python bridge smoke test is separate from CTest unless invoked explicitly.

The final submission should state which system produced each number. Do not call an executable-test count a source-code coverage percentage. A coverage percentage requires a coverage tool and its output.

Use this table as a final-run worksheet:

| Category | Total | Passing | Failing | Skipped | Evidence path |
| --- | ---: | ---: | ---: | ---: | --- |
| Unit tests | TBD | TBD | TBD | TBD | Fresh CTest output or report |
| Integration tests | TBD | TBD | TBD | TBD | Fresh CTest output or report |
| Compliance tests | TBD | TBD | TBD | TBD | `Validation/reports/...` |
| Performance tests | TBD | TBD | TBD | TBD | `Validation/reports/...` and benchmark output |

## Requirement Interpretation

### Demonstrated

- Code generation against Annex 3 vectors for the implemented code families.
- Table 10 secondary-code values and Table 11 interim assignments.
- 6000-symbol frame assembly with 68-symbol sync, 52-symbol SB1, and 5880 interleaved SB2-SB4 symbols.
- BPSK and AFS-I/AFS-Q signal generation with supported sample-rate constraints.
- Normalized frame synchronization, soft BCH/LDPC decoding, CRC gating, and accepted-payload handoff.
- SB1-SB4 parser modules, relative SB2 ToT computation, and local Gateway 5-to-6 integration.

### Not Yet Demonstrated

- Absolute UTC/GPS-style ToT conversion because the LRT epoch is unspecified in the source material.
- All detailed SB2, SB3, and SB4 message semantics where the source specification is provisional or incomplete.
- External interoperability with another implementation.
- Multi-node routing and network behavior.
- A broad BER-versus-SNR curve, including a separately measured 0 dB result.
- A source-code coverage percentage above 90%.
- Full compliance closure for every specification `shall` requirement.

## Final Evidence Checklist

- [ ] Run a fresh configure/build with the final compiler and platform.
- [ ] Run CTest and save the console output or generated report paths.
- [ ] Run `test_engine` and record the generated Markdown and XML filenames.
- [ ] Run the Gateway 5 BER qualification and record its output.
- [ ] Run a local encode-to-IQ-to-decode-to-JSON example.
- [ ] Confirm the JSON contains accepted CRC results and `subframes`.
- [ ] Record external interoperability results, or state that none were performed.
- [ ] Replace team metadata and score placeholders in the final root `SUBMISSION.md`.
- [ ] Re-check every claim in `SUBMISSION.md` against the final artifacts.
