# Q-OS Temporal-Vector Transition Algebra v0.1

Status: CANONICAL CORE LAW
Date: 2026-09-19

## Active state set

Q-OS has four allocated temporal states and one structural absence.

| Meaning | Symbol | Host code |
|---|---|---|
| peak negative | `-` | `00` |
| primed zero, upward momentum | `-+` | `01` |
| decaying zero, downward momentum | `+-` | `10` |
| peak positive | `+` | `11` |
| structural absence | `Null` | unallocated |

`Null` is not a fifth Q-state and MUST NOT be serialized into the two-bit state field.

## Observable components

The four states may be projected to magnitude and direction:

- `-`  -> magnitude -1, direction 0
- `-+` -> magnitude  0, direction +1
- `+-` -> magnitude  0, direction -1
- `+`  -> magnitude +1, direction 0

The projections do not replace the four-state identity.

## Strike algebra

A node changes only when a positive or negative tension strike is delivered to it.

| Current | `+` strike | `-` strike |
|---|---|---|
| `-` | `-+` | `-` |
| `-+` | `+` | `+-` |
| `+-` | `-+` | `-` |
| `+` | `+` | `+-` |

The defining friction case is:

`+-` struck by `+` -> `-+`

The positive strike is consumed reversing the downward momentum at zero; the node does not jump directly to peak positive.

The negative law is its mirror.

## Sparse execution invariant

If a strike leaves a node unchanged, there is no delta and no outgoing propagation.

`state' == state -> no downstream event`

Therefore the Q-kernel does not poll inactive nodes. Work exists only as queued deltas.

## Topology operators

Q-OS v0.1 defines three edge interactions and one structural operation:

- `CASCADE`: transmit the same strike once.
- `DEEPEN`: transmit the same strike twice.
- `CANCEL`: transmit the opposite strike once.
- `SEVER`: remove an edge or deallocate a node; a severed node becomes `Null`.

These are local topology laws, not IF/THEN branches.

## Program model

A program is:

`topology + initial Q-state + external strikes + settlement`

Q-ASM contains no conditional branch opcode in v0.1. Its runtime consequence is produced by the transition algebra and connected geometry.
