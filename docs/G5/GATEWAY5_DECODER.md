# Gateway 5 Receiver and Decoder

Gateway 5 converts an AFS-I I/Q recording into CRC-validated navigation
payloads for Gateway 6. The integrated implementation is owned by
`gateway5/frame_decoder.h`; the lower-level stages remain public for focused
testing and alternate receiver front ends.

## Quick Start

Build the CLI:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target goon --parallel
```

Generate a 12-second headerless IQ32 signal and decode it:

```bash
./build/bin/goon encode \
  --format iq32 \
  --prn 7 \
  --fid 2 \
  --toi 73 \
  --wn 1234 \
  --itow 256 \
  --output signal.iq32

./build/bin/goon decode \
  --input signal.iq32 \
  --input-format raw \
  --prn 7 \
  --rate 1023000 \
  --output decoded.json
```

`--input-format raw` is the default. A raw IQ32 file contains interleaved
little-endian float32 pairs `[I0,Q0,I1,Q1,...]` and has no metadata, so its PRN
must be supplied. The raw sample-rate default is 1,023,000 Hz.

For a file written with `ExportIqBinaryStandard`, use:

```bash
./build/bin/goon decode \
  --input signal.lsisiq \
  --input-format standard \
  --output decoded.json
```

The standardized 128-byte header supplies PRN and sample rate. If `--prn` or
`--rate` is also provided, it must match the header.

The command exits with:

- `0`: the frame passed BCH/LDPC decoding and all three CRC checks.
- `1`: invalid arguments, configuration, matrix data, or I/O.
- `2`: acquisition or decoding failed, including CRC rejection.

## Output Contract

Successful output is JSON containing:

- `fid` and `toi` decoded from SB1.
- acquisition telemetry: `code_phase`, `frame_offset`,
  `lock_correlation`, and `sync_psr`.
- decoder telemetry: elapsed milliseconds and LDPC iterations for SB2-SB4.
- per-subframe CRC verdicts.
- CRC-stripped payloads as `sb2_hex`, `sb3_hex`, and `sb4_hex`.
- payload bit counts of 1176, 846, and 846 respectively.
- `*_hex_padding_bits`, which reports right-side zero padding in the final
  hexadecimal nibble. Bits are emitted MSB-first; SB3 and SB4 each have two
  padding bits that are not part of the 846-bit payload.

The payload vectors are the direct Gateway 6 input contract:

| Payload | Gateway 5 LDPC output | CRC removed | Gateway 6 input |
| ------- | --------------------- | ----------- | --------------- |
| SB2 | 1200 bits | 24 bits | 1176 bits |
| SB3 | 870 bits | 24 bits | 846 bits |
| SB4 | 870 bits | 24 bits | 846 bits |

Gateway 6 must only parse these payloads when `accepted` is true. Gateway 5
does not release partially valid frames.

## Receiver Pipeline

```text
IQ32 import
  -> I-channel validation and integer-ratio downsampling
  -> Gold-code acquisition and AFS-I de-spreading
  -> normalized 68-symbol sync correlation
  -> 6000-symbol frame extraction
  -> SB1 soft BCH decode
  -> 60x98 soft deinterleave
  -> SB2/SB3/SB4 LLR generation
  -> normalized min-sum LDPC decode
  -> CRC-24Q frame gate
  -> CRC stripping
  -> Gateway 6 payloads
