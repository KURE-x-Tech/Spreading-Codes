# TODOs

## Gateway 6 Remaining

1. **MP-6.5: Time of Transmission calculation**
   - Inputs are already decoded: $WN$, $ITOW$, and $TOI$.
   - The intended relative formula is:
     $$
     ToT = WN \times 604800 + ITOW \times 1200 + TOI \times 12
     $$
   - Blocker: the LSIS document leaves the LRT epoch as `{LSIS-TBD-2003}`. We can implement seconds-since-LRT-epoch now, but cannot produce a trustworthy absolute UTC/GPS-style timestamp until that epoch is specified.

2. **Complete defined message layouts**
   - SB2’s clock/ephemeris fields beyond its time header are provisional because the detailed LSIS message layouts are still unfinished.
   - SB3 currently supports a provisional orbit-almanac profile and preserves unknown message types.
   - SB4 correctly validates and labels the dynamic type/payload, but actual network-access message semantics require an LNSP SISICD.

3. **Structured navigation output**
   - `goon decode` already emits the parser-owned `subframes` JSON object.
   - After ToT exists, add it to SB1/SB2’s parsed output and Mission Console navigation display.

## Gateway 7 / Integration Remaining

1. **Multi-node / mesh networking layer**
   - Node identity and addressing
   - Destination and next-hop fields
   - Route discovery/selection and forwarding policy
   - Relay scheduling, expiry/TTL, duplicate suppression
   - Acknowledgments/retries and link-state telemetry
   - A concrete SB4 or higher-layer packet/SISICD contract

2. **Broader receiver qualification**
   - BER-versus-SNR curves beyond the qualified 3 dB profile
   - External recordings and cross-team interoperability signals
   - Multi-PRN, offset, fading, and interference scenarios

3. **Operational integration**
   - Define the source of ephemeris/almanac and network-access data.
   - Persist decoded navigation/service state for a node.
   - Connect the Mission Console’s simulated Earth-to-orbit hop to a future multi-node scenario once the packet/routing model exists.

## Recommended Next Task

Implement **MP-6.5 ToT** first. It is narrowly scoped, uses fields already available, completes the remaining concrete Gateway 6 requirement, and gives the UI a meaningful “navigation time” field. After that, the next real design decision is the mesh-layer packet contract, because that determines whether SB4 holds routing metadata or merely advertises access to a separate data service.
