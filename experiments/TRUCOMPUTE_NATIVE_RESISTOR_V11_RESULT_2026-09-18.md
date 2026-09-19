# TruCompute Native Resistor Field v11 Result

**Date:** 2026-09-18  
**Status:** MEASURED ARCHITECTURE PASS / REFERENCE TRANSFER LAW  
**Workflow run:** 35420106351

## Native architecture

The tested machine uses an uninterrupted TruCompute field:

```
NEG / BOTH / POS
```

There is no OFF state, NEITHER state, reset-to-zero phase, or recompute-from-zero step.

Each data node is a continuously present condition-sensitive resistor element.

Its native substrate remains BOTH. A page-letter probe changes effective resistance and resolves the node directly to POS or NEG.

## Fixed generic controls

The runtime contains:

- 1,000 fixed page activators;
- 206 fixed character activators;
- fixed native data nodes;
- one generic resistance/threshold observation law.

No page or character response table exists in a data node.

## Fixed-width source imprint

Reference v11 uses exactly 8 imprint channels per node.

```
fixed_imprint_channels=8
coefficient_bytes_per_node=16
page_response_table=false
character_response_table=false
residual_bytes=0
```

The channel width is fixed before source access.

## Measured fixed-size behavior

Four-page test:

```
status=NATIVE_RESISTOR_FIELD_FROZEN
source_bytes=256
pages=4
node_count=64
artifact_bytes=1052
prefreeze_exact_replay=PASS
```

Eight-page test:

```
status=NATIVE_RESISTOR_FIELD_FROZEN
source_bytes=512
pages=8
node_count=64
artifact_bytes=1052
prefreeze_exact_replay=PASS
```

Thus:

```
artifact_size_independent_of_page_count=PASS
```

within the fixed reference law's representational capacity.

## Source-isolated replay

Both source files were deleted before replay.

Four-page replay:

```
status=NATIVE_RESISTOR_EXACT_REPLAY_PASS
recovered_bytes=256
pages=4
artifact_bytes=1052
machine_off_state=false
reset_between_probes=false
```

Eight-page replay:

```
status=NATIVE_RESISTOR_EXACT_REPLAY_PASS
recovered_bytes=512
pages=8
artifact_bytes=1052
machine_off_state=false
reset_between_probes=false
```

Both recovered SHA-256 hashes matched their deleted source fixtures.

## Fixed-capacity failure gate

A ninth page was deliberately changed so that it could not be represented by the fixed 8-channel transfer law.

Measured:

```
status=CAPACITY_EXCEEDED
node=0
page=8
expected=9
predicted=8
channels=8
artifact_created=false
```

The implementation did not add channels, page records, exceptions, residuals, or a fallback codec.

## Interpretation

This is the first executable architecture in the project where:

- nodes are native condition-sensitive resistor elements rather than response-table holders;
- the field is uninterrupted;
- selector changes do not reset the machine;
- source-dependent state per node has a fixed upper bound;
- additional representable page conditions do not grow the frozen artifact;
- capacity failure is explicit rather than hidden by conventional storage growth.

The polynomial resonance function is a bounded **reference transfer law**, not a claim that this particular function can encode canonical enwik9.

The next research problem is the transfer/imprint law itself: find a fixed-capacity native resistance function with substantially greater conditioned-page capacity while preserving all v11 invariants.