```

### 1. I/Q Import and Normalization

`ImportIqBinary` reads the headerless workshop format. It rejects empty files,
partial I/Q pairs, invalid sample rates, truncated reads, and non-finite
samples. `ImportIqBinaryStandard` validates the standardized header and
metadata. Binary import and export enforce the same 512 MiB payload ceiling,
finite I/Q values, and nonempty paired samples; allocation failures become
normal errors. The limit admits a complete 12-second recording at 5.115 MHz
while bounding untrusted-file memory use.

`DecodeAfsIIqSignal` accepts sample rates that are positive integer multiples
of 1.023 MHz. Multiple samples per AFS-I chip are averaged before de-spreading.

### 2. De-spreading

`DespreadAfsI` generates the selected PRN's 2046-chip Gold code, searches all
integer code phases, and chooses the largest cosine-normalized correlation.
Acquisition is invariant to uniform input gain. Once
locked, it performs integrate-and-dump over each 2046-chip epoch to recover one
soft navigation symbol.

### 3. Frame Synchronization

`DetectFrameSync` slides the fixed 68-symbol pattern over the soft-symbol
stream. Candidate selection uses normalized matched correlation so high-energy
noise windows are not favored. A detection must clear three scale-independent
confidence checks:

- positive peak-to-sidelobe ratio;
- peak-to-RMS correlation-floor ratio;
- normalized peak correlation.

### 4. Frame and FEC Decode

`ExtractFrameSymbols` returns SP(68), SB1(52), and the interleaved SB2-SB4
region(5880). SB1 is decoded by exhaustive soft correlation over all 256 BCH
hypotheses. The winning hypothesis must clear normalized-correlation and
runner-up-margin gates; all-zero erasures and tied/ambiguous inputs are
rejected. The remaining region is deinterleaved and split into
2400/1740/1740 symbols.

`DecodeLdpcMinSum` restores punctured codeword positions, pins shortened
SB3/SB4 filler variables to zero, saturates finite belief messages, and runs
normalized min-sum for at most 50 iterations. A zero syndrome and valid
shortened-bit invariants are required for convergence.

### 5. Acceptance and Handoff

`ValidateFrameCrc` validates CRC-24Q independently for SB2, SB3, and SB4. The
frame is accepted only when every check passes. The integrated decoder then
removes each 24-bit CRC and returns payload vectors ready for Gateway 6.

## C++ API

```cpp
lunanet::gateway1::SpreadingSpecTables spreading_tables;
lunanet::gateway1::Annex3Paths annex3_paths;
std::string error;

lunanet::gateway1::LoadSpreadingConfig(
    "config/spreading_codes_config.ini",
    &spreading_tables,
    &annex3_paths,
    &error);

lunanet::gateway5::DecoderMatrices matrices;
lunanet::gateway5::LoadDecoderMatrices(
    "Validation/annex3/csv", &matrices, &error);

lunanet::gateway4::IqSignal signal;
lunanet::gateway4::ImportIqBinary(
    "signal.iq32", 1023000, &signal, &error);

lunanet::gateway5::FrameDecoderConfig config;
config.prn = 7;
config.symbol_noise_variance = 1.0;

const auto decoded = lunanet::gateway5::DecodeAfsIIqSignal(
    signal, spreading_tables, matrices, config);

if (!decoded.accepted) {
    // decoded.error and acquisition/FEC telemetry identify the failed stage.
}
```

The noise variance is the post-de-spreading per-symbol AWGN variance used for
LLR scaling. A positive value is required. Scaling all LLRs by a common
positive factor does not change hard decisions, but realistic variance improves
decoder confidence calibration.

## Validation

Run the automated Gateway 5 and handoff tests:

```bash
ctest --test-dir build -R '^gateway5_' --output-on-failure
```

The Gateway 5 CTest group includes the longer deterministic BER qualification.
Run that executable directly when only the benchmark report is needed:

```bash
./build/bin/gateway5_ber_benchmark
```

Measured Release-build evidence on 2026-08-10:

| Requirement | Result |
| ----------- | ------ |
| Sync reliability | 9960/10000 correct at 0.1 dB; one-sided 95% lower bound 99.4819% |
| No-frame false alarms | 16/10000; one-sided 95% upper bound 0.2406% |
| De-spread symbol errors | 0/1000 at 0.1 dB |
| Full-frame noisy decode | Exact payload recovery at 3 dB and nonzero frame offset |
| BER qualification | 0 errors in 299,880 decoded bits at 3 dB |
| CRC acceptance | 102/102 BER-benchmark frames |
| LDPC iterations | Maximum 10 in the BER benchmark |
| Decode latency | Worst three-subframe BER trial 70.8 ms; integrated test < 1 s |
| Gateway 5 -> 6 | SB2 time fields and SB3 almanac parsed without mutation |

The BER executable uses a fixed seed, real Annex matrices, AWGN, and 102 sets
of SB2/SB3/SB4 codewords. It reports the empirical post-LDPC BER and CRC
acceptance for that reproducible campaign. Bits within an LDPC codeword are
correlated, so the observed zero-error result is not presented as an
independent-bit statistical confidence bound.

## Operating Boundaries

The implemented competition profile is a single-frame software AFS-I receiver
with a known PRN, integer chip timing, and AWGN soft decisions. Multiple
genuine sync peaks in one search window are not qualified. It does not perform carrier
or Doppler tracking, sub-chip timing recovery, automatic PRN search, AFS-Q
pilot acquisition, or arbitrary-rate resampling.

The full-frame qualification currently passes at 3 dB. A deterministic 1 dB
full-frame LDPC case did not converge during implementation, so this repository
does not claim full-frame reliability across every positive SNR. Sync and
de-spreading are independently qualified at 0.1 dB; broader BER-vs-SNR curves
and external interoperability recordings remain Gateway 7 work.
