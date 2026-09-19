# TruCompute Reflex Lexicon v22

Status: EXPERIMENTAL / EXACT REFERENCE CODEC
Date: 2026-09-19

## Purpose

Implement the TruCompute "word as button" model as an exact reversible program.

This is a fallback representation for material not already captured by a stronger structural, factual, numerical, XML, phrase, or relational program.

A maximal non-whitespace byte run is one reusable button. Exact whitespace runs are also buttons so no separator side stream is hidden.

## Core model

Let the retained lexicon contain N unique buttons.

For token position t:

- W_t is the true next button.
- C_t is the already reconstructed context.
- pi(C_t) is the deterministic reflex ordering of candidate buttons.
- R_t is the zero-based position of W_t in that ordering.

The reflex interpretation is:

```
candidate 0 -> FALSE?
candidate 1 -> FALSE?
...
candidate R_t -> TRUE
ENTER
```

Only R_t has to be retained. The FALSE states are consequences of traversing the known ordering and are not stored individually.

## Information invariant

Given context C_t and deterministic ordering pi(C_t), the map

```
W_t <-> R_t
```

is one-to-one.

Therefore:

```
H(W_t | C_t) = H(R_t | C_t)
```

The button/reflex representation does not create compression by renaming a word as a delay. Its value is that a useful context makes small delays much more probable and exposes that conditional structure to a compact code.

If k_t buttons remain genuinely equiprobable after all deterministic reflexes, the unresolved lower bound is:

```
log2(k_t) bits
```

If context resolves all but one button, k_t = 1 and the word identity itself requires zero additional choice bits once that context has been established.

## Geometric-delay illustration

If delay approximately follows:

```
P(R=r) = p(1-p)^r
```

then:

```
H(R) = -log2(p) - ((1-p)/p) log2(1-p)
```

and a literal unary FALSE...TRUE traversal has expected length:

```
E[L_unary] = 1/p
```

Example: p = 0.5 gives an ideal geometric entropy of 2 bits per press and a 2-bit mean unary traversal. This is only an illustration; measured rank distributions determine the real result.

## v22 exact tokenizer

The reference tokenizer is byte-exact:

```
\s+ | \S+
```

Thus:

```
hello,   world\n
```

becomes conceptually:

```
[hello,] [SPACE x3] [world] [NEWLINE]
```

No normalization occurs. Joining the decoded button byte strings must regenerate the exact source.

## Lexicon

Each unique button definition is retained exactly once.

Buttons are stored in:

1. descending whole-source frequency;
2. raw-byte lexical order for ties.

The dictionary ordering itself therefore carries the common-button ordering; no separate frequency table is retained.

The dictionary section is LZMA-compressed independently and its complete byte count is reported.

## Adaptive reflex fields

v22 uses source-independent adaptive context rules rebuilt by both encoder and decoder from already completed button presses.

It tries:

1. order-2 context: previous two buttons;
2. order-1 context: previous button;
3. order-0 context: globally observed buttons;
4. static lexicon fallback for a button never previously observed.

Within an adaptive context, candidate buttons are ordered by:

1. descending observed successor count;
2. descending recency;
3. dictionary ID as deterministic tie-breaker.

The target rank is exactly the number of reflex candidates that resolve FALSE before the TRUE button.

No adaptive model table is stored in the artifact.

## Event representation

Each event is one unsigned varint.

Low two bits:

```
0 = static dictionary fallback
1 = order-0 reflex field
2 = order-1 reflex field
3 = order-2 reflex field
```

Upper bits:

```
TRUE delay / rank
```

So:

```
event = (rank << 2) | field_kind
```

The event stream may then be stored raw, zlib-compressed, or LZMA-compressed. These outer coders are reported separately from the reflex transformation.

## Artifact

The exact v22 artifact consists of:

```
128-byte header
+ LZMA-compressed unique-button dictionary
+ reflex event stream
```

The header stores source length, token count, unique-button count, section lengths, backend identity, source SHA-256, and CRC.

Decode rebuilds the adaptive reflex fields solely from earlier decoded presses and requires exact source SHA-256.

## Local exact measurement

Engineering fixture:

```
source bytes:                         16,928
button presses:                        4,564
unique buttons:                        1,114
fixed global button ID width:             11 bits/press
dictionary LZMA:                       5,160 bytes
raw reflex events:                     6,112 bytes
LZMA reflex events:                    4,216 bytes
complete v22 LZMA artifact:            9,504 bytes
direct LZMA source:                    6,376 bytes
exact reconstruction:                  PASS
```

Reflex behavior:

```
rank-0 TRUE fraction:                 48.576%
first-use/global fallback fraction:   24.408%
empirical (field-kind,rank) entropy:   6.394 bits/press
fixed global-ID representation:       11 bits/press
empirical event entropy equivalent:   ~3,648 bytes
```

