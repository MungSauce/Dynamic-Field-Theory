# EIS Parallel Keyboard Architecture v1

## Purpose

EIS Parallel Keyboard is a custom audio-state file architecture for exact reconstruction of byte/text streams by treating sound as a synchronized parallel keyboard.

This document defines the representation architecture only. It does not define a second-layer compressor.

## Core model

Let:

- A = 206, the canonical enwik9 terminal alphabet/page count.
- B = number of parallel instrument banks.
- T = number of synchronized logical frames.
- M = stored samples per frame.
- Q = stored sample bit depth.

Each bank is a complete 206-instrument keyboard. Within one bank, instrument/page c corresponds to one canonical terminal symbol c.

At logical frame t, each bank b selects exactly one of its 206 instruments (or PAD after the end of the source).

The source position written by that bank is deterministic:

    p = t * B + b

Therefore ordering is not stored separately.

For source symbol s[p], bank b activates instrument:

    c(t,b) = page_id(s[p])

A frame is rendered by superposing all B selected bank/instrument basis waveforms:

    x_t[n] = gain(B) * sum_{b=0}^{B-1} phi(b, c(t,b), n)

for n = 0 ... M-1.

The complete payload is the concatenation of x_t frames.

## Interpretation

- Time/frame index = chronology.
- Bank index = fixed interleaved output lane.
- Instrument identity within a bank = page/terminal identity.
- Simultaneous instruments = simultaneous key presses for different future character positions.
- The decoder performs B * 206 matched listeners/filter channels over the same frame.
- The winning instrument in each bank emits one terminal into its deterministic output position.

Multiple banks may map different physical tones/signatures to the same terminal output. Example: with B=2, there are 412 physical instrument signatures but only 206 terminal outputs. Instrument (bank 0, page 'e') and instrument (bank 1, page 'e') both emit 'e', at different interleaved positions.

## Decoder

For each frame t:

1. Read M stored samples.
2. For every bank b, evaluate its 206 listeners against the same frame.
3. Select page c with the strongest valid bank-specific response.
4. Write terminal c to output position p = t*B+b.
5. Continue until original_length positions have been written.

All bank listeners operate on the same stored samples. More banks increase decoder work, not payload frame count.

A valid file must decode deterministically with an unambiguous margin. Ties or threshold failures are invalid.

## Basis family

The basis family is generic decoder logic, not source-derived metadata.

A basis is identified by:

    phi(bank_id, page_id, sample_index)

v1 permits several generic basis families for benchmarking:

1. FREQ — distinct sinusoidal carriers.
2. FREQ_PHASE — frequency plus bank-specific phase.
3. CODED_TONE — tone multiplied by a bank-specific orthogonal/pseudorthogonal code.
4. MULTI_BASIS — deterministic combinations of frequency, phase, quadrature and code sequence.

The encoder and decoder generate the same basis from the basis_family identifier and generic parameters. Per-source carrier tables are forbidden unless their bytes are counted.

## Amplitude / clipping

Because B instruments are active simultaneously, gain must be deterministic and independent of source content.

Recommended baseline:

    gain(B) = headroom / sqrt(B)

The exact function is part of the basis-family specification.

No source-specific normalization curve may be hidden in the decoder.

## Proposed .eis container

Header:

- magic: "EIS1"
- version
- basis_family_id
- bank_count B
- page_count = 206
- frame_samples M
- sample_rate / abstract clock rate
- sample_format and bit depth Q
- original_length
- canonical alphabet-map id
- payload byte length
- source SHA-256
- header CRC

Payload:

- raw quantized mixed frames only.

Trailer (optional):

- payload checksum

No page masks, character positions, per-frame symbol IDs, source-derived dictionaries, or hidden residual streams are part of the base architecture.

## Density metric

For raw .eis payload:

    payload_bits = T * M * Q

where:

    T = ceil(original_length / B)

Ignoring the small header:

    stored_bits_per_character ~= M * Q / B

Therefore the architecture's principal density experiment is:

    maximize B / (M * Q)

subject to exact reconstruction.

Increasing the number of simultaneous instrument banks is useful only if the listener can still recover every bank/page choice exactly without increasing M or Q enough to erase the gain.

## Benchmark matrix

The architecture should sweep:

- B: 1, 2, 4, 8, 16, 32, ...
- M: shortest exact frame size for each B
- Q: 8, 12 packed, 16, 24 bits
- basis family
- sample/clock rate where relevant
- gain/headroom
- detection method and threshold

For every point record:

- logical symbols encoded
- payload bytes
- complete bytes including decoder if accounting requires it
- exact reconstruction PASS/FAIL
- source SHA match
- minimum winning-listener margin
- maximum cross-bank/cross-page interference
- bits per reconstructed character

The winning architecture point is the smallest exact M*Q/B, not the loudest or most human-audible signal.

## Relationship to HMC / CSS

The 206 terminals/pages are preserved directly as instrument identities.

EIS supplies a physical/signal representation in which multiple page identities can be active concurrently. HMC/CSS can later operate above this base architecture by deciding which positions or relations need explicit activation, but that is a separate compression layer and is intentionally excluded from v1.

## Non-goals for v1

- MP3/FLAC compression.
- Domino/key residual coding.
- Source-specific learned dictionaries.
- Arithmetic coding.
- Claiming compression from simultaneity without an exact byte-accounted benchmark.

v1 exists to answer one question:

How many independently recoverable 206-way keyboard banks can one stored mixed frame carry exactly?


---

> **Canonical replication notice (2026-09-18):** This is predecessor/lineage material. Current protocol: [EIS-K32 Formal Replication v1.0](../compression/EIS_K32_FORMAL_REPLICATION_V1.md).
