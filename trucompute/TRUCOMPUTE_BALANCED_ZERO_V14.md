# TruCompute Balanced-Zero Environment v14

Status: EXPERIMENTAL ARCHITECTURE BRANCH  
Date: 2026-09-18  
Parent: `trucompute-fixed-keyboard-v13-20260918`

## Purpose

This branch tests a stronger TruCompute substrate law:

> Every TruCompute cell is naturally and continuously the balanced state (+1,-1).  
> Computation does not create +1 or -1. It temporarily removes/suppresses one or both rails from that pre-existing balanced pair.

This is an architecture experiment, not a replacement of earlier TruCompute lineage.

## Four observable conditions

| code | state | interpretation |
|---|---|---|
| 00 | NEITHER | both rails suppressed / OFF in the current observation |
| 01 | NEG | + rail suppressed, leaving -1 |
| 10 | POS | - rail suppressed, leaving +1 |
| 11 | NATURAL_ZERO | both +1 and -1 present; net zero |

The central invariant is:

```
NATURAL_ZERO = (+1,-1)
NATURAL_ZERO != NEITHER
```

NEITHER is not zero. It is non-participation caused by suppressing both rails.

## Rest-state invariant

Every structural cell has the same immutable latent rail set:

```
latent(cell) = { -1, +1 }
```

There is no persistent scalar polarity in the cell itself.

A newly created environment therefore satisfies:

```
forall cell i:
    state(i) = NATURAL_ZERO
```

and after every observation frame is released, the same invariant holds again.

## Primitive computation

The only primitive state operation is rail suppression:

```
(+1,-1) - {-1}      -> POS
(+1,-1) - {+1}      -> NEG
(+1,-1) - {+1,-1}   -> NEITHER
(+1,-1) - {}         -> NATURAL_ZERO
```

This makes resolved values temporary projections of the balanced substrate rather than values written into the substrate.

## Observation frames

An observation frame contains temporary suppression masks.

It does not own or mutate the latent cell rails.

Therefore:

```
substrate S
+ observation frame F
-> expressed configuration C_F
```

Destroying/releasing `F` returns the observable environment to the natural balanced state without restoring data from a copied snapshot.

## Reference-conditioned collapse

The environment can store SAME or OPPOSITE relations between cells.

Holding one reference cell as POS or NEG creates a temporary observation frame. The held polarity propagates through the relation graph:

```
SAME      -> target exposes the same rail
OPPOSITE  -> target exposes the opposite rail
unreached -> target remains NATURAL_ZERO
```

Example:

```
A SAME B
B OPPOSITE C
C SAME D
```

Holding A as POS projects:

```
A=POS B=POS C=NEG D=NEG
```

Holding the same A as NEG, without rewriting the substrate, projects:

```
A=NEG B=NEG C=POS D=POS
```

Releasing either observation returns all four cells to NATURAL_ZERO.

## Contradictions

A relation network that demands both POS and NEG for the same cell under one held reference is inconsistent.

The runtime reports the contradiction. It must not silently choose a polarity, widen the cell, append hidden state, or mutate the underlying balanced substrate to rescue the observation.

## Current implementation

```
trucompute/trucompute_balanced_runtime_v14.hpp
trucompute/trucompute_balanced_environment_v14.cpp
```

Build:

```bash
g++ -O2 -std=c++17 -Wall -Wextra -pedantic \
  trucompute/trucompute_balanced_environment_v14.cpp \
  -o trucompute_balanced_v14
```

Conformance:

```bash
./trucompute_balanced_v14 selftest
```

Expected:

```
TRUCOMPUTE_BALANCED_ZERO_CONFORMANCE=PASS
natural_state=NATURAL_ZERO(+1,-1)
compute_primitive=SUPPRESS_RAIL
observation=REFERENCE_CONDITIONED_NONDESTRUCTIVE_PROJECTION
release=RESTORE_NATURAL_ZERO
contradiction_detection=PASS
```

Demo:

```bash
./trucompute_balanced_v14 demo
```

Expected projection:

```
hold node0=POS: POS POS NEG NEG
hold node0=NEG: NEG NEG POS POS
released substrate: NATURAL_ZERO NATURAL_ZERO NATURAL_ZERO NATURAL_ZERO
```

## Claim boundary

This implementation is a software model running on conventional binary hardware.

It demonstrates that the requested state law and observation semantics are executable and internally testable. It does not establish that a physical dual-rail implementation has been built, nor that this architecture provides a computational advantage.

The next experiments should test whether useful computation can be represented primarily as:

```
balanced relational substrate
-> hold reference
-> suppress incompatible rails
-> settle
-> read projection
-> release
-> reuse same substrate from another reference
```

without converting the architecture back into conventional stored Boolean values.
