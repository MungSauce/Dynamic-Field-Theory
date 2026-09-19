# TruCompute EIS Word-Pixel Physics v24

**Date:** 2026-09-19  
**Status:** EXPERIMENTAL / EXACT REFERENCE

## Core synthesis

v24 combines the user's word-button, EIS206, visual-painting, and separated snapshot-reader ideas.

A visual pixel is not one source character. It is one reusable **button identity** encoded directly as three base-206 channel states:

```
R206, G206, B206
```

Therefore one pixel has:

```
206^3 = 8,741,816
```

possible exact states.

The EIS field physically resolves the three channel states. The painter places that resulting pixel in deterministic raster order. Pixel position is chronology.

## Universal-space grammar

A normal non-whitespace source run is a word/button. Pressing it reflexively emits:

```
BUTTON_BYTES + one ASCII space
```

Therefore ordinary separator spaces require no separate button.

An exact whitespace control button exists only when the source differs from that default: newline, tab, multiple spaces, CR/LF combinations, leading whitespace, or terminal whitespace.

A whitespace override following a word removes the reflexive default space and emits its exact bytes.

If the final event is a word button, the decoder removes the final reflexive space.

This is byte-exact, not text-normalizing.

## Physics path

```
retained contextual forcing
 -> target word/control button
 -> button ID
 -> three base-206 EIS states
 -> RGB206 pixel
 -> deterministic raster position
 -> completed-frame snapshot
 -> separate snapshot reader
 -> button ID
 -> universal-space renderer
 -> exact source
```

The visual matrix is 3840 × 2160 and is reused after every completed frame.

## Snapshot separation

Two replay modes are mandatory:

- **stream:** one completed frame is read then deleted;
- **buffer:** snapshots are collected first, then a separate reader consumes and deletes them.

Neither mode retains snapshots in the compressed artifact.

The internal snapshot stores the three base-206 channel values directly, exactly three bytes per painted button-pixel. RGB display rendering is optional presentation, not required retained state.

## Contextual forcing

The artifact does not store the RGB206 snapshots. It stores the unresolved contextual TRUE ranks needed to reproduce button selections.

The adaptive law is inherited from v22:

- order-2 button context;
- order-1 context;
- order-0 observed buttons;
- static lexicon fallback on first use.

Encoder and decoder rebuild the same candidate ordering from completed events.

## Exactness and accounting

The complete artifact contains:

```
header
+ LZMA-compressed exact button dictionary
+ retained forcing events
```

The dictionary, forcing, and decoder are not free.

This version tests whether word-level painting plus universal-space elimination reduces the retained trajectory compared with the character-level v23 machine and the whitespace-token v22 machine.

The physics itself is not claimed to violate information bounds. Compression exists only where deterministic grammar/context removes choices from the retained forcing.

## Source

```
trucompute/trucompute_eis_word_pixel_v24.py
```
