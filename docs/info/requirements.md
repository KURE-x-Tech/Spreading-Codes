LSIS-AFS Reference Implementation — Requirements

## Project Overview

This document deﬁnes the requirements for the LSIS-AFS reference implementation competition. Par-
ticipants will build a complete software implementation of the LSIS-AFS signal specification, demonstrat-
ing a fully functional receiver capable of acquiring, decoding, and extracting navigation messages from
LSIS-AFS signals.

**What is LSIS-AFS?**
LSIS (Land and Satellite-based Integrated System) is a continuous positioning system developed by the European
Space Agency (ESA) for Artiﬁcial Frequency Band (AFS) satellite constellations. AFS-I and AFS-Q signals
carry navigation data using:

- Low Earth Orbit (LEO) satellites
- Spectrally efﬁcient spreading and modulation
- Time-synchronized 12-second navigation frames
- Advanced Forward Error Correction
- Interoperable with international GNSS constellations

**Your Challenge:**
Implement the complete signal processing chain to generate and decode LSIS-AFS navigation messages, validat-
ing against the speciﬁcation and demonstrating interoperability with other reference implementations.

## Gateway-Based Approach

To manage complexity and enable partial credit, the competition uses eight gateways — progressive milestones
that build upon each other:

| Gateway | Name | Function |
| --- | --- | --- |
| 0 | Design & Architecture | System design, setup, testing strategy |
| 1 | Spreading Code Generation | Generate all 210 PRN codes per speciﬁcation |
| 2 | FEC Encoding | BCH & LDPC encoders, CRC, interleaving |
| 3 | Message Framing | Build complete 12-second navigation frames |
| 4 | Signal Generation | Create I/Q baseband signals |
| 5 | Frame Sync & Decoding | Detect and decode frames from signals |
| 6 | Message Parsing | Extract navigation data from decoded frames |
| 7 | Integration & Validation | End-to-end testing, interoperability, compliance |
| 8 | Documentation | Complete setup/usage docs, examples, design docs |

## Functional Requirements

### Domain Overview

The LSIS-AFS system deﬁnes signal structures, modulation schemes, and data encoding for two signal
components:

- **AFS-I (In-phase):** 1.023 Mchip/s, narrowband transmission for wide coverage
- **AFS-Q (Quadrature):** 5.115 Mchip/s, wideband transmission for high-performance receivers

Navigation data is encoded using sophisticated Forward Error Correction (FEC) codes and organized
into 12-second frames containing clock, ephemeris, and network access information.

### Core Requirements

#### Requirement Set 1: Spreading Code Generation

| ID | Title | Description |
| --- | --- | --- |
| FR-1.1 | Gold Codes | Generate 2046-chip Gold codes for all 210 PRNs |
| FR-1.2 | Weil Primary Codes | Generate 10230-chip Weil primary codes for all 210 PRNs |
| FR-1.3 | Weil Tertiary Codes | Generate 1500-chip Weil tertiary codes for all 210 PRNs |
| FR-1.4 | Secondary Codes | Generate 4-chip secondary codes (4 variants per Table 10) |
| FR-1.5 | Tiered Code Assembly | Assemble spreading codes into coherent tiered sequences |

**Notes:**

- Codes must match Annex 3 hexadecimal reference values exactly
- 210 distinct PRN codes across 1–210
- Code generation must complete in < 1 second per PRN
- All 12 interim test codes (Table 11) must work correctly

#### Requirement Set 2: Forward Error Correction

| ID | Title | Description |
| --- | --- | --- |
| FR-2.1 | BCH(51,8) Encoder | Encode subframe 1 data with BCH(51,8) code using generator poly 763 |
| FR-2.2 | BCH(51,8) Decoder | Decode BCH(51,8) codewords via syndrome calculation |
| FR-2.3 | LDPC Encoder | Encode subframes 2–4 with rate-1/2 LDPC codes |
| FR-2.4 | LDPC Decoder | Decode LDPC codes via iterative belief propagation |
| FR-2.5 | CRC-24 Generator | Generate CRC-24 checksums using speciﬁed polynomial |
| FR-2.6 | CRC-24 Validator | Validate CRC-24 checksums on received data |
| FR-2.7 | Block Interleaver | Interleave blocks in 60×98 pattern per LSIS speciﬁcation |
| FR-2.8 | Block Deinterleaver | Reverse block interleaving pattern on received data |

**Notes:**

