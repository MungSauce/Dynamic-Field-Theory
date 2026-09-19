# TruCompression — Formal Replication Protocol v1.1

**Date:** 2026-09-18  
**Project:** Epsilonic / TruCompute / TruCompression  
**Status:** CANONICAL MACHINE SPECIFICATION / EXPERIMENTAL IMPRINT LAW

## 1. Definition

TruCompression is a fixed-capacity storage experiment implemented as a native program of the TruCompute split-state runtime.

The compressed artifact is the tuned machine itself.

The machine geometry is fixed before the source is opened. Imprinting may only tune state already present in the machine. If an imprint law requires another node, wider retained node state, another page object, a residual stream, a source-sized index, or any other source-dependent retained storage outside the frozen machine, that law fails TruCompression.

## 2. Canonical TruCompute dependency

Canonical runtime for this revision:

`trucompute/trucompute_runtime_v3.hpp`

TruCompute has four distinct machine conditions:

```
NEITHER         = no participating contribution
NEG             = resolved negative state
STATELESS_ZERO  = participating sum exactly zero
POS             = resolved positive state
```

The essential law is:

```
STATELESS_ZERO != NEITHER
```

`NEITHER` is not numeric zero. It has no numeric net because nothing participated.

`STATELESS_ZERO` is genuine computed zero because participating positive and negative contributions balanced exactly.

## 3. Fixed machine

Before source access:

- physical elements: exactly **1,000,000**;
- every element is structurally present at all times;
- retained tuning: exactly **one polarity bit per element**;
- retained machine payload: exactly **125,000 bytes**;
- page span: **1,000,000 positions**;
- character buttons: **256**, fixed generic order `0..255`;
- geometry never grows;
- retained per-element width never grows.

An element may be structurally present while currently expressing `NEITHER`.

Activation does not create the element. It only causes the existing tuned relation to participate.

## 4. Runtime settlement

For participating signed contributions:

```
sum < 0 -> NEG
sum = 0 -> STATELESS_ZERO
sum > 0 -> POS
```

When no contribution participates:

```
no participation -> NEITHER
```

Thus zero is a result. Neither is absence of a result.

Neither condition requires additional retained source-dependent storage.

## 5. Page excitation

For source position `t`:

```
q = floor(t / 1,000,000)
k = t mod 1,000,000
```

Page `q` is a control/excitation condition over the same million-element machine.

A page is not a retained page object.

No page bank, page embedding array, page snapshot, or extra million-element copy is permitted.

## 6. Character interrogation

For active page `q`, local position `k`, and character button `c`, the machine answers:

- `POS` = TRUE;
- `NEG` = FALSE;
- `STATELESS_ZERO` = participated but no polarity resolved;
- `NEITHER` = query/path did not participate.

For a valid reconstructed source position, exactly one character button must resolve TRUE:

```
sum_c [Press(q,k,c) == POS] = 1
```

A `STATELESS_ZERO` or `NEITHER` at a required payload decision is not silently converted to a character.

## 7. Fixed controller order

Read order is source-independent:

1. activate page `q`;
2. press byte buttons `0,1,...,255` in ascending order;
3. collect the position responses;
4. require one and only one TRUE button per valid position;
5. advance to the next page through the generic controller;
6. terminate after the counted source length.

The ordering is generic program logic, not stored source data.

## 8. Write/read temporal separation

### Imprint time

The source exists.

The imprint process may compare machine response with the source and retune only pre-existing internal polarity/relations.

Temporary training/solver memory is permitted only here and is destroyed before replay.

### Replay time

The source is absent.

The machine is immutable.

The reader receives only:

- frozen machine artifact;
- generic TruCompute runtime;
- generic TruCompression reader;
- counted source length contained in the artifact.

No training solver or source-derived side object may be consulted.

## 9. Frozen artifact

The v3 experimental machine retains:

```
64-byte fixed header
+ 1,000,000 polarity bits
= 125,064 bytes
```

The file reader rejects trailing source-dependent bytes.

The page law, routing law, character-button order, and state law are generic executable code.

## 10. No-growth invariant

If the machine cannot absorb a new relation:

```
IMPRINT_CONTRADICTION
```

is the required result.

Failure does not authorize:

- more elements;
- wider elements;
- appended relations;
- page files;
- residual/correction streams;
- source-specific sidecars;
- source-sized indexes.

The machine is already complete. Only a later imprint/routing/settling law may change.

## 11. Exact replay gate

A TruCompression PASS requires:

1. machine geometry fixed before source access;
2. exactly 1,000,000 physical elements;
3. one retained polarity bit per element;
4. all elements structurally present for the whole run;
5. no chronology-dependent machine growth;
6. source removed before replay;
7. fixed page chronology;
8. fixed character-button order;
9. exactly one TRUE character per required position;
10. recovered length exact;
11. recovered bytes exact;
12. recovered cryptographic hash identical to the pre-recorded source hash.

One incorrect byte is failure.

## 12. Current imprint law status

The current v3 machine still uses the v1 parity-style routing law so that the state-law correction can be isolated from the tuning-law experiment.

That routing law has demonstrated bounded exact replay on small inputs but a measured contradiction on a larger deterministic pseudo-random prefix.

This is a **FAILED-LAW** result for the routing/imprint law, not a reason to modify fixed machine capacity.

## 13. Canonical target

Canonical enwik9:

- size: **1,000,000,000 bytes**;
- SHA-256: `159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc`;
- MD5: `e206c3450ac99950df65bf70ef61a12d`.

No full enwik9 TruCompression result is claimed until the entire fixed-machine source-isolated exact-replay gate passes.

## 14. Evidence labels

- **CANONICAL** — settled architecture/protocol.
- **MEASURED** — directly executed evidence.
- **EXPERIMENTAL** — implemented but not through the full target gate.
- **PROJECTED** — extrapolated.
- **FAILED-LAW** — candidate tuning/routing law fails while machine invariants remain fixed.
- **TRUCOMPRESSION PASS** — complete fixed-machine source-isolated exact reproduction.

## 15. Reference implementation

```
trucompute/trucompute_runtime_v3.hpp
trucompression/trucompression_native_v3.cpp
```

Build:

```bash
g++ -O3 -std=c++17 trucompression/trucompression_native_v3.cpp -o trucompression_v3
```

The canonical repository filename is `trucompression_native_v3.cpp`; the command above should therefore be executed as:

```bash
g++ -O3 -std=c++17 trucompression/trucompression_native_v3.cpp -o trucompression_v3
```

NOTE: repository tooling should use the exact checked-in path reported by GitHub. The authoritative file is the v3 source under `trucompression/`.

## 16. DCT revision rule

Changes to the four-condition state law, physical element count, retained bit width, page-as-control invariant, read/write temporal separation, fixed query ordering, or no-growth rule constitute a new architecture.

A failed compression experiment may revise only the imprint/routing/settling law and must preserve its failure evidence.
