# TruCompute Native-Law / CSS v9 Conformance Result

**Date:** 2026-09-18  
**Status:** MEASURED PASS  
**Branch:** `trucompute-native-law-v4-20260918`

## Correction verified

The runtime now defines TruCompute as an environmental law first.

Nodes retain no observation state.

```
node_retained_state_bytes=0
observations_retained_in_nodes=false
```

The canonical observations NEITHER / NEG / STATELESS_ZERO / POS are produced only by resolving participating contributions under the current environment and selector condition.

## Native-law runtime

Workflow run: **35417169726**

Measured:

```
TRUCOMPUTE_NATIVE_LAW_V4=PASS
node_retained_state_bytes=0
observation_is_law_result=true
selector_changes_observation_without_node_mutation=true
stateless_zero_is_participating_result=true
neither_is_no_participation=true
```

## CSS integration

Workflow run: **35417248908**

Measured:

```
TRUCOMPUTE_NATIVE_LAW_V4=PASS
CSS_NATIVE_OVERLAY_V9=PASS
node_retained_state_bytes=0
observations_retained_in_nodes=false
persistent_page_banks=0
persistent_character_tables=0
```

A 10,000-byte deterministic source was imprinted into the fixed CSS field, the source was deleted, and cold replay reproduced it exactly.

Frozen artifact:

```
css_field_bytes=1000000
frozen_state_bytes=1000248
```

The one-million bytes belong to the serialized CSS relation imprint, not to stored node observations.

## Boundary

This conformance fixes the substrate semantics. It does not establish a full enwik9 TruCompression pass.

The current routing/imprint law remains a separate experimental component and may still contradict on larger corpora. Any such contradiction belongs to the routing/imprint law, not to the native TruCompute node model.