- All polynomials speciﬁed in LSIS Section 2.4
- LDPC matrices provided in Annex 1 (CSV format)
- BER < 10⁻⁵ required at SNR > 0 dB
- Soft-decision decoding preferred for LDPC

#### Requirement Set 3: Navigation Message Framing

| ID | Title | Description |
| --- | --- | --- |
| FR-3.1 | Sync Pattern | Generate 68-symbol synchronization pattern per Table 8 |
| FR-3.2 | Subframe 1 Builder | Assemble FID + TOI with BCH encoding |
| FR-3.3 | Subframe 2 Builder | Assemble Clock & Ephemeris data with LDPC encoding |
| FR-3.4 | Subframe 3 Builder | Assemble variable-format navigation data |
| FR-3.5 | Subframe 4 Builder | Assemble network access information |
| FR-3.6 | Frame Assembly | Combine sync pattern + 4 subframes with interleaving |
| FR-3.7 | Frame Duration | Ensure frame duration = 12 seconds = 6000 symbols |

**Notes:**

- Bit allocations speciﬁed in Tables 14, 18, 19, 20
- Total frame structure: SP(68) + SB1(52) + interleaved(SB2+SB3+SB4)(5880) = 6000 symbols
- Each symbol transmitted at 500 sym/s for 12 seconds

#### Requirement Set 4: Baseband Signal Generation

| ID | Title | Description |
| --- | --- | --- |
| FR-4.1 | AFS-I Modulator | Generate AFS-I baseband at 1.023 Mchip/s using BPSK |
| FR-4.2 | AFS-Q Modulator | Generate AFS-Q baseband at 5.115 Mchip/s using BPSK |
| FR-4.3 | Code Spreading | Apply tiered codes to symbols via chip-by-chip spreading |
| FR-4.4 | I/Q Generation | Generate complex I/Q samples at conﬁgurable sample rate |
| FR-4.5 | Signal Export | Export I/Q samples in binary or CSV format |

**Notes:**

- BPSK: Logic 1 → -1.0, Logic 0 → +1.0
- Chip rate for AFS-I: 1.023 Mchip/s (2046 samples per symbol)
- Chip rate for AFS-Q: 5.115 Mchip/s (10230 samples per symbol)
- Signal duration for 12-second frame

#### Requirement Set 5: Frame Synchronization & Decoding

| ID | Title | Description |
| --- | --- | --- |
| FR-5.1 | Frame Sync Detection | Detect sync pattern in received signal |
| FR-5.2 | Symbol Extraction | Extract symbols from I/Q samples aligned to sync pattern |
| FR-5.3 | De-spreading | De-spread symbols using known codes |
| FR-5.4 | Soft-Decision Decoding | Use log-likelihood ratios from I/Q samples for decoding |
| FR-5.5 | BCH Decoding | Decode BCH(51,8) subframe 1 |
| FR-5.6 | LDPC Decoding | Decode LDPC subframes 2–4 |
| FR-5.7 | CRC Validation | Check CRC-24 on decoded data |
| FR-5.8 | Deinterleaving | Reverse block interleaving on received data |

**Notes:**

- Frame sync detection: > 99% at SNR > 0 dB
- Symbol error rate < 1% at SNR > 0 dB
- LDPC decoder: < 50 iterations, converge on valid codewords

#### Requirement Set 6: Message Parsing

| ID | Title | Description |
| --- | --- | --- |
| FR-6.1 | Subframe 1 Parser | Extract FID (Frame ID) and TOI (Time of Transmission Index) |
| FR-6.2 | Subframe 2 Parser | Extract Clock data (WN, ITOW, signal health) and Ephemeris |
| FR-6.3 | Subframe 3 Router | Route variable-format navigation messages correctly |
| FR-6.4 | Subframe 4 Parser | Extract network access information |
| FR-6.5 | Time Calculation | Calculate Time of Transmission (ToT) from FID, TOI, WN, ITOW |
| FR-6.6 | Message Extraction | Extract all message ﬁelds and output in structured format |

**Notes:**

- All ﬁeld locations speciﬁed in LSIS Section 2.5
- Time reconstruction accurate to code phase
- Support all variable message types (Pages 1–4 in SB3/SB4)

#### Requirement Set 7: Integration & Compliance

