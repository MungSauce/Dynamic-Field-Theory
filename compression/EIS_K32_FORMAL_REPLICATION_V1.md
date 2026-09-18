# EIS-K32 Formal Replication Protocol v1.0

Status: **CANONICAL REPLICATION TARGET**

Date: 2026-09-18

Repository: \`MungSauce/Dynamic-Field-Theory\`

Canonical implementation directory: \`eis_k32/\`

This document supersedes earlier EIS/WAV/MP3 carrier experiments as the authoritative specification for the current compression/signal architecture.

## 1. Purpose

EIS-K32 is a machine-oriented stacked signal representation. It is not a consumer audio format and is not designed for human listening.

The architecture deliberately separates three responsibilities:

1. **Composer / Encoder** — converts source bytes into synchronized stacked carrier states.
2. **EIS-K32 file format** — stores the single composite machine-signal field.
3. **Listener / Decoder** — runs 32 carrier-specific listeners concurrently and reconstructs the original byte stream.

The core experimental idea is that 32 independent logical controls occupy one synchronized composite signal interval. Decoder computation is intentionally spent to separate those controls.

No compression credit is awarded for decoder work alone. Every stored source-dependent byte must be counted.

## 2. Canonical geometry

Version 1 uses:

- logical lanes / instruments: **32**
- dedicated listeners: **32**, one per lane carrier
- symbol states per lane: **256** byte states
- source symbols reconstructed per signal frame: **32**
- chronology: frame order, then lane order
- source position:
  \[
  p = 32t + b
  \]
  where \(t\) is the frame index and \(b\in[0,31]\) is the lane index.

Each lane is a fixed orthogonal digital carrier. A lane does not need to be human-audible.

Each source byte is one state of its lane for that frame. The v1 reference modulation maps:

- high nibble -> in-phase state \(I\)
- low nibble -> quadrature state \(Q\)

using 16 uniformly spaced values on each axis.

Thus each carrier has \(16\times16=256\) deterministic states and carries exactly one byte per frame.

All 32 modulated carriers are summed into one composite real signal block.

## 3. The three independently testable components

### 3.1 Composer / Encoder

Reference: \`eis_k32/composer_encoder.py\`

Input:
- arbitrary byte stream.

Required behavior:
1. Read source bytes exactly.
2. Split source into groups of 32 bytes.
3. Zero-pad only the final incomplete group; original length is retained in the container header.
4. For frame \(t\), assign byte \(s[32t+b]\) to lane \(b\).
5. Convert every byte to the canonical 16x16 I/Q state.
6. Generate the 32 fixed carrier basis functions from generic format parameters only.
7. Modulate all 32 carriers simultaneously.
8. Sum them into one composite frame.
9. Quantize the composite frame.
10. Fail if deterministic scaling would clip.
11. Write one \`.eis\` file through the format component.

The encoder MUST NOT write:
- a second copy of the source,
- per-position symbol IDs,
- per-frame lane choices outside the composite payload,
- source-derived frequency tables,
- hidden correction streams,
- WAV/MP3/FLAC sidecars required for decoding.

### 3.2 EIS-K32 machine-signal file

Reference: \`eis_k32/eis_k32_format.py\`

Extension: **\`.eis\`**

The file is not required to obey acoustic sample-rate, audible-frequency, PCM-WAV, MP3, or psychoacoustic conventions. “Carrier,” “tone,” “instrument,” and “listener” refer to digital signal dimensions.

The v1 file is:

\[
\text{128-byte header} + \text{quantized composite payload}
\]

Header fields:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | magic = \`EISK32V1\` |
| 8 | 2 | version = 1 |
| 10 | 2 | header bytes = 128 |
| 12 | 4 | flags |
| 16 | 2 | lane count = 32 |
| 18 | 2 | symbol states per lane = 256 |
| 20 | 2 | frame samples |
| 22 | 2 | sample bits |
| 24 | 2 | basis id |
| 26 | 2 | modulation id |
| 28 | 4 | reserved |
| 32 | 8 | exact source length |
| 40 | 8 | frame count |
| 48 | 8 | payload bytes |
| 56 | 32 | source SHA-256 |
| 88 | 32 | payload SHA-256 |
| 120 | 4 | header CRC32 over bytes 0..119 |
| 124 | 4 | reserved |

v1 reference payload samples are signed little-endian 16-bit integers.

The format is intentionally minimal. It stores the **composite signal**, not the 32 decoded lane states.

Future versions may introduce other quantizers or basis families, but any such representation must remain exactly decodable and byte-accounted.

### 3.3 Listener / Decoder

Reference: \`eis_k32/listener_decoder.py\`

Required behavior:
1. Parse and validate the EIS-K32 header.
2. Verify payload length and payload SHA-256.
3. Regenerate the 32 generic carrier/listener bases from header parameters.
4. For every composite frame, run all 32 listeners over the same stored samples.
5. Recover each lane's I/Q state independently.
6. Convert the 16x16 state back to its byte.
7. Emit lane bytes in deterministic order \(b=0..31\).
8. Trim only the known final padding using source_length.
9. Require the reconstructed source SHA-256 to match the header.

The 32 listeners may execute sequentially, vectorized, or in parallel. That changes compute cost, not file semantics.

## 4. Carrier construction

The reference basis is a real-valued orthogonal DFT field.

For a frame with \(M\) stored samples, choose 32 unique positive-frequency DFT bins distributed across the usable interval:

\[
1 \le k_b \le \lfloor M/2\rfloor-1
\]

The composer creates complex carrier coefficients \(X[k_b]\), mirrors them by Hermitian symmetry,

\[
X[-k_b]=X[k_b]^*
\]

then takes the real inverse DFT.

The listener performs the corresponding DFT and reads only the same 32 bins.

This is the machine equivalent of 32 instruments playing simultaneously while 32 frequency-specific listeners listen concurrently.

## 5. Chronology and reconstruction

No ordering stream is required for the base K32 codec.

Frame \(t\) always owns source positions:

\[
32t,\;32t+1,\ldots,32t+31
\]

Lane \(b\) always reconstructs position \(32t+b\).

Therefore the stacked signal carries 32 source symbols per chronological interval.

HMC/CSS/Domino may later operate above this representation by deciding which states need explicit storage, creating reusable signal relations, or introducing a deterministic key. Those are compression layers and MUST be benchmarked separately from the base K32 transport.

## 6. Non-human signal requirement

Canonical EIS-K32 does **not** use MP3.

Canonical EIS-K32 does **not** require WAV.

Canonical EIS-K32 does **not** require FLAC.

Those formats may be used as legacy diagnostics only and are not authoritative storage targets.

Because the decoder is software, the carrier basis should be optimized for:
- orthogonality,
- exact quantized recovery,
- maximum state density,
- minimum payload bytes,
- deterministic replay,

not for audibility, musical quality, psychoacoustic masking, or conventional speaker playback.

## 7. Exactness invariants

A replication PASS requires all of the following:

1. The only retained source-dependent artifact is the \`.eis\` file, unless an explicitly declared higher compression layer is being tested.
2. The original source is removed before decode in a cold-replay test.
3. Decoder output length equals the exact source length.
4. Decoder output SHA-256 equals the source SHA-256.
5. Every auxiliary source-derived rule/table/key required to decode is stored and counted.
6. Generic encoder/decoder algorithms may be source-independent code.
7. Any hidden source copy, correction stream, page mask, position list, or uncounted source-specific constant invalidates the result.

For canonical enwik9:

- bytes: **1,000,000,000**
- MD5: \`e206c3450ac99950df65bf70ef61a12d\`
- SHA-256: \`159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc\`
- terminal bytes observed historically: **206**

## 8. Required accounting

Every benchmark reports:

- source bytes,
- EIS payload bytes,
- EIS complete file bytes,
- header bytes,
- frame count,
- frame samples,
- quantizer bits,
- lanes = 32,
- reconstructed symbols per frame = 32,
- payload bits/source byte,
- complete bits/source byte,
- compression or expansion percentage,
- exact reconstruction PASS/FAIL,
- source SHA-256,
- recovered SHA-256,
- decoder/source-derived state bytes if complete-engineering accounting is being used.

Two scores should be kept distinct:

**Carrier score**
\[
R_{carrier}=\frac{|\text{EIS file}|}{|\text{source}|}
\]

**Complete engineering score**
\[
R_{complete}=\frac{|\text{EIS file}|+|\text{custom decoder/source-derived retained state}|}{|\text{source}|}
\]

## 9. Floor search

The primary engineering task after replication is to find the smallest exact composite frame.

Sweep:
- frame samples \(M\), beginning at the mathematical minimum that permits 32 distinct real carrier bins;
- sample quantization depth;
- deterministic drive/headroom;
- carrier-bin placement;
- alternative orthogonal basis families;
- symbol constellations;
- listener estimator.

For every candidate:
1. encode,
2. delete source from the decoding workspace,
3. decode from \`.eis\`,
4. verify exact SHA,
5. record size,
6. reduce representation again until exactness fails.

The objective is not “audio quality.” It is:

\[
\min |\text{EIS file}|\quad\text{subject to exact reconstruction}
\]

## 10. Reference replication commands

From repository root:

\`\`\`bash
python3 eis_k32/composer_encoder.py SOURCE.bin SOURCE.eis --frame-samples 96 --drive 1.0
python3 eis_k32/listener_decoder.py SOURCE.eis RECOVERED.bin --drive 1.0
cmp SOURCE.bin RECOVERED.bin
sha256sum SOURCE.bin RECOVERED.bin
\`\`\`

A formal cold-replay test must move or delete \`SOURCE.bin\` before running the listener and compare the recovered hash against a separately recorded source hash.

## 11. Relationship to earlier work

Earlier HMC, Domino, TRUECSS, WAV, QAM, FLAC and MP3 experiments remain useful lineage and falsification data. They are **not** the canonical EIS-K32 replication definition.

In particular:
- HMC established page/relational reconstruction ideas.
- Domino explored deterministic continuation and reusable structure.
- TRUECSS established strict source-removal and retained-state accounting.
- orchestral/QAM work established multi-dimensional carrier recovery.
- WAV/MP3 experiments were transport experiments.
- EIS Parallel Keyboard work established simultaneous control lanes.

All future compression experiments should state explicitly whether they:
1. reproduce base EIS-K32 exactly,
2. modify the EIS-K32 carrier representation, or
3. add an HMC/CSS/Domino layer above it.

## 12. Reproduction claim discipline

Do not claim a compression result from theoretical packing density alone.

A result is MEASURED only when the retained artifact has been decoded independently to the exact source and all source-derived retained bytes have been counted.

Projected full-enwik9 size may be reported only as **PROJECTED**, with the measured source size and scale factor stated beside it.

---

Canonical reference: **this document**.
