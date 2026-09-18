# Epsilon Information Symphony (EIS)
## Staggered-Guard Tone-Word Multicarrier Audio Codec — Formal Replication Specification v0.2

**Date:** 2026-09-17  
**Project:** Epsilonic / Epsilon Information Symphony (EIS)  
**Status:** Experimental, falsifiable engineering specification  
**Primary objective:** Determine the highest byte-exact source payload rate that can be carried by a compact audio file using multiple simultaneous machine-designed instrument lanes whose internal tones are attached into "tone-words" and whose word-boundary silence guards are intentionally staggered.

---

## 1. Project Name

**Formal project name:** Epsilon Information Symphony (EIS)  
**Technical name:** Staggered-Guard Tone-Word Multicarrier Audio Codec  
**Short description:** A machine-readable symphony of pure information.

---

## 2. Core Hypothesis

Arbitrary source data can be represented as a dense synthetic audio performance in which:

1. several independent spectral/instrument lanes transmit simultaneously;
2. each lane has a small legal alphabet of machine-readable tone states;
3. several tone states are attached directly to one another to form a **tone-word**;
4. silence occurs primarily at tone-word boundaries rather than after every tone;
5. each lane's word-boundary silence is phase-shifted relative to the others;
6. the receiver already knows the legal tone alphabet, lane layout, timing rules, and word-boundary schedule;
7. the receiver reconstructs the exact original bytes.

The experiment succeeds only when the reconstructed byte sequence is exactly identical to the source.

---

## 3. Why Tone-Words Replace Per-Tone Silence

The v0.1 design placed a silence guard around every lane symbol. That is useful for synchronization but wastes a large fraction of available time.

The v0.2 design treats continuous tone changes more like machine syllables inside a word:

```
old:
tone | gap | tone | gap | tone | gap

v0.2:
tone-tone-tone-tone-tone | gap | tone-tone-tone-tone-tone | gap
```

The tones inside a word remain individually decodable because:

- the receiver knows the exact sub-symbol duration;
- each lane has a fixed legal set of tone states;
- the transitions occur on a known sub-symbol clock;
- phase-continuous or smoothly shaped transitions can be used to limit spectral splatter.

The silence is therefore used as a **word boundary and resynchronization marker**, not as a delimiter for every character/state.

If a tone-word contains **K** sub-symbols, guard overhead is paid once per K symbols rather than once per symbol.

---

## 4. Staggered Word Boundaries

The key EIS mechanism remains that silence guards do not line up across lanes.

For each lane:

```
Lane 0: wordwordword |gap| wordwordword |gap| ...
Lane 1:   wordwordword |gap| wordwordword |gap| ...
Lane 2:      wordwordword |gap| wordwordword |gap| ...
Lane 3:         wordwordword |gap| wordwordword |gap| ...
```

At any moment, one lane may be inside its guard while the other lanes continue carrying payload.

The aggregate waveform therefore stays information-dense even though each individual lane periodically receives a clean reset/resynchronization interval.

---

## 5. Test Corpus

Canonical **enwik9**:

- uncompressed size: **1,000,000,000 bytes**
- SHA-256: `159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc`
- MD5: `e206c3450ac99950df65bf70ef61a12d`
- canonical download used by current workflows: `https://mattmahoney.net/dc/enwik9.zip`

Before any benchmark run, verify both expected size and SHA-256.

---

## 6. Hard Size Target

### 6.1 Carrier-only target

The engineering target is:

**audio carrier < 50,000,000 bytes for canonical 1,000,000,000-byte enwik9, with byte-exact reconstruction.**

This is a carrier ratio below 5%, or greater than 20:1 source-to-carrier reduction.

### 6.2 Complete-package target

A formal compression result must separately account for all reconstruction requirements:

- EIS decoder;
- source-specific model or mapping data, if any;
- dictionaries or side information, if any;
- carrier codec dependencies when benchmark rules require them;
- final audio carrier.

Report both:

1. **audio carrier bytes**
2. **complete reconstruction package bytes**

A carrier-only experiment must never be presented as a complete compression score.

---

## 7. Density Requirement

For a carrier encoded at bitrate **B** and an exact source payload rate **R**:

`carrier/source ≈ B / R`

To reach a 5% carrier ratio:

`R >= 20 × B`

Examples:

| Carrier bitrate | Required exact source payload rate for <50 MB |
|---:|---:|
| 6 kbps | 120 kbps |
| 12 kbps | 240 kbps |
| 24 kbps | 480 kbps |
| 32 kbps | 640 kbps |
| 64 kbps | 1.28 Mbps |
| 96 kbps | 1.92 Mbps |
| 128 kbps | 2.56 Mbps |

These are required ratios, not predicted achievements.

---

## 8. v0.2 Signal Architecture

### 8.1 Parallel lanes