| ID | Title | Description |
| --- | --- | --- |
| FR-7.1 | Round-Trip Testing | Encode message → generate signal → decode → verify match |
| FR-7.2 | Reference Validation | Compare codes/frames against Annex 3 references |
| FR-7.3 | Test Coverage | All major components covered by automated tests |
| FR-7.4 | Interoperability | Decode signals from other teams' implementations |
| FR-7.5 | Performance Benchmarks | Measure and report encoding/decoding times |
| FR-7.6 | Specification Compliance | Verify all "shall" requirements from LSIS doc |

**Notes:**

- BER vs SNR performance curves required
- All 12 interim test codes must work
- > 90% test code coverage

#### Requirement Set 8: Documentation

| ID | Title | Description |
| --- | --- | --- |
| FR-8.1 | Setup Instructions | Reproducible build/installation for any environment |
| FR-8.2 | API Documentation | All public functions/classes documented |
| FR-8.3 | Usage Examples | Code examples for all major features |
| FR-8.4 | Design Documentation | Architecture, design decisions, data formats |
| FR-8.5 | Test Documentation | How to run tests, interpret results |

## Non-Functional Requirements

### Performance (NFR-1)

| ID | Requirement | Target | Rationale |
| --- | --- | --- | --- |
| NFR-1.1 | Spreading code generation | < 1 sec per PRN | Real-time capable |
| NFR-1.2 | Frame encoding | < 100 ms per frame | Support batch operations |
| NFR-1.3 | Frame decoding | < 1 sec per frame | Faster than real-time |
| NFR-1.4 | Real-time operation | Faster than signal duration | Practical deployment |

### Accuracy & Reliability (NFR-2)

| ID | Requirement | Target | Rationale |
| --- | --- | --- | --- |
| NFR-2.1 | Spreading code correctness | 100% match Annex3 | Interoperability |
| NFR-2.2 | Bit error rate | < 10⁻⁵ at SNR > 0 dB | Spec compliance |
| NFR-2.3 | Frame sync reliability | > 99% at SNR > 0 dB | Acquisition robustness |
| NFR-2.4 | Time reconstruction | ±1 code phase | Positioning accuracy |

### Quality & Testing (NFR-3)

| ID | Requirement | Target | Rationale |
| --- | --- | --- | --- |
| NFR-3.1 | Unit test coverage | > 90% of modules | Code quality |
| NFR-3.2 | Integration tests | End-to-end pipeline | System validation |
| NFR-3.3 | Test reproducibility | Deterministic results | Reliability |
| NFR-3.4 | Known test vectors | Reference codes from Annex3 | Correctness veriﬁcation |

### Usability & Maintainability (NFR-4, NFR-5)

| ID | Requirement | Target | Rationale |
| --- | --- | --- | --- |
| NFR-4.1 | API documentation | All public APIs | User clarity |
| NFR-4.2 | Working examples | Every major feature | Ease of adoption |
| NFR-4.3 | Error messages | Actionable guidance | Debugging support |
| NFR-5.1 | Modular architecture | Clean boundaries | Maintainability |
| NFR-5.2 | Code style | Consistent & enforced | Team collaboration |
| NFR-5.3 | Version control | Meaningful history | Project tracking |

## Data Formats & Specifications

### Input Data

1. **LDPC Matrices** (Annex 1):
   - CSV-formatted parity-check matrices
   - Rate 1/2 codes for subframes 2, 3, 4
   - Available from LSIS document

2. **Reference Codes** (Annex 3):
   - Hexadecimal spreading codes for all 210 PRNs
   - 12 interim test codes (Table 11) for validation
   - Available from LSIS document

3. **Navigation Data**:
   - 8-bit symbols to be encoded into 12-second frames
   - Clock, ephemeris, system messages
   - Speciﬁc bit allocations per Table 14, 18–20

### Output Data

1. **I/Q Signal Files**:
   - Binary (int16/ﬂoat32) or CSV format
   - Complex samples (I + jQ)
   - Ready for RF simulation or receiver testing

2. **Decoded Navigation Data**:
   - FID, TOI, WN, ITOW
   - Clock and ephemeris parameters
   - Network messages
   - Structured format (JSON/CSV/binary)

## Technical Constraints

### TC-1: Language & Implementation

- **Language:** Flexible (Python, Rust, C++, Go, Java, etc.)
- **Libraries:** Open source preferred; avoid proprietary NFR-heavy dependencies
- **Core algorithms:** Standard libraries or textbook implementations
- **Test framework:** Built-in or minimal external dependency

### TC-2: Dependencies

- **LDPC matrices:** Provided in Annex 1 as CSV
- **Reference codes:** Provided in Annex 3 as hex
- **LSIS specification:** Reference document (LSIS-vol.A)
- **No RF hardware:** All simulation-based

