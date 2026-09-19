# TruCompute One-Hot Native Field v11 — Conformance Result

**Date:** 2026-09-18  
**Status:** MEASURED PASS / FIXED-CAPACITY REFERENCE LAW  
**Workflow run:** 35419189418

## Corrected control law

Each control bank is one-hot.

```
page bank      = 1 TRUE + 999 FALSE
character bank = 1 TRUE + 205 FALSE
```

Pressing one button sets that button TRUE and forces every peer in the same bank FALSE.

The button banks are generic runtime control and are not persisted in the source artifact.

## Data-node law

Every data node is continuously powered.

For the currently TRUE page button and TRUE character button, every data node emits exactly:

```
TRUE
or
FALSE
```

No active data-node observation has a third output.

The v11 reference node is a fixed-width condition-sensitive resistor/transfer element. Its source-dependent state is an 8-channel imprint vector. There are no page response tables, character response tables, residual streams, or dynamically growing channels.

## Measured conformance

```
TRUCOMPUTE_ONEHOT_FIELD_V11=PASS
page_button_true_count=1
page_button_false_count=999
character_button_true_count=1
character_button_false_count=205
button_press_forces_peer_labels_false=PASS
data_outputs_nonboolean=0
```

## Fixed artifact geometry

A 4-page source and an 8-page source were both represented with the same fixed 64-node, 8-channel machine.

Both frozen artifacts measured:

```
artifact_bytes=1056
```

Therefore:

```
artifact_size_independent_of_imprinted_page_count=PASS
```

Both sources were deleted before replay, and both cold replays passed exactly.

## Capacity refusal

A ninth page was deliberately changed so it could not be represented by the fixed 8-channel transfer law.

The machine correctly refused to grow:

```
status=CAPACITY_EXCEEDED
node=0
page=8
expected=9
predicted=8
fixed_channels=8
artifact_created=false
```

This demonstrates that the current architecture fails closed instead of silently creating page storage.

## Scope

The 8-channel polynomial transfer function is a bounded reference law, not a claim that this specific law is the final TruCompute imprint mechanism.

The architecture now enforced is:

```
fixed one-hot page buttons
+ fixed one-hot character buttons
+ continuously powered data nodes
+ fixed-width source imprint per node
+ condition-dependent TRUE/FALSE response
+ no per-page or per-character response storage
+ no growth on capacity failure
```

The next experiment is to search for a stronger native transfer/imprint law under these same invariants.
