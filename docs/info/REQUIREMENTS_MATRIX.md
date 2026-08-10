# LSIS-AFS Competition — Requirements Traceability Matrix

This document maps every testable requirement to its LSIS specification source, acceptance criteria, and scoring weight. Use it as a checklist to track your progress and understand exactly what will be measured during evaluation.

## How to Read This Matrix

| Column | Meaning |
| -------- | --------- |
| ID | Unique requirement identifier |
| LSIS Ref | Section/table in LSIS-AFS Volume A |
| Requirement | What you must implement |
| Acceptance Criteria | How we verify it passes |
| Gateway | Which development milestone it belongs to |
| Points | Scoring weight (out of 100 total) |

## Minimum Viable Submission (Pass/Fail Gate)

Your submission **must** include ALL of the following or it receives 0 points:

1. Generate at least 12 PRN spreading codes correctly (Table 11)
2. Encode a complete 12-second navigation frame
3. Generate I/Q signal files
4. Pass validation tests for implemented features
5. Include setup/build instructions

---

## Requirement Traceability Matrix

### Section 1: Spreading Code Generation (Gateway 1)

| ID | LSIS Ref | Requirement | Acceptance Criteria | Gateway | Points |
| ---- | ---------- | ------------- | ------------------- | --------- | -------- |
| SC-1.1 | Sec 2.3.5, Table 9 | Generate Gold codes (2046 chips) for all 210 PRNs | All codes match Annex 3 hex references exactly | 1 | 5 |
| SC-1.2 | Sec 2.3.5, Table 9 | Generate Weil primary codes (10230 chips) for all 210 PRNs | All codes match Annex 3 references exactly | 1 | 5 |
| SC-1.3 | Sec 2.3.5, Table 9 | Generate Weil tertiary codes (1500 chips) for all 210 PRNs | All codes match Annex 3 references exactly | 1 | 5 |
| SC-1.4 | Sec 2.3.5, Table 10 | Generate secondary codes (4 chips, 4 variants per PRN) | Secondary codes match Table 10 specifications | 1 | 5 |
| SC-1.5 | Sec 2.3.5 | Assemble tiered codes (primary + secondary + tertiary) with coherent generation | Tiered sequences verified against test vectors | 1 | 5 |
| SC-1.6 | Table 11 | All 12 interim test codes generate correctly | All test codes in Table 11 verified | 1 | 5 |
| SC-1.7 | Sec 2.3.5 | Code generation performance | Generation completes in < 1 second per PRN | 1 | 2 |

**Subtotal: 32 points**

### Section 2: Forward Error Correction & Message Framing (Gateways 2 & 3)

#### FEC Codes (Gateway 2)

| ID | LSIS Ref | Requirement | Acceptance Criteria | Gateway | Points |
| ---- | ---------- | ------------- | ------------------- | --------- | -------- |
| FEC-2.1 | Sec 2.4, Table 12 | BCH(51,8) encoder with generator polynomial 763 | Encodes 51-bit input to 52-symbol codewords; syndrome calculation verified | 2 | 3 |
| FEC-2.2 | Sec 2.4, Table 12 | BCH(51,8) decoder via syndrome & Chien search | Decodes BCH codewords with t=2 error correction | 2 | 3 |
| FEC-2.3 | Sec 2.4, Annex 1 | LDPC encoder (rate 1/2) for subframes 2, 3, 4 | Generates valid codewords; puncturing pattern correct | 2 | 3 |
| FEC-2.4 | Sec 2.4, Annex 1 | LDPC decoder via belief propagation | Decodes LDPC codewords; converges in < 50 iterations | 2 | 3 |
| FEC-2.5 | Sec 2.4 | CRC-24 generator with polynomial from spec | CRC computation matches specification | 2 | 2 |
| FEC-2.6 | Sec 2.4 | CRC-24 validator on received data | Detects corrupted data reliably | 2 | 2 |
| FEC-2.7 | Sec 2.4, Table 13 | Block interleaver (60×98 pattern) | Interleave/deinterleave pattern verified | 2 | 2 |
| FEC-2.8 | Sec 2.4 | BER performance at SNR > 0 dB | BER < 10⁻⁵ at target SNR | 2 | 3 |
| FEC-2.9 | Sec 2.4 | Encoding/decoding performance | Encoding < 100ms; decoding < 1 second per frame | 2 | 2 |

**Subtotal: 25 points**

#### Message Framing (Gateway 3)