Start with the existing v1 baseline:

- sample rate: 48 kHz;
- 8 independent spectral lanes;
- 4 legal tone states per lane;
- 2 bits per tone state;
- mono aggregate waveform.

Existing center frequencies:

`[900, 2200, 3500, 4800, 6100, 7400, 8700, 10000] Hz`

Existing state offsets:

`[-240, -80, +80, +240] Hz`

These values are only the initial replication baseline and may be optimized.

### 8.2 Tone-word parameters

A tone-word is defined by:

- **sub-symbol duration S**
- **sub-symbols per word K**
- **guard duration G**
- **lane phase P**
- **state alphabet size M**

For 4 states, every sub-symbol carries 2 bits.

Per-lane payload per word:

`K × log2(M)`

Aggregate payload per word cycle for L lanes:

`L × K × log2(M)`

Approximate aggregate source payload rate when all lanes use the same K, S, and G:

`R ≈ L × K × log2(M) / (K × S + G)`

The crucial difference from v0.1 is that **G is paid once per word, not once per tone**.

### 8.3 Example only

For illustration, not as a claimed working configuration:

- L = 8 lanes
- M = 4 states
- K = 8 tones per word
- S = 0.25 ms per tone
- G = 0.25 ms guard

Then:

- each lane carries 16 bits per word;
- all lanes carry 128 bits per word cycle;
- a word lasts 2.25 ms including guard;
- theoretical signaling payload is about 56.9 kbps before codec losses or synchronization overhead.

This example exists only to show the scaling effect of tone-words. It is not accepted as a result until exact audio round-trip testing passes.

---

## 9. Attached-Tone Generation

Within a tone-word, adjacent tones should be connected with no full silence.

Candidate transition modes:

1. **hard frequency switch on the known sub-symbol clock**;
2. **short crossfade between adjacent tone states**;
3. **phase-continuous FSK-style transition**;
4. **minimum-phase/smooth transition chosen to minimize codec smearing**.

The first replication implementation should test at least hard-switch and phase-continuous modes.

The goal is to preserve sub-symbol identity while reducing the spectral cost of repeatedly starting and stopping every tone.

---

## 10. Word-Boundary Silence

A word boundary is a short local silence or near-silence on one lane.

The reader uses it for:

- re-establishing lane phase;
- correcting accumulated timing drift;
- reducing state carryover into the next word;
- measuring local noise floor;
- detecting missing or malformed words.

The guard is not intended to carry source data in the baseline design.

Later experiments may test whether guard timing itself can safely carry information, but that is outside v0.2 replication.

---

## 11. Independent Word Schedules

The strongest version need not force every lane to use identical word lengths.

A later test may assign different K values by lane, for example:

- lane 0: 7 tones per word;
- lane 1: 8;
- lane 2: 9;
- lane 3: 10;
- etc.

This causes word boundaries to naturally walk through one another rather than periodically aligning.

However, the first v0.2 implementation should keep K equal across lanes and use fixed phase offsets. This isolates the benefit of tone-words before introducing asynchronous word lengths.

---

## 12. Encoder

For each source block:

1. split source bytes into state-sized bit groups;
2. distribute groups across lanes;
3. group K consecutive lane states into one tone-word;
4. synthesize each lane's tone-word using the known sub-symbol clock;
5. append the lane's word-boundary silence;
6. offset each lane's word schedule by its assigned phase;
7. sum all lanes into the aggregate PCM waveform;
8. encode PCM using the selected carrier codec.

No language prediction is required.

No enwik-specific dictionary is required by the v0.2 signaling layer.

---

## 13. Reader

1. decode the carrier into PCM;
2. locate the global startup preamble;
3. initialize each lane clock;
4. for each lane:
   - identify its scheduled word region;
   - identify the local word-boundary guard;
   - use the guard to correct timing drift;
   - divide the preceding/following word into K known sub-symbol windows;
   - matched-filter each sub-symbol against the legal states for that lane;
   - recover K state values;
5. interleave lane states back into their original bit ordering;
6. reconstruct bytes;
7. compare against the exact source.

The reader must not use source semantics to repair errors in the baseline experiment.

---

## 14. Synchronization

Keep the existing startup preamble for initial acquisition.

Current v1 preamble:

- 430 Hz for 250 ms;
- 50 ms gap;
- 860 Hz for 250 ms;
- 50 ms gap.

After startup, per-lane word guards provide recurring local resynchronization.

A mature EIS stream should therefore not require a large repeated global synchronization signal.

---

## 15. Exactness Gate

Pass only when:

- recovered byte count equals source byte count;
- every recovered byte matches;
- recovered SHA-256 equals source SHA-256.

**One incorrect byte = failure.**

Low bit-error rate does not count as lossless success.

---

## 16. v0.2 Experimental Matrix

The first tone-word sweep should vary:

