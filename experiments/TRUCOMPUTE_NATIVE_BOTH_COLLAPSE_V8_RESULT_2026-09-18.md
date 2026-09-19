# TruCompute Native BOTH Collapse Conformance v8

**Date:** 2026-09-18  
**Status:** MEASURED PASS / CONFORMANCE ONLY  
**Workflow run:** 35417912659

## Canonical semantic correction

The native TruCompute field is intrinsically BOTH.

```
native field node = BOTH
```

Observation of a selected page/character combination produces a transient collapse:

```
BOTH --observe(page, character)--> POS or NEG
```

The collapse does not mutate the underlying node.

NEITHER is restricted to the machine power switch boundary.

## Measured conformance

Direct self-test passed:

```
TRUCOMPUTE_NATIVE_COLLAPSE_V8=PASS
native_field_rest_state=BOTH
neither_scope=POWER_SWITCH_ONLY
observation_collapse=POS_OR_NEG_ONLY
collapse_mutates_native_field=false
conditioned_overlays_coexist=PASS
selector_revisit=PASS
exact_one_true_per_position=PASS
```

A four-page / 64-position fixture was frozen, the source was deleted, and the field was reloaded in a fresh process.

Replay:

```
status=NATIVE_COLLAPSE_EXACT_REPLAY_PASS
recovered_bytes=256
query_frames=824
true_collapses=256
false_collapses=52480
nonboolean_collapses=0
native_field_after_queries=BOTH
selector_revisit=PASS
final_switch_state=NEITHER
```

The recovered SHA-256 matched the deleted source.

The frozen field file hash before and after all observation sweeps was identical:

```
be6de4c27b163e566155c5e585470c3e2f83ded0f033f091f1c623de694a74bf
```

Therefore:

```
frozen_field_immutable=PASS
```

## Scope

The 284-byte fixture artifact is explicitly a conformance serialization and is not a compression result.

This test proves only the required semantics:

- multiple conditioned overlays coexist in one native field;
- page/character selector changes expose different collapses;
- every data node collapses TRUE or FALSE during a valid query;
- observations do not rewrite the native BOTH field;
- source deletion + reload + exact replay succeeds;
- selector order is reversible;
- NEITHER appears only at the OFF switch boundary.

The next compression experiment must replace the explicit conformance overlay serialization with a fixed-capacity native imprint law while preserving these semantics.