The exact first-use fallback count was 1,114, equal to the number of unique buttons. Once a button had appeared, later occurrences could be resolved through adaptive reflex fields.

## Interpretation of the local result

The reflex sequence is materially more structured than a fixed global-ID stream:

```
11 bits/press fixed-ID width
vs
6.394 bits/press measured event entropy
```

However the small fixture must pay 5,160 bytes to retain its entire lexicon, so the complete v22 artifact does not beat direct LZMA.

That is not treated as failure of exactness or as a compression win. It isolates the two questions:

1. Does reflex ranking reduce sequence uncertainty? YES on this fixture.
2. Does sequence gain amortize the retained lexicon and beat a mature compressor? NOT on this fixture.

## Hutter relevance

For enwik9 the important quantity is:

```
dictionary
+ reflex sequence
+ all required decoder/model bytes
```

The lexicon is not free.

The next benchmark is a canonical enwik9 prefix with:

- exact cold replay;
- direct zlib/LZMA comparison;
- rank-0 rate;
- first-use fallback rate;
- empirical delay entropy;
- complete artifact size;
- codec source bytes reported separately.

The architecture is promoted only if larger-corpus amortization makes the complete retained representation competitive.

## TruCompute interpretation

This is the first direct reference program for the user's Boolean-button/reflex proposal:

```
active contextual field
-> deterministic candidates exist as possible buttons
-> reflex ordering tests possibilities
-> FALSE possibilities need not be retained
-> TRUE occurs at delay R
-> ENTER settles the press
-> completed press updates context
-> next active field
```

The stored delay is not claimed to evade information theory. It is the remaining unresolved information after the context/reflex program has done its deterministic work.

## Source

```
trucompute/trucompute_reflex_lexicon_v22.py
```


## Canonical enwik9 64 KiB result

GitHub Actions run: `35428137920`

The canonical enwik9 prefix probe and cold replay completed successfully.

```
source bytes:                         65,536
button presses:                       15,331
unique buttons:                        3,469
fixed global ID width:                    12 bits/press
fixed-ID sequence equivalent:         22,996.5 bytes

dictionary raw:                       40,618 bytes
dictionary LZMA:                      18,200 bytes

raw reflex events:                    20,444 bytes
reflex events LZMA:                   13,444 bytes
empirical reflex entropy:              6.755 bits/press
empirical entropy equivalent:         12,945 bytes

complete v22 LZMA artifact:           31,772 bytes
artifact/source ratio:                 48.480%

direct zlib:                          24,577 bytes
direct LZMA:                          22,604 bytes
```

Reflex resolution statistics:

```
rank-0 TRUE presses:                   8,023
rank-0 fraction:                      52.332%
first-use/global fallbacks:            3,469
fallback fraction:                    22.627%

order-0 resolutions:                   3,536
order-1 resolutions:                   3,202
order-2 resolutions:                   5,124
```

The fallback count again exactly equals the number of unique buttons. Every unique button therefore pays one first-use/static selection; subsequent appearances are eligible for contextual reflex resolution.

The measured sequence result is significant but is not yet a complete compression win:

```
fixed button space:    12.000 bits/press
measured reflex event:  6.755 bits/press
```

This is about a 43.7% reduction in empirical sequence entropy relative to fixed-width global button selection.

The complete artifact still loses to direct LZMA on this 64 KiB prefix because the full lexicon costs 18,200 bytes at this small scale.

Cold replay:

```
decoded bytes: 65,536
source SHA-256:
05fc5f44993ef0557959db76bf47e45badb2dd9c69d93ce08911935b5e52bf40

recovered SHA-256:
05fc5f44993ef0557959db76bf47e45badb2dd9c69d93ce08911935b5e52bf40

exact: PASS
```

## Random-button control

A 64 KiB synthetic control built from 1,024 randomly selected word buttons plus deterministic spaces also passed exact reconstruction.

```
complete v22 LZMA artifact: 19,584 bytes
direct LZMA:                15,568 bytes
rank-0 fraction:            50.108%
empirical event entropy:     6.410 bits/press
```

This control intentionally contains predictable alternating separator buttons, so it is not a uniformly random token process. It confirms exact operation and shows that merely converting values into buttons/ranks does not beat direct compression.

## Current conclusion

v22 validates the mathematical mechanism:

1. a complete lexicon can be represented as reusable buttons;
2. FALSE candidates do not need separate retained bits;
3. the TRUE button can be represented by deterministic contextual delay/rank;
4. encoder and decoder independently rebuild the same reflex ordering from prior presses;
5. exact source recovery succeeds;
6. on real enwik9 text the delay distribution is substantially narrower than the global button space.

The remaining problem is no longer whether the button/reflex representation works. It is whether stronger TruCompute programs can establish contexts/categories/relations that make the remaining TRUE delays small enough to amortize the retained lexicon and outperform mature compressors.
