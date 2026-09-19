# TruCompute Shared Context Field v18

Status: EXPERIMENTAL / TESTED
Date: 2026-09-18
Parent: `trucompute-contextual-zero-v17-20260918`

## Purpose

Test the contextual-zero recurrence across a large shared field rather than on one node.

A single global +/- event is presented at each contextual layer. Every physical node experiences exactly one local +1 or -1 contribution according to its fixed relation to that global event.

No node stores an explicit history list.

## Per-node state

For node i:

```
V_i(t+1) = V_i(t) + s_i(t)
Z_i(t+1) = 2*Z_i(t) + s_i(t)
```

where:

```
s_i(t) in {-1,+1}
```

V is the ordinary visible displacement.

Z is the recursively redefined contextual zero.

## Shared event and fixed relation

The same global sign g(t) reaches the whole field.

Each node has a fixed structural relation r(i,t):

```
s_i(t) = g(t) * r(i,t)
```

The current experiment derives r(i,t) from the physical node identity bit at layer t. Therefore no per-history relation table is allocated.

At depth 30, the fixed relation vectors span:

```
2^30 = 1,073,741,824
```

possible distinct node relation patterns.

## Million-node experiment

The implementation evolves 1,000,000 nodes through the same 30 global events.

Verified:

```
field_nodes_tested=1000000
context_depth=30
analytical_capacity=1073741824
million_node_context_collisions=0
node_inverse_roundtrip=PASS
visible_range=[-24,16]
```

An observed ordinary arithmetic collision:

```
node 1 -> visible -10
node 2 -> visible -10
```

while their contextual zeros differ:

```
node 1 -> Z = -926258173
node 2 -> Z = -926258171
```

Thus final scalar displacement alone loses identity, while the recursively carried zero context preserves it.

## Why the full depth is injective

The local sign bit is the equality/XNOR relation between the fixed global bit and the physical-node relation bit.

Applying XNOR with one fixed global vector is a permutation over all 30-bit node vectors.

The contextual-zero recurrence is itself a bijection between a fixed-depth +/- path and its signed contextual coordinate.

Therefore the composition is injective over all 2^30 possible node identities.

The implementation exhaustively checks the first 1,000,000 physical nodes and also performs inverse round trips.

## Storage boundary

This version does not store the literal +/- history.

However it still maintains one evolving contextual scalar Z per node, and the relation law uses physical node identity.

Therefore v18 proves shared-field chronology discrimination, not compression and not yet address elimination.

The next falsification target is stricter:

> Can fixed relational topology itself generate/retain the contextual distinctions without deriving them from an explicit numeric node index or giving every node its own ordinary address-equivalent contextual register?

That is the point at which TruCompute would stop being an unusual signed-binary address representation and become a genuinely different storage/computation substrate.

## Source

```
trucompute/trucompute_shared_context_field_v18.cpp
```
