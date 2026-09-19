# TruCompute × EIS206 Mixed-Field v21

Status: EXPERIMENTAL / IMPLEMENTED
Date: 2026-09-18
Parent EIS lineage: `hmc-eis-206tone-20260918`

## Purpose

Test the latest EIS interpretation as a single mixed composition in which all 206 node carriers are physically present in every frame, then apply TruCompute's contextual-zero idea to the complete node field rather than to isolated K32 lanes.

This is explicitly a Hutter-oriented experiment. Signal-file shrinkage and source/Hutter shrinkage are reported separately.

## Mixed EIS composition

There are 206 physical nodes.

Every node owns one orthogonal carrier bin and every carrier is nonzero in every signal frame.

For source symbol/page `c`:

- node `c` plays at +A;
- every other node plays at -A.

The single stored real waveform is:

```
x_t = IFFT( sum_i state_i(t) * carrier_i )
```

with:

```
state_c(t) = +1
state_i(t) = -1, i != c
```

So this is not 206 separately stored streams. The frame is one composite waveform produced by all nodes at once.

The listener performs one FFT over that mixed frame and identifies the positive node.

## TruCompute interpretation

A finished 206-node composition becomes the contextual zero/reference for the next composition.

If the next source symbol is unchanged:

```
F_(t+1) = F_t
changed nodes = 0
```

If the selected node changes from a to b:

```
node a: +1 -> -1
node b: -1 -> +1
all other 204 nodes unchanged
changed nodes = 2
```

ENTER is the boundary that finishes the current composition. The resulting whole field is then the reference condition for the next frame.

The retained transition stream therefore stores:

- `0` when the whole composition is unchanged;
- `1..206` for the new selected node when the composition changes.

Given the previous composition, a nonzero event completely determines both node flips. No 206-bit field snapshot is written per symbol.

## Exactness

The v21 artifact is required to cold-replay both:

1. the original source bytes;
2. the exact original mixed EIS signal file.

The artifact embeds the 320-byte EIS signal header, including the counted source-specific alphabet map. It does not retain the original waveform.

On decode:

```
artifact
 -> transition events
 -> exact node sequence
 -> exact source
 -> deterministic all-node composer
 -> exact original mixed waveform
```

Both source SHA-256 and full signal SHA-256 are verified.

## Signal geometry

- nodes: 206
- all nodes physically active per frame: 206
- frame samples: 512
- sample bits: 16
- mixed waveform bytes per source symbol: 1024
- signal header: 320 bytes
- TruCompute artifact header: 416 bytes

The signal representation is intentionally expansive. Its purpose is to test whether a mixed physical composition can be losslessly collapsed back into relational state.

## Backends

Four retained-state backends are tested:

- `raw`: one contextual event byte per source symbol;
- `tc_rle`: TruCompute-native no-change run encoding only;
- `zlib`: generic entropy coder over the contextual event stream;
- `lzma`: stronger generic entropy coder over the contextual event stream.

Direct zlib/LZMA on the original source are measured beside them.

This separation is important: improvements caused by EIS/TruCompute modeling must be distinguished from improvements supplied by the outer entropy coder.

## Local core-model validation

A deterministic 4,500-byte text fixture was used during construction.

The all-node composer produced:

```
source bytes: 4,500
mixed waveform payload bytes: 4,608,000
listener recovery: exact
```

For the contextual event stream:

```
events: 4,500 bytes
zlib(events): 76 bytes
lzma(events): 136 bytes
zlib(original): 79 bytes
lzma(original): 140 bytes
```

This fixture is deliberately repetitive and is not a Hutter result. It only verifies that the field transform can expose repeated whole-composition states rather than destroying them.

## Enwik9-prefix benchmark

Workflow:

```
.github/workflows/trucompute-eis206-mixed-field-v21.yml
```

The workflow downloads canonical enwik9, extracts a prefix, then requires:

- exact mixed-signal listener recovery;
- exact source cold replay from each retained artifact;
- exact mixed signal regeneration;
- artifact bytes versus signal bytes;
- artifact bytes versus source bytes;
- direct zlib/LZMA source baselines;
- codec script bytes.

The initial CI prefix is 65,536 bytes. Full enwik9 is not claimed until the same exactness/accounting gate is run at full scale.

## Hutter accounting boundary

A large reduction from the mixed waveform is not, by itself, a Hutter compression result because the waveform is a deterministic expansion of the source.

The Hutter-relevant comparison is:

```
complete retained artifact + required decoder/model bytes
versus
original enwik9 bytes
```

and, for architecture evaluation, versus a direct compressor on the identical source prefix.

This v21 implementation therefore reports two distinct facts:

1. whether TruCompute can losslessly collapse the all-node EIS signal;
2. whether its contextual representation improves source compression.

## Claim boundary

v21 demonstrates the intended all-node mixed-composition architecture and an exact reversible whole-field delta representation.

It does not claim that two-node transitions create information for free. Once the prior field is known, the destination node is still sufficient information to identify the next one-hot 206-node state. Any actual Hutter gain must come from making that transition stream more predictable/compressible than the raw source, not from the waveform expansion itself.

## Source

```
trucompute/trucompute_eis206_mixed_field_v21.py
```


## Secondary local text fixture

A 65,536-byte real Python-source fixture was used as a less repetitive structured-text control.

Measured contextual field statistics:

```
source bytes: 65,536
alphabet symbols: 95
unchanged whole-field compositions: 14,622
changed compositions: 50,914
TC no-change RLE bytes: 55,191
```

Equivalent entropy-coder comparison:

```
direct zlib: 16,268 bytes
TC events + zlib: 16,393 bytes

direct LZMA: 15,352 bytes
TC events + LZMA: 15,492 bytes
```

So the native field transition representation removed about 15.8% by itself on this fixture, but the transformed stream was slightly worse than the original input once the same mature entropy coder was applied.

A 4,096-byte subset was also rendered through the actual all-206-node mixed waveform:

```
mixed payload bytes: 4,194,304
all 206 carriers present per frame: YES
FFT listener exact: PASS
```

This is a local engineering control, not an enwik9 or Hutter score.