| ID | LSIS Ref | Requirement | Acceptance Criteria | Gateway | Points |
| ---- | ---------- | ------------- | ------------------- | --------- | -------- |
| MF-3.1 | Table 8 | Synchronization pattern (68 symbols) | Sync pattern matches specification; 68 symbols verified | 3 | 2 |
| MF-3.2 | Table 14 | Subframe 1 builder (FID + TOI, BCH encoded) | Frame ID and Time Of Index encoded with BCH | 3 | 2 |
| MF-3.3 | Table 18 | Subframe 2 builder (Clock & Ephemeris, LDPC encoded) | Clock data and ephemeris bits allocated correctly | 3 | 2 |
| MF-3.4 | Table 19 | Subframe 3 builder (Variable navigation data, LDPC encoded) | Variable-format data routed and encoded correctly | 3 | 2 |
| MF-3.5 | Table 20 | Subframe 4 builder (Network access, LDPC encoded) | Network information encoded per specification | 3 | 2 |
| MF-3.6 | Figure 9 | Frame assembly (sync + 4 subframes + interleaving) | Frame structure: SP(68) + SB1(52) + IL(5880) = 6000 symbols | 3 | 2 |
| MF-3.7 | Sec 2.3 | Frame duration (12 seconds) | Symbols transmitted at 500 sym/s for exactly 12 seconds | 3 | 2 |
| MF-3.8 | Tables 14, 18-20 | Bit allocations match specification | All bit assignments per tables verified | 3 | 1 |

**Subtotal: 15 points**

### Section 3: Baseband Signal Generation (Gateway 4)

| ID | LSIS Ref | Requirement | Acceptance Criteria | Gateway | Points |
| ---- | ---------- | ------------- | ------------------- | --------- | -------- |
| SG-4.1 | Sec 2.3, Table 7 | AFS-I modulator at 1.023 Mchip/s | Chip rate verified; BPSK modulation correct | 4 | 3 |
| SG-4.2 | Sec 2.3, Table 7 | AFS-Q modulator at 5.115 Mchip/s | Chip rate verified; BPSK modulation correct | 4 | 3 |
| SG-4.3 | Sec 2.3 | BPSK modulation (1 → -1.0, 0 → +1.0) | I/Q sample values verified | 4 | 2 |
| SG-4.4 | Sec 2.3 | Tiered code spreading | Symbols spread by tiered code sequences correctly | 4 | 2 |
| SG-4.5 | Sec 2.3 | I/Q sample generation at configurable rate | Complex I/Q output ready for RF simulation | 4 | 2 |
| SG-4.6 | Sec 2.3 | Signal file export (binary, CSV) | Files exported in standard formats | 4 | 2 |
| SG-4.7 | Sec 2.3 | Signal duration (12 seconds) | Signal length matches frame duration | 4 | 1 |

**Subtotal: 15 points**

### Section 4: Frame Synchronization & Decoding (Gateway 5)

Current repository coverage note (2026-08-10):

- Implemented: raw/standard I/Q import, AFS-I de-spreading, normalized noisy-stream synchronization, frame extraction, soft BCH/LDPC decode, soft deinterleaving, CRC-gated acceptance, CRC stripping, `goon decode`, and the Gateway 6 payload handoff.
- Qualification: 9960/10000 sync detections at 0.1 dB with a 99.4819% one-sided 95% lower bound; 16/10000 false alarms with a 0.2406% upper bound; 0/1000 de-spread symbol errors at 0.1 dB; empirical 0/299,880 post-LDPC bit errors at 3 dB with 102/102 CRC-accepted frames; maximum 10 LDPC iterations and worst measured three-subframe decode of 70.8 ms.
- Operating envelope: known PRN, AFS-I, integer chip timing, integer-multiple sample rate, and AWGN soft decisions. Full-frame operation below the qualified 3 dB point and external IQ interoperability remain Gateway 7 qualification work.
- Usage and implementation details: `docs/G5/GATEWAY5_DECODER.md`.

