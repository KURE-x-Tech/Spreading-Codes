# Current Documentation and Integration Gaps

Reviewed 2026-08-23. Gateways 1-6 are implemented in the current pipeline. These are the remaining items that affect submission wording or future scope.

## Gateway 6

1. **Absolute Time of Transmission**
   - [x] The decoded inputs are available: $WN$, $ITOW$, and $TOI$.
   - [x] The relative time calculation is defined by the repository table as:
     $$
     ToT = WN \times 604800 + ITOW \times 1200 + TOI \times 12
     $$
   - [x] Implemented in Gateway 6 as `time_of_transmission_seconds`, a relative LSIS epoch offset emitted by `goon decode` under `subframes.sb2`.
   - [x] The unresolved LRT epoch limitation is documented. Do not present a UTC/GPS-style absolute timestamp until `{LSIS-TBD-2003}` is specified.

2. **Specification-dependent message semantics**
   - [x] SB2 clock/ephemeris fields beyond the decoded time header are parsed as provisional raw CED and Time Conversions blocks where the detailed message layout is incomplete.
   - [x] SB3 supports the provisional orbit-almanac profile and preserves unknown message types as validated payloads.
   - [x] SB4 validates and labels its dynamic type and payload.
   - [x] SB4 network-access payload is extracted as raw bits and marked as requiring the LNSP SISICD for semantic decoding.

3. **Submission wording**
   - [x] `goon decode` emits the parser-owned `subframes` JSON object after full Gateway 5 acceptance.
   - [x] Describe Gateway 6 as Partial in the submission because absolute ToT and some specification-dependent field meanings remain unresolved.

## Gateway 7 / Integration

1. **Multi-node and mesh networking**
   - [ ] Node identity, addressing, routes, forwarding, relay scheduling, expiry, duplicate suppression, acknowledgments, retries, and link-state telemetry are not implemented.
   - [ ] A concrete SB4 or higher-layer packet/SISICD contract is still required.

2. **Broader receiver qualification**
   - [ ] BER-versus-SNR curves beyond the qualified 3 dB profile.
   - [ ] External recordings and cross-team interoperability signals.
   - [ ] Multi-PRN, fading, interference, fractional timing, and other channel models.

3. **Operational integration**
   - [ ] Define sources for ephemeris, almanac, and network-access data.
   - [ ] Persist decoded navigation/service state for a node.
   - [ ] Connect the Mission Console simulation to a future multi-node scenario after the packet/routing model exists.

## Submission References

- [SUBMISSION_DATA.md](SUBMISSION_DATA.md) is the current submission evidence pack.
- [ARCHITECTURE.md](ARCHITECTURE.md) is the current architecture summary.
- [Reproduce.md](Reproduce.md) contains platform-specific build, test, CLI, and decode commands.
- [GATEWAY5_DECODER.md](../G5/GATEWAY5_DECODER.md) contains receiver behavior and qualification measurements.
