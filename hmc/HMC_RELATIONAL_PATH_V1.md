# HMC-RP v1 — Relational Hangman Compression as a Puzzle Path

## Core representation

The corpus is treated as one shared board of N slots and A symbol-pages (206 for canonical enwik9).
Each slot belongs to exactly one page.

The stored object is NOT:
- a bitmap per page,
- a list of absolute positions,
- a full gap list,
- a residual copy of the source.

The stored object is an information path through a deterministic reconstruction puzzle.

## Initial puzzle state

Source-dependent initial clues are counted as part of the path:
- corpus length N,
- alphabet membership / canonical symbol ids,
- total occurrence count C[s] for every present symbol.

These clues define the total occupancy required for every page but not chronology.

One page may become implicit when all other pages are resolved.

## Generic solver

A fixed, source-independent solver operates on:
- unresolved slot set V,
- remaining per-page counts C_remaining[s],
- already resolved occupancy,
- relations derivable from resolved state.

At every step it forms candidate next assignments and scores them using fixed relational projectors such as:
1. transition/context relations between nearby resolved slots;
2. same-page gap recurrence and gap-delta recurrence;
3. repeated local occupancy motifs;
4. cross-page shifted relations;
5. periodic / stride relations;
6. deterministic count/vacancy constraints.

No source-derived rule table is free. If the encoder introduces a source-specific rule, dictionary, seed, threshold, ordering, or exception, it is serialized in the path and counted.

## Information path

When the generic solver can prove exactly one assignment, the assignment costs 0 path bits.
When k alternatives remain, the encoder records only the branch required to choose the source-consistent alternative, using a reversible entropy/range code.

The decoder:
1. reconstructs the same puzzle state,
2. runs the same generic solver,
3. follows the stored branch only at ambiguous points,
4. updates all relational state,
5. repeats until every slot is assigned.

Thus the path is the unresolved information left after relational inference.

## Exactness invariant

Decode(path) must yield exactly:
- 1,000,000,000 bytes for canonical enwik9
- SHA-256 159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc

Source removal / cold replay is mandatory.

## Accounting

Complete retained bytes =
- path header and counts
- branch stream
- source-specific relation declarations, if any
- exceptions
- custom decoder bytes when using complete-engineering accounting

Logical 206-page board state is runtime formation and is not serialized as 206 bitmaps.

## Benchmark metric

Primary:
    path_bits / source_bits

Secondary:
    forced_slots / total_slots
    branch_events
    mean candidates per branch
    bits per unresolved slot
    complete bytes after optional generic outer compression

## Interpretation

HMC-RP is successful only if relational inference removes enough ambiguity that the retained branch path is smaller than direct baselines.
The puzzle framing itself does not receive compression credit; only exact measured path length does.


---

> **Canonical replication notice (2026-09-18):** This is predecessor/lineage material. Current protocol: [EIS-K32 Formal Replication v1.0](../compression/EIS_K32_FORMAL_REPLICATION_V1.md).
