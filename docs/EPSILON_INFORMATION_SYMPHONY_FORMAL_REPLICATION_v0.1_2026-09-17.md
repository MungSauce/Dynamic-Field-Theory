# Epsilon Information Symphony (EIS)
## Staggered-Guard Multicarrier Audio Codec — Formal Replication Specification v0.1

**Date:** 2026-09-17  
**Project:** Epsilonic / Epsilon Information Symphony (EIS)  
**Status:** Experimental; implementation exists, benchmark sweep queued at time of this document  
**Primary objective:** Determine the highest byte-exact source payload rate that can be carried by a compact audio file using a purpose-built, multi-instrument, staggered-guard signaling system.

---

## 1. Purpose

Epsilon Information Symphony (EIS) is an experimental codec in which arbitrary source bytes are converted into a dense synthetic audio "performance." Multiple machine-designed instrument lanes transmit data simultaneously. Each lane uses a small set of legal spectral states. Lane-specific silence guards are phase-shifted so that the guards do not coincide, allowing one lane to reset while the others continue transmitting.

The receiver is not a general audio recognizer. It is a deterministic reader built specifically for the known EIS signal family.

The project is intended to answer an empirical question:

> How much exactly recoverable digital information can a compact audio codec carry when both the transmitted waveform and the receiver are engineered together?

No result is accepted unless the decoded byte stream is exactly identical to the source.

---

## 2. Naming

**Formal project name:** Epsilon Information Symphony (EIS)  
**Technical subtitle:** Staggered-Guard Multicarrier Audio Codec  
**Short description:** A machine-readable symphony of pure information.

---

## 3. Current Test Corpus

The canonical test corpus is **enwik9**.

- Uncompressed size: **1,000,000,000 bytes**
- SHA-256: `159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc`
- MD5: `e206c3450ac99950df65bf70ef61a12d`
- Canonical source used by the workflow: `https://mattmahoney.net/dc/enwik9.zip`

The workflow must verify both the expected size and SHA-256 before testing.

---

## 4. Hard Targets and Accounting

### 4.1 Carrier target

The current engineering target is:

**Final audio carrier < 50,000,000 bytes for the 1,000,000,000-byte enwik9 source, with exact reconstruction.**

That corresponds to a carrier-size ratio below 5%, or greater than 20:1 source-to-carrier reduction.

### 4.2 Complete-package target

Carrier size alone is not a formal compression score. For a complete compression claim, all source-specific information needed for reconstruction must be counted, including:

- custom EIS decoder/reader;
- source-specific model or lookup data, if any;
- embedded dictionaries or side information;
- codec binaries/libraries if the benchmark rules require them;
- the audio carrier itself.

EIS therefore reports two values:

1. **Carrier bytes**
2. **Complete reconstruction package bytes**

### 4.3 Required payload-to-carrier relationship

Ignoring small container/header overhead, if the audio file is encoded at bitrate **B** and the EIS source payload is carried at rate **R**, the approximate carrier/source size ratio is:

`carrier/source ≈ B / R`

To reach a 5% carrier ratio:

`R >= 20 × B`

Examples:

| Audio carrier bitrate | Minimum exact source payload rate for <50 MB carrier |
|---:|---:|
| 6 kbps | 120 kbps |
| 12 kbps | 240 kbps |
| 24 kbps | 480 kbps |
| 32 kbps | 640 kbps |
| 64 kbps | 1.28 Mbps |
| 96 kbps | 1.92 Mbps |
| 128 kbps | 2.56 Mbps |

These are target relationships, not evidence that a lossy audio codec can preserve data at those ratios.

---

## 5. Core EIS Architecture

### 5.1 Parallel instrument lanes

The v1 implementation uses:

- **8 independent spectral lanes**
- **4 legal tone states per lane**
- **2 bits per lane state**
- **16 source bits per logical frame**
- **48 kHz mono PCM synthesis**

Current lane center frequencies:

`[900, 2200, 3500, 4800, 6100, 7400, 8700, 10000] Hz`

Current state offsets:

`[-240, -80, +80, +240] Hz`

Each lane therefore selects one of four frequencies around its center.

### 5.2 Staggered silence guards

Every lane has a silent guard interval at the start of each lane symbol period.

The critical EIS mechanism is that these guards are **not aligned**.

For lane index `i`, with `L` lanes and period `T`, the v1 phase is approximately:

`phase_i = i × T / L`

Thus lane resets are distributed across time. The aggregate waveform can remain active while individual lanes receive a clean local separation window.

Conceptually:

```
Lane 0: [gap][tone----------------][gap][tone----------------]
Lane 1:    [gap][tone----------------][gap][tone-------------
Lane 2:       [gap][tone----------------][gap][tone----------
...
```

The receiver knows these phase offsets in advance.

### 5.3 Transition shaping