| ID | LSIS Ref | Requirement | Acceptance Criteria | Gateway | Points |
| ---- | ---------- | ------------- | ------------------- | --------- | -------- |
| FSD-5.1 | Table 8, Sec 2.3 | Frame sync detection via sync pattern | Detects sync pattern > 99% at SNR > 0 dB | 5 | 3 |
| FSD-5.2 | Sec 2.3 | Symbol extraction from I/Q samples | Symbols aligned to detected sync pattern | 5 | 2 |
| FSD-5.3 | Sec 2.3 | De-spreading using known codes | Symbols de-spread correctly using PRN codes | 5 | 2 |
| FSD-5.4 | Sec 2.3 | Soft-decision decoding (LLR from I/Q) | Log-likelihood ratios computed from samples | 5 | 2 |
| FSD-5.5 | Sec 2.4, Table 12 | BCH decoding for Subframe 1 | BCH codewords decoded; errors corrected | 5 | 2 |
| FSD-5.6 | Sec 2.4, Annex 1 | LDPC decoding for Subframes 2–4 | LDPC codewords decoded; converges correctly | 5 | 3 |
| FSD-5.7 | Sec 2.4 | CRC-24 validation on decoded data | CRC errors detected; invalid frames rejected | 5 | 2 |
| FSD-5.8 | Sec 2.4, Table 13 | Block deinterleaving | Interleaving reversed; data recovered | 5 | 1 |
| FSD-5.9 | Sec 2.3 | Decoding performance | Decoding completes in < 1 second per frame | 5 | 1 |

**Subtotal: 18 points**

### Section 5: Message Parsing (Gateway 6)

| ID | LSIS Ref | Requirement | Acceptance Criteria | Gateway | Points |
| ---- | ---------- | ------------- | ------------------- | --------- | -------- |
| MP-6.1 | Sec 2.5, Table 14 | Subframe 1 parser (FID + TOI extraction) | Frame ID and TOI extracted correctly | 6 | 2 |
| MP-6.2 | Sec 2.5, Table 18 | Subframe 2 parser (Clock & Ephemeris) | WN, ITOW, signal health extracted | 6 | 2 |
| MP-6.3 | Sec 2.5, Table 19 | Subframe 3 parser (Variable data routing) | Data routed to correct message types | 6 | 2 |
| MP-6.4 | Sec 2.5, Table 20 | Subframe 4 parser (Network access) | Network information extracted correctly | 6 | 2 |
| MP-6.5 | Sec 2.5 | Time of Transmission calculation | ToT computed from FID, TOI, WN, ITOW | 6 | 2 |
| MP-6.6 | Sec 2.5 | Message field extraction & formatting | All fields output in structured format (JSON/CSV) | 6 | 1 |

**Subtotal: 11 points**

### Section 6: Integration & Validation (Gateway 7)

| ID | LSIS Ref | Requirement | Acceptance Criteria | Gateway | Points |
| ---- | ---------- | ------------- | ------------------- | --------- | -------- |
| IV-7.1 | Sec 1-5 | Round-trip testing (encode → signal → decode → verify) | Original data recovered with 100% accuracy | 7 | 4 |
| IV-7.2 | Annex 3 | Validation against reference codes (all 210 PRNs) | Comparison to hex references shows 100% match | 7 | 3 |
| IV-7.3 | Table 11 | Test coverage (12 interim codes) | All test codes from Table 11 working | 7 | 2 |
| IV-7.4 | Sec 1-5 | BER vs SNR performance curves | Performance data collected and reported | 7 | 2 |
| IV-7.5 | All sections | LSIS compliance verification | All "shall" requirements from spec verified | 7 | 2 |
| IV-7.6 | Sec 1-5 | Interoperability testing | Decode signals from another implementation | 7 | 2 |

**Subtotal: 15 points**

### Section 7: Documentation & Examples (Gateway 8)

| ID | LSIS Ref | Requirement | Acceptance Criteria | Gateway | Points |
| ---- | ---------- | ------------- | ------------------- | --------- | -------- |
| DOC-8.1 | — | Setup & build instructions | Any developer can build from scratch | 8 | 2 |
| DOC-8.2 | — | API documentation | All public functions/classes documented | 8 | 2 |
| DOC-8.3 | — | Usage examples | Examples demonstrate key features working | 8 | 2 |
| DOC-8.4 | — | Design documentation | Architecture, data formats, design decisions | 8 | 2 |
| DOC-8.5 | — | Test documentation | How to run tests; interpretation guide | 8 | 1 |

**Subtotal: 9 points**

### Section 8: Code Quality & Testing (All Gateways)

| ID | LSIS Ref | Requirement | Acceptance Criteria | Gateway | Points |
| ---- | ---------- | ------------- | ------------------- | --------- | -------- |
| QA-9.1 | — | Modular architecture | Clear separation of concerns; easy to test/extend | All | 2 |
| QA-9.2 | — | Unit test coverage | > 90% of major components tested | All | 2 |
| QA-9.3 | — | Code style & documentation | Consistent formatting; comments where needed | All | 1 |

