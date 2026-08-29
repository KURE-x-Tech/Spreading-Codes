# Gateway Todo Tracker

Last updated: 2026-08-10

## Status Snapshot

- Gateway 1: Complete and Annex3-validated.
- Gateway 2: Complete for encode pipeline (BCH/CRC/LDPC encode/interleaver).
- Gateway 3: Complete frame assembly pipeline.
- Gateway 4: Complete baseband generation pipeline.
- Gateway 5: Integrated AFS-I receiver implemented and qualified for the documented software profile.
- Gateway 6: SB1-SB4 parsing, relative ToT computation, and tested Gateway 5 payload handoff are implemented.

## Gateway 5 Completion Evidence

### In place

- `goon decode` for headerless IQ32 and standardized LSISIQ files.
- I-channel normalization and Gold-code de-spreading with code-phase acquisition.
- Normalized noisy/offset frame synchronization over the fixed 68-symbol SP.
- 6000-symbol extraction, BCH decode, soft deinterleaving, LDPC decode, and CRC gate.
- CRC-stripped 1176/846/846-bit Gateway 6 payload contract.
- Full-frame and Gateway 5-to-6 navigation handoff tests.
- Sync qualification: 9960/10000 detections at 0.1 dB with a 99.4819% one-sided 95% lower bound; 16/10000 false alarms with a 0.2406% upper bound.
- De-spread qualification: 0/1000 symbol errors at 0.1 dB.
- BER qualification: empirical 0/299,880 post-LDPC bit errors at 3 dB with 102/102 CRC-accepted frames.
- Decode qualification: fewer than 50 LDPC iterations and under one second.

### Residual Gateway 7 Qualification

- Produce BER-vs-SNR and frame-acceptance curves across a broader SNR sweep.
- Improve the current full-frame LDPC operating point below the qualified 3 dB profile.
- Validate external recordings and cross-team IQ interoperability.
- Add RF-front-end features only if competition scope expands to Doppler,
  carrier tracking, sub-chip timing, automatic PRN search, or AFS-Q acquisition.

## Cross-Gateway Next Steps

- Gateway 6 implementation: complete; absolute timestamp conversion remains blocked on the undefined LRT start epoch.
- End-to-end round-trip qualification: encode -> channel/noise -> decode -> parse.

## External References

- Gateway 5 design/scaffold notes: <https://github.com/KURE-x-Tech/Asteria-Knowledge-Base-G5-share>
- Note: the external knowledge bank is guidance and is not auto-integrated into this repository build.