### Tone-word length K
- 2
- 4
- 8
- 16

### Sub-symbol duration S
- 0.125 ms
- 0.25 ms
- 0.5 ms
- 1.0 ms

### Guard duration G
- 0.125 ms
- 0.25 ms
- 0.5 ms
- 1.0 ms

### Transition mode
- direct/hard switch
- phase-continuous

### Carrier
- MP3 baseline
- Opus when the same exact PCM waveform and decoder can be used reproducibly

### Carrier bitrate
Choose a codec-appropriate sweep; do not compare nominal bitrate alone. Compare **carrier bytes at zero error**.

---

## 17. Promotion Ladder

Only zero-error configurations are promoted.

1. deterministic synthetic/random test vector
2. 4 KiB enwik9 prefix
3. 1 MiB
4. 10 MiB
5. 100 MiB
6. full canonical 1 GB enwik9

A candidate that fails at a larger stage returns to engineering status.

---

## 18. Optimization Objective

Primary EIS objective:

> Maximize exact source payload rate per encoded carrier byte.

Operational ranking among zero-error candidates:

1. smallest complete carrier bytes;
2. smallest complete reconstruction package;
3. higher timing/detection margin;
4. lower computational cost, when size is equal.

Never rank a configuration that is not byte-exact as a lossless winner.

---

## 19. Measurements to Record

For every run:

- corpus identity and hash;
- source bytes;
- sample rate;
- carrier codec and bitrate;
- carrier file bytes;
- lane count L;
- states per lane M;
- bits per state;
- sub-symbol duration S;
- tones per word K;
- guard duration G;
- phase offsets;
- transition mode;
- nominal source payload rate;
- effective source payload rate including startup preamble;
- detected word-boundary timing drift;
- minimum matched-filter confidence margin;
- mean margin;
- byte errors;
- exact PASS/FAIL;
- recovered hash;
- extrapolated full-enwik9 carrier size, explicitly labeled extrapolation;
- complete decoder/package bytes where applicable.

---

## 20. Existing Implementation Baseline

The earlier per-symbol-guard implementation remains the baseline:

- source: `audio_lab/staggered_multicarrier_v1.py`
- branch: `hutter-target-apc-bot-20260917`
- implementation commit: `e0956c66fa2fe4b52836904e9869ffb683dab7a8`
- workflow: `.github/workflows/staggered-multicarrier-sweep.yml`
- corrected workflow commit: `424062e62620d1bbfbac59222c5c4c9ab38f204b`
- corrected workflow run: `35311520767`

At the time v0.2 was written, that run remained queued. No benchmark result from it is claimed here.

v0.2 is the next architecture to implement after preserving the v1 baseline.

---

## 21. Replication Commands for v1 Baseline

```bash
git clone https://github.com/MungSauce/Dynamic-Field-Theory.git
cd Dynamic-Field-Theory
git checkout hutter-target-apc-bot-20260917

curl -L --fail --retry 5 --retry-delay 5 \
  https://mattmahoney.net/dc/enwik9.zip -o enwik9.zip
unzip enwik9.zip

test "$(stat -c%s enwik9)" = "1000000000"
echo "159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc  enwik9" | sha256sum -c -

python3 audio_lab/staggered_multicarrier_v1.py enwik9 \
  --prefix 4096 \
  --sr 48000 \
  --period-ms 4 \
  --guard-ms 0.5 \
  --bitrate 128 \
  --out-prefix eis_v1_probe
```

A pass must report:

```json
"exact": true,
"errors": 0
```

---

## 22. Falsification Conditions

A proposed density point fails if:

- one or more source bytes are wrong;
- word-boundary drift cannot be corrected;
- adjacent attached tones become indistinguishable after the carrier codec;
- spectral interference between lanes causes ambiguity;
- the carrier bitrate required for exactness grows faster than the source payload rate;
- source-specific side information is required but excluded from accounting;
- the complete package does not improve over the chosen comparison baseline.

Failed configurations must remain recorded.

---

## 23. Claim Boundary

At v0.2:

- EIS is a defined and reproducible experimental architecture;
- the v1 per-symbol staggered-guard implementation exists;
- the tone-word architecture is formally specified here;
- no <50 MB carrier is claimed;
- no complete compression record is claimed;
- no result may be promoted from an extrapolation alone.

The next required evidence is a byte-exact tone-word implementation and sweep.

---

## 24. Defining Principle

> Epsilon Information Symphony distributes source data across simultaneous machine-readable voices. Within each voice, tones are attached into compact words. Silence marks word boundaries rather than every tone, and those boundaries are staggered across voices so the symphony remains continuously information-dense. A purpose-built reader uses the known lane alphabets, sub-symbol clocks, and staggered word guards to reconstruct the exact original bytes.

That mechanism defines EIS v0.2.