### TC-3: Compatibility & Deployment

- **Cross-platform:** Windows, Linux, macOS
- **No external services:** Standalone operation
- **Reproducible:** Same results on any machine with same inputs
- **Version control:** Git with clear history

### TC-4: Data Privacy & IP

- **Source code:** Published and open for interoperability testing
- **Algorithms:** Standard, implementer's choice of design patterns
- **No proprietary formats:** Use standard file formats (binary, CSV, JSON)

## Scoring Rubric

### Correctness (40 points)

| Criterion | Points | Evaluation |
| --- | --- | --- |
| Spreading codes match Annex3 | 10 | All 210 PRNs correct |
| Frame encoding correct | 10 | Subframes structure, bit allocations, symbol counts |
| Signal generation correct | 10 | Modulation, chip rates, timing |
| Frame decoding & parsing | 10 | Extract all fields correctly, > 99% sync, BER < 10⁻⁵ |

### Performance (20 points)

| Criterion | Points | Evaluation |
| --- | --- | --- |
| Code generation speed | 5 | < 1 sec per PRN |
| Encoding speed | 5 | < 100 ms per frame |
| Decoding speed | 5 | < 1 sec per frame |
| Real-time operation | 5 | Faster than signal duration; practical throughput |

### Completeness (20 points)

| Criterion | Points | Evaluation |
| --- | --- | --- |
| All 8 gateways implemented | 10 | Evidence of all gateways working |
| End-to-end functionality | 5 | Round-trip encode → decode with 100% recovery |
| Interoperability testing | 5 | Decode signals from other teams or reference |

### Code Quality (10 points)

| Criterion | Points | Evaluation |
| --- | --- | --- |
| Modular architecture | 5 | Clean separation; easy to test/extend |
| Test coverage | 3 | > 90% of major components tested |
| Code style & documentation | 2 | Consistent, readable, commented |

### Innovation & Extras (10 points)

| Criterion | Points | Notes |
| --- | --- | --- |
| Optimization insights | 3 | Novel algorithms, performance tricks |
| Testing framework | 3 | Automated CI/CD, visualization tools |
| Design documentation | 2 | Architecture diagrams, design rationale |
| Bonus features | 2 | Advanced error correction, multi-threaded decoding, etc. |

## Submission Checklist

### Code & Deliverables

- [ ] Complete source code in Git repository
- [ ] Build/setup instructions (README)
- [ ] All 8 gateways implemented
- [ ] Test suite with clear pass/fail results
- [ ] API documentation
- [ ] Usage examples for all major features

### Validation & Compliance

- [ ] Spreading codes compared to Annex3 (all 210 PRNs)
- [ ] All 12 interim test codes (Table 11) working
- [ ] BER < 10⁻⁵ at SNR > 0 dB
- [ ] Frame sync > 99% at SNR > 0 dB
- [ ] Round-trip test (encode → decode) with 100% recovery
- [ ] Interoperability test with another implementation
- [ ] Performance benchmarks reported

### Documentation

- [ ] Setup & build instructions (reproducible)
- [ ] All APIs documented
- [ ] Usage examples working
- [ ] Design documentation included
- [ ] Test suite documentation

## Timeline & Submission

- **Registration closes:** 5 April 2026
- **Workshop:** Late June / Early July 2026 (Goonhilly, Cornwall)
- **Final submission:** 31 August 2026
- **No intermediate deadlines** — all work due at final submission

**Flexibility:** Use the 8 gateways as internal milestones to track progress. Focus on correctness first, then performance, then polish.

## Support & Resources

- **LSIS Specification:** LSIS-vol.A (official document)
- **Reference Data:** Annex 1 (LDPC matrices), Annex 3 (spreading codes)
- **Test Data:** 12 interim test codes in Table 11
- **Discussion Forum:** Available for Q&A
- **Workshop:** Hands-on technical session at Goonhilly (late June/early July)

## Next Steps

1. **Understand the specification** — Read LSIS-vol.A in detail
2. **Design your architecture** — Modular components, clear interfaces
3. **Implement gateways progressively** — 0 → 1 → ... → 8
4. **Test continuously** — Use Annex 3 references and test codes
5. **Document as you go** — Setup, APIs, design decisions
6. **Prepare for interoperability** — Share interfaces with other teams
7. **Submit complete package** — Code + docs + validation by 31 August 2026
