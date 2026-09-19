# TruCompute Native Resistive Field v10 — Architecture Result

**Date:** 2026-09-18  
**Status:** MEASURED ARCHITECTURE PASS / REFERENCE TRANSFER LAW  
**Branch:** `trucompute-native-resistor-field-v10-20260918`  
**Workflow run:** 35418465914

## Native architecture

The v10 field is built from continuously powered condition-sensitive resistor nodes.

Each node has:

```
unresolved condition = BOTH
fixed source-dependent imprint width = 8 channels
page response table = false
character response table = false
```

The fixed machine provides:

- 1,000 page activator identities;
- 206 character activator identities;
- a generic page-driven transfer law;
- a generic resistance threshold.

For the reference law:

```
resonance_i(page) =
    a0 + a1*page + ... + a7*page^7
    mod 65521

R_i(page,character) =
    1000 if resonance_i(page) == character
    0 otherwise

R >= 500 -> POS
R < 500  -> NEG
```

The polynomial is a reference transfer law, not a claim that this is the final TruCompute physical law.

## Primitive conformance

```
TRUCOMPUTE_NATIVE_RESISTOR_V10=PASS
node_native_primitive=CONDITION_SENSITIVE_RESISTOR
node_unresolved_condition=BOTH
negative_resistance_units=0
positive_resistance_units=1000
flip_threshold_units=500
page_response_table=false
character_response_table=false
fixed_imprint_channels=8
observation_mutates_imprint=false
```

## Fixed-size artifact proof

A 64-node field with eight fixed 16-bit imprint coefficients per node was tested.

Four-page source:

```
source_bytes=256
pages=4
node_count=64
fixed_imprint_channels=8
coefficient_bytes_per_node=16
residual_bytes=0
prefreeze_exact_replay=PASS
artifact_bytes=1056
```

Eight-page source:

```
source_bytes=512
pages=8
node_count=64
fixed_imprint_channels=8
coefficient_bytes_per_node=16
residual_bytes=0
prefreeze_exact_replay=PASS
artifact_bytes=1056
```

Therefore:

```
artifact_size_independent_of_imprinted_page_count=PASS
```

within the fixed law's representational capacity.

Both sources were deleted before cold replay. Both recovered files matched their original SHA-256 hashes exactly.

## Bounded-capacity falsification

A ninth page was deliberately changed so it could not be represented by the already-fixed eight-channel transfer law.

Measured:

```
status=CAPACITY_EXCEEDED
node=0
page=8
expected=9
predicted=8
channels=8
node_count=64
artifact_created=false
```

The runtime did not:

- grow the node;
- add a ninth coefficient;
- allocate a page table;
- allocate a character table;
- write residual bytes;
- invoke an external codec.

This is the required bounded behavior.

## Architectural conclusion

v10 is the first implementation in this lineage where the data node itself is a fixed condition-sensitive transfer element rather than a conventional node carrying page-conditioned records.

The source-dependent object is one fixed imprint vector per node.

The selected page changes the node's effective resonance. The selected character probes that resonance. The same node therefore resolves differently under different page-character observations without persistent-state growth.

## Remaining research question

The architecture is validated; the current polynomial transfer law is only a baseline.

The next task is to search for a stronger fixed-width native transfer law that can represent substantially more page-conditioned source structure, ultimately testing the full 1,000-page canonical enwik9 corpus under the same no-growth rules.