Tone bursts use a Hann-shaped amplitude envelope inside the active window. The purpose is to reduce broadband transition energy and make the signal more tolerant of perceptual audio encoding.

### 5.4 Synchronization preamble

The current probe begins with a fixed two-tone timing preamble:

- 430 Hz for 250 ms
- 50 ms gap
- 860 Hz for 250 ms
- 50 ms gap

The receiver searches for this pattern to estimate codec-induced timing delay before decoding the lane streams.

---

## 6. Encoder Procedure

For an input byte stream:

1. Read bytes in source order.
2. Split each byte into four 2-bit groups, most-significant group first.
3. Distribute groups across the 8 lanes.
4. For every lane symbol:
   - apply the lane-specific phase offset;
   - leave the lane silent for its guard duration;
   - emit the legal frequency corresponding to that lane's 2-bit state during the active interval.
5. Sum all lane waveforms into mono PCM.
6. Apply conservative amplitude headroom to avoid clipping.
7. Encode the PCM waveform into the selected audio format and bitrate.

No linguistic prediction is used in EIS v1. No enwik-specific dictionary is required by the signaling layer.

---

## 7. Decoder Procedure

1. Decode the audio file back to mono PCM.
2. Search the beginning of the PCM stream for the known two-tone preamble.
3. Use the recovered synchronization offset to establish the absolute lane clocks.
4. For each lane and logical symbol index:
   - skip the known lane guard interval;
   - inspect only the known active interval;
   - compute matched-filter energy for each of that lane's four legal frequencies;
   - choose the highest-energy legal state.
5. Recombine all recovered 2-bit states in their original order.
6. Rebuild bytes.
7. Compare the recovered byte sequence with the original source.
8. Reject the configuration if any byte differs.

The current matched-filter implementation calculates in-phase and quadrature energy for every candidate tone.

---

## 8. Exactness Gate

A configuration passes only when:

- recovered byte count equals source byte count;
- every recovered byte equals the source byte at the same position;
- recovered SHA-256 matches source SHA-256 for the tested corpus/prefix.

**One wrong byte = failure.**

Detection confidence/margin may be reported for engineering purposes but cannot replace exact byte equality.

---

## 9. Current v1 Benchmark Matrix

Current GitHub workflow:

`.github/workflows/staggered-multicarrier-sweep.yml`

Current codec implementation:

`audio_lab/staggered_multicarrier_v1.py`

Current branch:

`hutter-target-apc-bot-20260917`

Current corrected workflow commit:

`424062e62620d1bbfbac59222c5c4c9ab38f204b`

Codec implementation commit:

`e0956c66fa2fe4b52836904e9869ffb683dab7a8`

Corrected workflow run:

`35311520767`

Status at document creation: **queued; no measured result is claimed in this document.**

### Sweep variables

- Sample rate: 48,000 Hz
- Source prefix: 4,096 bytes
- Logical period: 4, 6, 8 ms
- Guard interval: 0.5, 1.0, 1.5 ms
- MP3 bitrate: 64, 96, 128, 192 kbps

Total matrix: **36 configurations**.

The fastest zero-error configuration is selected automatically and promoted to an exact **1 MiB** round-trip test.

---

## 10. Current v1 Payload Rates

With 8 lanes × 2 bits/lane = 16 bits per logical frame:

- 4 ms frame: **4.0 kbps nominal source payload**
- 6 ms frame: **2.667 kbps nominal source payload**
- 8 ms frame: **2.0 kbps nominal source payload**

These values demonstrate that v1 is a **readability and synchronization proof**, not yet a <50 MB density candidate.

For example, a 64 kbps carrier would need at least 1.28 Mbps of exactly recoverable source payload to satisfy the 50 MB carrier target. The v1 architecture is intentionally far below that; its purpose is to establish the zero-error baseline before increasing density.

---

## 11. Density Scaling Plan

After a zero-error baseline exists, modify only one major dimension at a time.

### Stage A — Reduce temporal cost

- shorten logical period;
- shorten guard duration;
- retain phase-staggered guards;
- quantify error onset and matched-filter margin.

### Stage B — Increase state density

Increase states per lane:

- 4 states → 8 states → 16 states;
- measure how tightly spectral states can be packed while surviving the audio codec.

### Stage C — Increase parallelism

Increase lane count while respecting usable audio bandwidth and inter-lane interference.

Potential lane identities do not need to imitate literal acoustic instruments. "Bass", "snare", and "synth" are useful conceptual labels; the actual waveforms should be machine-designed for separability.

### Stage D — Use multiple signal dimensions

Candidate state dimensions:

- frequency;
- phase;
- amplitude;
- orthogonal/matched waveform shape;
- controlled transient signature.

Any added dimension must remain deterministically recoverable after audio encoding.

### Stage E — Compare carrier codecs

Run the same EIS source waveform and exactness gate through multiple carriers, including:

