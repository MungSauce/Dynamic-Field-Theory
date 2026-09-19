# TruCompute Native Field Nodes v5

**Date:** 2026-09-18  
**Status:** CANONICAL CORRECTION / EXPERIMENTAL NODE SET

## Correction

The TruCompute field is not an external environment acting on generic nodes.

The field is made of TruCompute-native node types.

```
Field = composition(native TruCompute nodes)
```

Each node type defines its own lawful participation behavior.

The field observation is the settlement of the contributions produced by the participating native nodes under the current selector.

## Native node types

Initial reference set:

- `PositiveNode` — natively contributes positive participation.
- `NegativeNode` — natively contributes negative participation.
- `BalancedNode` — natively contains opposing participation and therefore resolves through coexistence, not through a stored zero code.
- `NeitherNode` — structurally present but contributes nothing.
- `FilteredPositiveNode` — positive native carrier visible only under its selector condition.
- `FilteredNegativeNode` — negative native carrier visible only under its selector condition.

These are separate semantic node types. They are not one binary storage cell whose bit pattern is later renamed.

## Field law

A field observation proceeds as:

```
selector
-> each native node decides participation
-> participating native contributions coexist
-> field settles
-> NEITHER / NEG / STATELESS_ZERO / POS
```

The observation is not stored in the node.

## CSS implication

Selector-sensitive native carriers provide the correct primitive for the CSS "different colours / tinted glasses" model.

Two carriers can occupy the same field population while responding to different selectors:

```
same field
+ selector A
-> contribution A visible

same field
+ selector B
-> contribution B visible
```

Changing the selector does not rewrite either carrier.

This is the primitive required before a real overwrite-without-overwrite imprint law can be attempted.

## Current boundary

This revision fixes node ontology and field composition.

It does not yet claim that the full 1 GB corpus can be represented by a fixed-size population of these nodes. A compression claim still requires a source-dependent imprint law whose retained native-node population does not grow with corpus chronology, followed by source deletion and exact cold replay.
