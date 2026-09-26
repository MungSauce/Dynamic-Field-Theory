# MSF Modifier-Bank Page Orchestra v2.0

Status: CANDIDATE CANONICAL — validation in progress\n\nCanonical three-object model: orchestra/Encoder = the process that performs the page data; recording = the retained `.msf` artifact being optimized; audience/Decoder = the generic Listener that listens only to the recording and reconstructs the source.

## Canonical revision

1. Source length L is partitioned into N deterministic contiguous pages.
2. Instrument i owns page i. Instrument count is a codec parameter, not fixed at 32.
3. Every page is read from both ends toward the center.
4. A full logical timestamp therefore carries up to 2N source-symbol positions.
5. Every instrument reuses the same symbol lexicon. With A <= 206 source symbols, the two directional roles form at most 2A <= 412 directional note identities.
6. Instrument identity is a source-independent procedural modifier of that same lexicon; it is not a separately stored alphabet.
7. All modified instrument contributions are combined into one composite machine field.
8. The .msf is the retained signal. The Composer can be discarded/reset after composition.
9. The Listener knows the generic page, lexicon, modifier, mixing, and inverse rules. It must not retain source-specific answers outside .msf.
10. Exactness is established only by cold replay after source removal.

## Geometry

Page i:
```
start_i = floor(iL/N)
end_i   = floor((i+1)L/N)
```

Maximum chronological length:
```
T = ceil(max_page_length / 2) ~= ceil(L/(2N))
```

For 100,000,000 bytes:
- N=32: about 1,562,500 timestamps
- N=64: about 781,250
- N=128: about 390,625
- N=256: about 195,313
- N=512: about 97,657

Repeated doubling halves chronology. This is not claimed as byte compression unless complete retained .msf bytes also fall.

## Shared lexicon / modifier invariant

The intended abstraction is "the same trombone through different synth/effect chains": timbre/page identity changes, note meaning does not.

```
Instrument_i = M_i(shared_lexicon)
```

M_i must be reproducible from generic codec parameters and instrument index. A source-derived modifier table is forbidden unless explicitly stored and counted.

The reference discrete realization applies a procedural reversible modifier to each instrument state, then combines all states through a reversible cumulative field mixer. This is a test realization, not a claim of minimum physical signal width.

## Two simultaneous page directions

At timestamp t, page i can contribute:
- the t-th symbol from the front;
- the t-th symbol from the back.

The odd center of a page is emitted once. Deterministic page geometry tells the Listener which directional slots are active; no page mask is stored.

## Composer contract

The Composer:
- derives the alphabet;
- partitions pages;
- maps forward/reverse symbols to the shared directional lexicon;
- applies procedural instrument identity;
- forms one composite field per timestamp;
- writes minimal framing, the shared alphabet map when required, payload, lengths, and hashes.

It must not retain/write a source copy, per-position symbol stream, hidden answer key, correction stream, or uncounted source-dependent table.

## Listener contract

The Listener:
- parses and validates .msf;
- derives the same page boundaries;
- regenerates the modifier bank;
- inverts/separates the composite field;
- recovers both page directions;
- places symbols at deterministic positions;
- reconstructs exact length;
- requires source SHA-256 equality.

Decoder computation is allowed. Source-dependent retained data must be counted.

## Optional transport layer

Canonical:
```
source -> orchestra/Composer -> recorded .msf -> audience/Listener -> source
```

Optional:
```
.msf -> WAV/FLAC/MP3/other -> reconstructed .msf -> Listener
```

Transport is a separate experiment. WAV is a raw signal control, FLAC a lossless baseline, and MP3 a lossy error-channel test. Human hearing is irrelevant. For strict byte-preserving transport, original and reconstructed .msf SHA-256 must match before Listener decode.

## Validation/accounting

Every scale result records:
- N instruments/pages
- 2N source roles per full timestamp
- page length
- timestamp count
- payload bytes
- complete .msf bytes
- bytes/timestamp
- source bytes/timestamp
- source and recovered SHA-256
- source removed before decode
- PASS/FAIL

Required scale sweep: N = 32, 64, 128, 256, 512 on the same 100 MB enwik9 prefix.

## Status vocabulary

- CANONICAL: authoritative current architecture.
- MEASURED: executed cold replay, exact hashes, complete retained-byte accounting.
- EXPERIMENTAL: implemented but not sufficiently validated.
- PROJECTED: formula/partial measurement only.
- SUPERSEDED: lineage only.

## Lineage

EIS-K32 v1.0 remains the fixed-32 replication baseline. Earlier 32-instrument MSF tests remain measured lineage. The 100 MB dense-composite predecessor produced a 95,996,459-byte .msf with exact reconstruction; it does not establish modifier-bank scaling.

v2.0 becomes canonical when the declared modifier-bank scaling validation passes and its measured report is attached to the lineage.
