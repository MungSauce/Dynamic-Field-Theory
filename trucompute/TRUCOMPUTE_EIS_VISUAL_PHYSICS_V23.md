# TruCompute EIS × Visual Particle Physics v23

**Date:** 2026-09-19  
**Status:** EXPERIMENTAL / EXACT PHYSICS-REPLAY REFERENCE

## Purpose

Integrate the 206-node EIS field, a reusable 4K visual particle matrix, and contextual/reflex forcing into one cold-replayable machine.

The document is represented as a trajectory of a deterministic simulated material:

```
X_(t+1) = F(X_t, S_t)
```

where `S_t` is the EIS forcing choice, `X_t` is the current machine state, and `F` is the fixed physics law.

## Physical interpretation

The runtime contains:

- 206 continuously available EIS nodes;
- a fixed 3840 × 2160 = 8,294,400-pixel material;
- a deterministic raster painter;
- a snapshot collector;
- a separate snapshot reader.

At each source event the forcing field resolves one EIS node. The visual material settles that node ID into the next raster position.

The EIS composition itself is reused: if selection changes from node A to B, only A and B switch polarity. If the selection repeats, the whole EIS composition remains unchanged.

## Snapshot/read separation

A key architectural rule is that observation is not retained compression state.

The producer completes a visual page/frame, takes a snapshot of that settled state, and either:

1. **stream mode:** hands it immediately to the reader, which decodes it and discards it; or
2. **buffer mode:** accumulates completed snapshots, then runs the reader afterward and deletes them.

Both paths must reconstruct identical source bytes.

Snapshots are therefore runtime working memory only:

```
snapshot bytes retained in artifact = 0
```

For the 206-state alphabet each internal pixel state fits in one byte. If an entire one-billion-symbol visual trajectory were deliberately buffered before reading, the transient payload is therefore on the order of the source scale (~1 GB), not multiplied by RGB rendering. Streaming keeps peak snapshot memory to one completed frame (~8.29 MB).

A display renderer may map the byte palette to RGB colors, but RGB pixels are presentation and are not required for the internal state representation.

## Raster chronology

Inside a frame:

```
left -> right
top -> bottom
```

Frame order continues chronology after the fixed matrix is reused.

Thus no separate positional index is stored for the normal path. Position in the deterministic raster traversal is the temporal index.

## Forcing representation

The forcing stream uses the v22 deterministic contextual TRUE-rank law:

1. previous two EIS selections;
2. previous selection;
3. global observed selections;
4. static alphabet fallback on first use.

Within a context, candidates are ordered by count, recency, then node ID.

Only the unresolved rank is retained. Encoder and decoder rebuild the adaptive field from prior completed states.

## Exactness gate

The retained artifact contains only:

```
128-byte header
+ exact source alphabet (<=206 bytes)
+ compressed forcing stream
```

It does **not** contain visual snapshots.

Cold decode performs:

```
artifact
 -> forcing ranks
 -> EIS node selections
 -> EIS state transitions
 -> visual particle settlement
 -> completed-frame snapshot
 -> separate snapshot reader
 -> exact source
```

Source SHA-256 is verified after reconstruction.

## Accounting boundary

This version is an architecture proof, not yet a claim that physics creates compression for free.

The Hutter-relevant retained size is:

```
artifact + required decoder/program
```

Runtime RAM and transient snapshots are reported separately and are not counted as retained compressed payload, just as decompressor working memory is not itself part of a compressed file.

The forcing signal is fully counted. If it carries essentially all source entropy, the visual physics is merely a reversible representation. The research target is to improve the field law so that most next states arise deterministically and the retained forcing becomes only a small residual.

## Source

```
trucompute/trucompute_eis_visual_physics_v23.py
```