**Subtotal: 5 points**

---

## Scoring Summary

| Section | Category | Points |
| --------- | ---------- | -------- |
| 1 | Spreading Code Generation | 32 |
| 2 | FEC & Message Framing | 40 |
| 3 | Baseband Signal Generation | 15 |
| 4 | Frame Sync & Decoding | 18 |
| 5 | Message Parsing | 11 |
| 6 | Integration & Validation | 15 |
| 7 | Documentation & Examples | 9 |
| 8 | Code Quality & Testing | 5 |
| **TOTAL** | | **145 points** |

*Note: Scoring rubric cap is 100 points. Top-scoring implementations receive up to 100 points based on relative performance in these categories. Bonus points available for innovation and extras.*

---

## Gateways at a Glance

| Gateway | Name | Key Requirements | Points | Min Viable? |
| --------- | ------ | ------------------ | -------- | ------------ |
| 0 | Design & Architecture | Architecture document, testing strategy | — | ✓ (implicit) |
| 1 | Spreading Codes | 210 PRN codes, Table 11 test codes | 32 | ✓ |
| 2 | FEC Encoding | BCH, LDPC, CRC, interleaver | 15 | ✓ |
| 3 | Message Framing | 12-second frame assembly | 25 | ✓ |
| 4 | Signal Generation | I/Q baseband at correct chip rates | 15 | ✓ |
| 5 | Frame Sync & Decoding | Detect sync, decode subframes | 18 | ✓ |
| 6 | Message Parsing | Extract FID, TOI, clock, ephemeris | 11 | — |
| 7 | Integration & Validation | Round-trip testing, interop | 15 | — |
| 8 | Documentation | API docs, examples, setup | 9 | ✓ |

---

## Acceptance Criteria Summary

### **Correctness** (40 points)

- Spreading codes: 100% match Annex 3 references (32 points from SC)
- Frame encoding: Correct structure, bit allocations (25 points from MF + FEC)
- Signal generation: Correct modulation and chip rates (15 points from SG)
- Decoding & parsing: > 99% frame sync, BER < 10⁻⁵ (29 points from FSD + MP)

### **Performance** (20 points)

- Code generation: < 1 sec per PRN
- Encoding: < 100ms per frame
- Decoding: < 1 sec per frame
- Real-time: Faster than signal duration (12 seconds)

### **Completeness** (20 points)

- All 8 gateways: Evidence of implementation
- End-to-end: Round-trip encode → decode with 100% recovery
- Interoperability: Decode signals from other teams

### **Code Quality** (10 points)

- Modular architecture: Clean boundaries
- Test coverage: > 90% of modules
- Documentation: Readable, commented code

### **Innovation & Extras** (10 points)

- Algorithm optimizations
- Testing tools & visualizations
- Advanced features (multi-threading, GPU, etc.)

---

## Checklist for Competition Success

Use this checklist to track your progress:

### Gateways 1–3 (Foundation & Encoding)

- [ ] Spreading codes generated and validated (Gateway 1)
- [ ] BCH & LDPC encoders working (Gateway 2)
- [ ] Complete 12-second frame assembly (Gateway 3)
- [ ] All 12 Table 11 test codes pass

### Gateways 4–6 (Signal & Decoding)

- [ ] I/Q baseband signals generated (Gateway 4)
- [ ] Frame sync > 99% at SNR > 0 dB (Gateway 5)
- [ ] Subframes decoded and parsed (Gateway 6)
- [ ] BER < 10⁻⁵ at SNR > 0 dB

### Gateways 7–8 (Integration & Delivery)

- [ ] Round-trip testing: 100% data recovery (Gateway 7)
- [ ] Interoperability with another implementation
- [ ] All documentation complete (Gateway 8)
- [ ] Setup instructions reproducible
- [ ] Performance benchmarks reported

### Quality & Compliance

- [ ] > 90% test code coverage
- [ ] All "shall" requirements verified
- [ ] Codes match Annex 3 (all 210 PRNs)
- [ ] Git history clean and meaningful

---

## Notes & Tips

1. **Start with the specs:** Read LSIS-vol.A completely before designing.
2. **Test early & often:** Use Table 11 test codes and Annex 3 references continuously.
3. **Modular design:** Implement and test each gateway independently.
4. **Documentation:** Write it as you go — easier to maintain and complete.
5. **Performance:** Profile and optimize the critical paths (code gen, decoding).
6. **Interoperability:** Share your frame format and test vectors with other teams early.