- MP3 baseline;
- Opus;
- other codecs only when a reproducible encoder/decoder is available.

The winner is not the smallest nominal bitrate. The winner is the smallest resulting file that preserves exact EIS reconstruction.

---

## 12. Recommended Search Objective

For each tested configuration, record:

- source bytes tested;
- carrier format;
- carrier bytes;
- audio bitrate;
- sample rate;
- lane count;
- states per lane;
- bits per lane state;
- symbol period;
- guard duration;
- per-lane phase offsets;
- nominal payload rate;
- effective payload rate including preamble;
- byte errors;
- exact PASS/FAIL;
- minimum matched-filter margin;
- mean matched-filter margin;
- estimated full-enwik9 carrier bytes at the same signaling rate;
- complete decoder/package size where relevant.

Primary optimization objective:

> Minimize complete reconstruction bytes subject to exact reconstruction.

Intermediate EIS-specific objective:

> Minimize audio carrier bytes subject to exact reconstruction.

---

## 13. Replication Procedure

### Prerequisites

- Linux environment capable of running Python 3
- FFmpeg with MP3 encode/decode support
- Git
- sufficient disk space for enwik9 and generated audio intermediates

### Clone and select branch

```bash
git clone https://github.com/MungSauce/Dynamic-Field-Theory.git
cd Dynamic-Field-Theory
git checkout hutter-target-apc-bot-20260917
```

### Fetch canonical enwik9

```bash
curl -L --fail --retry 5 --retry-delay 5 \
  https://mattmahoney.net/dc/enwik9.zip -o enwik9.zip
unzip enwik9.zip
test "$(stat -c%s enwik9)" = "1000000000"
echo "159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc  enwik9" | sha256sum -c -
```

### Run one v1 probe

```bash
python3 audio_lab/staggered_multicarrier_v1.py enwik9 \
  --prefix 4096 \
  --sr 48000 \
  --period-ms 4 \
  --guard-ms 0.5 \
  --bitrate 128 \
  --out-prefix eis_probe
```

A passing run must report:

```json
"exact": true,
"errors": 0
```

### Run the full matrix

Use GitHub Actions workflow:

`Staggered multicarrier MP3 exactness sweep`

or execute the same parameter matrix locally.

### Promotion rule

Only zero-error configurations may be promoted to larger corpora.

Recommended progression:

1. 4 KiB screening
2. 1 MiB
3. 10 MiB
4. 100 MiB
5. full 1,000,000,000-byte enwik9

At every stage, failure resets the candidate to engineering status.

---

## 14. Interpretation Rules

A smaller audio file is not automatically a better result.

The following do **not** count as success:

- audio that sounds correct but reconstructs different bytes;
- low byte-error rate;
- error correction using uncounted source-specific side information;
- a source-specific dictionary excluded from accounting;
- extrapolated size without an exact reconstruction run;
- a carrier that cannot independently reconstruct the source with the specified reader.

The following does count as a valid EIS signal result:

- specified encoder;
- specified audio carrier;
- specified reader;
- exact byte reconstruction;
- all required source-specific reconstruction information disclosed and counted.

---

## 15. Falsification Conditions

The experiment should be considered falsified for a proposed density point if:

- the codec erases distinctions needed by the reader;
- lane interference produces nonzero errors;
- synchronization cannot remain stable over larger corpora;
- the required carrier bitrate rises faster than source payload rate;
- total reconstruction package size no longer improves over direct binary compression;
- the proposed <50 MB configuration cannot reproduce canonical enwik9 exactly.

A failed configuration is useful evidence and should remain in the result log.

---

## 16. Current Claim Boundary

As of this document:

- the EIS staggered-guard codec exists in source code;
- the GitHub benchmark workflow exists;
- the corrected matrix workflow is queued;
- no zero-error benchmark result from that corrected run is yet claimed;
- no <50 MB result is claimed;
- no Hutter Prize result is claimed.

The current scientific claim is only that the EIS hypothesis is now implemented in a form that can be tested and falsified.

---

## 17. Next Experimental Milestone

1. Obtain the first zero-error MP3 baseline.
2. Record the fastest passing timing configuration.
3. Repeat at larger prefixes.
4. Add an Opus carrier with the identical EIS waveform and decoder.
5. Search for the maximum exact payload/carrier ratio.
6. Continue density increases only while exactness is preserved.
7. Treat <50 MB as a hard target, not an assumed outcome.

---

## 18. Replication Principle

EIS should remain understandable as a physical information channel:

> Source data is distributed across simultaneous machine-readable voices. Each voice receives its own non-coincident silence guard for local separation and resynchronization. A purpose-built reader reconstructs the voices into the exact original byte sequence. Density is increased experimentally until the exactness boundary is reached.

That is the defining mechanism of the Epsilon Information Symphony.
