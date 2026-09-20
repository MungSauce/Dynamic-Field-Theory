# Senary Machine Environment v0.4

Status: substrate dependency for Qompression. This module comes before any compression grammar or seed experiment.

## Semantic vocabulary

The machine-level semantic vocabulary is:

`S6 = { Ø, -, -+, +-, +, Π0 }`.

This is **not** a six-valued local digit alphabet.

Local primitive context has five possibilities:

`{ Ø, -, -+, +-, + }`.

Where:

- `Ø` = structural absence / unallocated primitive.
- `-` = peak negative tension.
- `-+` = directed zero toward positive; primed-zero local formation.
- `+-` = directed zero toward negative; decaying-zero local formation.
- `+` = peak positive tension.
- `Π0` = derived whole-field Primed Zero condition.

`Π0` holds iff at least one primitive is allocated and **every allocated primitive is `-+`**. An entirely unallocated field is `Ø`, not `Π0`.

This reconciles the older v0.2 quinary-context law with the senary machine vocabulary: the sixth semantic item is a global field condition.

## Canonical host codes

The proven Q-OS host encodings remain unchanged:

| Q formation | code |
|---|---:|
| `-` | `00` |
| `-+` | `01` |
| `+-` | `10` |
| `+` | `11` |

Structural absence is outside the active two-bit alphabet.

## Primitive directed-zero reflex

Encoding direction:

`- -> -+`

`+ -> +-`

Primitive reversal direction:

`-+ -> -`

`+- -> +`

No deeper chronology is inferred from one primitive.

## Whole-number machine snapshot

A sparse machine snapshot is represented canonically by whole numbers:

`(allocation_mask, payload)`.

The machine capacity is fixed by the executable/environment.

- `allocation_mask` is a nonnegative integer; bit `i` marks allocation of primitive `i`.
- `payload` is a nonnegative integer containing active states of allocated primitives in ascending address order, base 4.
- For terminal-state packing only, digit order is `(-+, -, +-, +) -> (0,1,2,3)` so an all-primed-zero active field has `payload = 0`.

Therefore:

- `allocation_mask = 0` means structural VOID.
- `allocation_mask != 0 and payload = 0` means Primed Zero `Π0`.

This snapshot is a substrate primitive, not yet the final Qompression seed contract.

## Dependency rule

Qompression layers must depend on this environment rather than redefining Q-state meaning. A later reversible field/coupling law may transform these states, but it must preserve:

1. structural VOID vs allocated state,
2. the two directed zero formations,
3. exact Q-OS host encodings,
4. global `Π0` semantics,
5. whole-number canonical snapshot reversibility.
