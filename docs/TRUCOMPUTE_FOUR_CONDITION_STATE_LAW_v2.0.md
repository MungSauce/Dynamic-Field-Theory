# TruCompute Four-Condition State Law v2.0

**Status:** CANONICAL CORRECTION  
**Date:** 2026-09-18

## Canonical machine states

TruCompute has four distinct machine conditions:

| Code | State | Meaning |
|---|---|---|
| `00` | `NEITHER` | no participating contribution; not addressed; not numeric zero |
| `01` | `NEG` | resolved negative orientation |
| `10` | `POS` | resolved positive orientation |
| `11` | `STATELESS_ZERO` | participating/active contributions resolve to net zero |

The critical distinction is:

```
STATELESS_ZERO != NEITHER
```

`STATELESS_ZERO` is a real computed zero:

```
(+1) + (-1) = 0
```

and records that the structure participated but has no resolved polarity.

`NEITHER` means no state participated in the computation. It is the condition previously overloaded as "dead zero" / `DEAD_STOP`.

## Numeric law

Numeric net is defined for participating states only:

```
NEG            -> -1
POS            -> +1
STATELESS_ZERO ->  0
NEITHER        -> no numeric value
```

An implementation MUST NOT map `NEITHER` to numeric zero.

## Structural-live law

A TruCompute circuit element may be structurally present and wired at all times while currently expressing `NEITHER`.

Activation does not allocate or create an element. It exposes the element's already-retained orientation into the active circuit.

Thus:

```
structural existence != instantaneous signal state
```

and an inactive million-element TruCompression machine can have all one million structures present while each currently reports `NEITHER`.

## Settlement law

For an addressed relation:

```
sum < 0  -> NEG
sum > 0  -> POS
sum = 0  -> STATELESS_ZERO
```

For a relation with no participating contribution:

```
no participation -> NEITHER
```

## TruCompression implication

TruCompression character interrogation uses the four conditions as follows:

- `POS` — TRUE;
- `NEG` — FALSE;
- `STATELESS_ZERO` — the query participated but no polarity resolved;
- `NEITHER` — this structure/query did not participate.

The source-dependent retained machine still uses one polarity bit per physical element. Neither zero condition requires another retained payload bit; they are runtime conditions produced by participation and settlement.

This document supersedes any earlier wording that equated `DEAD_STOP` with numeric zero or described both inactive and balanced states as equivalent zero states.
