# TruCompute Four-Action Native Node Architecture v11

**Date:** 2026-09-18  
**Status:** CANONICAL SEMANTIC CORRECTION / HOST-ENCODING CONFORMANCE

## Core correction

A TruCompute node has four native actions:

1. **NEITHER** — no power / no participation.
2. **NEG** — resolved negative / FALSE.
3. **BOTH** — powered and unresolved; +1 and -1 coexist.
4. **POS** — resolved positive / TRUE.

These are actions of one native node law, not four independent storage cells.

A conventional host may encode the four actions with two binary bits:

\`\`\`
00 -> NEITHER
01 -> NEG
10 -> BOTH
11 -> POS
\`\`\`

The two-bit representation is an emulator transport encoding only. It does not define the TruCompute ontology.

## Native transition law

For one continuously available native node:

\`\`\`
power OFF
    -> NEITHER

power ON, no page-letter observation
    -> BOTH

power ON + page-letter observation below resistance threshold
    -> NEG

power ON + page-letter observation at/above resistance threshold
    -> POS
\`\`\`

Releasing the observation returns the node to the unresolved BOTH action without rewriting its source-dependent imprint.

## Resistive interpretation

The native node is modeled as a condition-sensitive resistor/transfer element.

A selected page-letter combination probes the node's fixed transfer characteristic.

\`\`\`
low effective resistance
    -> NEG / FALSE

enough effective resistance to cross threshold
    -> POS / TRUE
\`\`\`

The same node may therefore take different actions under different observations.

## Architecture constraints

A valid TruCompute implementation must not implement the four actions as expanding response tables.

The node may have a bounded source-dependent imprint chosen before source access, but:

- page response tables are forbidden;
- character response tables are forbidden;
- selector sweeps may not mutate the imprint;
- persistent node state may not grow with the number of observations;
- adding a new page may not silently allocate another page-sized data structure.

## Relationship to BOTH and NEITHER

BOTH and NEITHER are distinct operations.

BOTH means the powered node contains both +1 and -1 possibilities in the unresolved field condition.

NEITHER means the node is outside participation because power is absent.

A numeric zero must never be used to collapse these two meanings into one state.

## Compression gate

This four-action primitive is architectural. It does not by itself establish compression.

A compression claim still requires a fixed-capacity imprint law that survives full source deletion and reconstructs canonical enwik9 exactly without hidden response tables or growing source-dependent state.
