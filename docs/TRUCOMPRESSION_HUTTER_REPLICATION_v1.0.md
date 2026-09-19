# TruCompression — Hutter-Facing Replication Protocol v1.0

**Date:** 2026-09-18  
**Status:** EXPERIMENTAL / NO PRIZE CLAIM

## Purpose

This document gives a conventional verification surface for TruCompression without requiring the verifier to adopt the internal signed/split-state terminology used by the research notes.

It does not conceal any bytes required for decoding or scoring.

## Public machine description

The submitted artifact represents one fixed-capacity binary-state machine.

Frozen before source access:

- exactly 1,000,000 binary retained machine elements;
- exactly 125,000 bytes of retained machine-state payload;
- fixed 1,000,000-position page span;
- fixed byte-query order 0 through 255;
- deterministic source-independent routing code.

A page index is an input condition. It is not a stored page.

A byte button is an input query. For page `q`, position `k`, and byte `c`, the machine answers TRUE or FALSE.

Exactly one byte query must answer TRUE for each reconstructed position.

## Encoding/imprint phase

The source is available only during imprinting.

The encoder may tune the existing 1,000,000 retained binary elements. It may use temporary computation and solver memory, but none of that temporary state may survive into the compressed artifact.

If the fixed machine cannot satisfy another source relation, encoding fails. Capacity may not be increased.

## Decoding phase

The source is unavailable.

The decoder receives only:

- the complete frozen machine artifact;
- the generic submitted decoder/runtime;
- source length contained in the counted artifact.

For each page it evaluates byte buttons in fixed order `0..255`, reconstructs each position from the unique TRUE response, then advances to the next page. The final page terminates at the counted source length.

## Forbidden hidden state

A valid result contains no:

- page bank;
- page-specific snapshots;
- source-sized index;
- character position lists;
- residual/correction stream;
- source-derived dictionary outside the counted artifact;
- source-derived executable constants not charged to the submission;
- network/external side information.

## Complete size accounting

The research carrier size is:

```
complete .truc artifact bytes
```

The Hutter-style complete score must additionally include every custom program/runtime/model/dictionary/asset byte that the applicable rules require to ship.

No decoder dependency may be omitted from accounting merely because it is described as part of the machine.

## Canonical target

enwik9:

- 1,000,000,000 bytes;
- SHA-256 `159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc`;
- MD5 `e206c3450ac99950df65bf70ef61a12d`.

A benchmark claim requires source-isolated exact reconstruction of all one billion bytes.

## Current claim boundary

The fixed machine and replay interface are implemented.

The v1 imprint law has passed small exact-replay tests but has also produced a measured fixed-capacity contradiction before 70,000 pseudo-random input bytes.

Therefore:

- no enwik9 compression result is claimed;
- no Hutter score is claimed;
- the measured contradiction is retained as negative evidence;
- future work may replace the imprint law without changing machine capacity or replay accounting.

## Public replication commands

```bash
g++ -O3 -std=c++17 trucompression/trucompression_native_v1.cpp -o trucompression
./trucompression selftest
./trucompression imprint SOURCE.bin machine.truc
rm SOURCE.bin
./trucompression replay machine.truc RECOVERED.bin --strict-buttons
```

A valid result must independently verify recovered length and cryptographic hash against values recorded before source removal.
