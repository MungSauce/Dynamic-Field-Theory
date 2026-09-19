# TruCompute × EIS Signal Compression Probe v20

Status: EXPERIMENTAL / EXACT ROUND-TRIP VERIFIED
Date: 2026-09-18
Purpose: test whether the canonical EIS-K32 machine-signal representation can be collapsed into a smaller TruCompute contextual artifact without losing either the original source or the exact EIS signal file.

## Architecture

Canonical EIS-K32 is retained unchanged:

- 32 simultaneous lanes
- 256 states per lane
- 32 source bytes per frame
- 96 signed 16-bit composite samples per frame in the canonical probe
- deterministic DFT/QAM composer
- deterministic listener
- exact SHA-256 reconstruction gate

The TruCompute layer operates above EIS-K32.

For each lane b, the preceding lane value is treated as that lane's contextual zero:

```
Z_b(t) = previous resolved lane value
D_b(t) = X_b(t) - Z_b(t) mod 256
```

The transformed residuals are stored lane-major so one lane's chronology is contiguous.

Interpretation in the current TruCompute language:

- contextual zero = old resolved state from which the next +/- displacement is measured
- signed displacement = resolution away from that zero
- terminal/end of event = the displacement is committed and becomes the next zero
- no literal history list is appended

This v20 experiment uses byte-valued displacement for engineering convenience. It is a bridge experiment, not yet a physical two-rail implementation.

## Exact signal regeneration

The retained artifact contains:

1. a 184-byte TruCompute-EIS header;
2. the original 128-byte EIS header inside it;
3. a compressed contextual-delta stream.

On decode:

1. contextual deltas reconstruct the exact 32-lane byte stream;
2. the canonical EIS composer regenerates the composite signal;
3. regenerated source SHA-256 must equal the original source SHA-256;
4. regenerated full EIS file SHA-256 must equal the original EIS file SHA-256.

A pass therefore proves both source and signal-file round trip.

## Measured local probe

### Structured synthetic PCM signal

Input:
- raw signal bytes: 1,048,576
- deterministic multi-tone PCM with slowly varying amplitude and periodic silence

Canonical EIS-K32:
- exact EIS file: 6,291,584 bytes
- EIS/source ratio: ~6.0001

TruCompute-EIS retained artifacts:

| backend | artifact bytes | vs EIS | vs original signal source |
|---|---:|---:|---:|
| raw contextual state | 1,048,760 | 16.67% | 100.018% |
| zero-run only | 996,542 | 15.84% | 95.04% |
| contextual + zlib | 207,761 | 3.30% | 19.81% |
| contextual + LZMA | 156,736 | 2.49% | 14.95% |

All four artifacts regenerated:
- the exact original source bytes;
- the exact original EIS signal bytes.

### Random 1 MiB control

| backend | artifact bytes | vs original |
|---|---:|---:|
| raw contextual state | 1,048,760 | 100.018% |
| zero-run only | 1,059,108 | 101.00% |
| contextual + zlib | 1,049,086 | 100.05% |
| contextual + LZMA | 1,048,872 | 100.03% |

The random control does not compress. This is required evidence that the structured-signal result is not caused by a missing-data path.

### Text control

Input:
- 16,928-byte Epsilonic project text file

Results:

| backend | artifact bytes | vs source |
|---|---:|---:|
| raw contextual state | 17,112 | 101.09% |
| zero-run only | 17,926 | 105.90% |
| contextual + zlib | 14,860 | 87.79% |
| contextual + LZMA | 14,156 | 83.63% |

For the same text file, direct LZMA was 6,376 bytes in the local comparison.

Therefore this particular 32-lane old-zero transform is useful for the structured PCM signal but actively harms the text compression model.

## Critical accounting distinction

Two separate ratios MUST be reported.

### Signal compression

EIS composite signal -> TruCompute-EIS artifact.

This probe succeeds strongly on the structured signal:

```
6,291,584 -> 156,736 bytes
```

with exact signal regeneration.

### Hutter/source compression

Original source -> complete retained artifact.

For the signal input:

```
1,048,576 -> 156,736 bytes
```

is genuine lossless source compression for that structured PCM file.

However, collapsing a canonical EIS carrier from ~6x source size back toward ~1x source size is NOT by itself Hutter compression. The EIS carrier is a deterministic expanded representation.

For text, v20 does not currently improve on a direct general-purpose compressor.

## Hutter Prize relevance

All EIS/TruCompute experiments are being treated as candidate preprocessing/modeling architectures for the enwik9 objective.

For Hutter-style accounting, the relevant total is:

```
compressed enwik9 bytes
+ zipped decompressor/source and all required source-specific runtime files
```

The EIS carrier size is not a Hutter score unless it is itself the retained reconstruction artifact.

v20 is therefore evidence of:

- exact signal-domain collapse: YES
- structure-sensitive source compression on PCM: YES
- random-data false gain: NO
- text/Hutter advantage: NOT SHOWN
- enwik9 result: NOT YET MEASURED

## Next experiment

Do not simply run the current transform on full enwik9 and expect improvement.

The next useful EIS/TruCompute test is to make the simultaneous 32-lane field share linguistic/contextual prediction, so each lane's active zero is predicted from the whole preceding field rather than only the previous value of that lane.

The success criterion is not smaller EIS samples. It is:

```
complete exact artifact < direct baseline on the same enwik9 prefix
```

with the decoder and any retained model bytes counted.
