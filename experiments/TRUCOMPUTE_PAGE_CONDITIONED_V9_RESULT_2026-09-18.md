# TruCompute Page-Conditioned Native Node Conformance v9

**Date:** 2026-09-18  
**Status:** MEASURED PASS / CONFORMANCE ONLY  
**Workflow run:** 35418001295

## Corrected native law

Each data node is continuously powered.

Its unresolved field condition is BOTH, but the selected page-letter pair determines its resolved output by design:

```
N_i(P_q, C_c) = POS  if page q has character c at position i
              = NEG  otherwise
```

For every valid page-letter query, every data node resolves TRUE or FALSE. No active data-node query produces NEITHER.

NEITHER remains restricted to the machine OFF switch boundary.

Page and character activators contribute only by being selected as the current condition. Unselected activators are not part of the operation and do not emit a node state.

## Direct conformance

The self-test proved:

```
TRUCOMPUTE_PAGE_CONDITIONED_V9=PASS
data_nodes_continuously_powered=PASS
unresolved_field_condition=BOTH
resolution_condition=PAGE_PLUS_CHARACTER
every_page_has_true_or_false_for_every_character=PASS
same_node_changes_output_by_condition=PASS
unselected_activators_not_part_of_operation=PASS
selector_revisit=PASS
exact_one_true_letter_per_page_position=PASS
neither_scope=POWER_SWITCH_ONLY
```

A single data node was explicitly tested across two pages with different true letters. Changing only page or character changed the node's resolved output as required while the imprint remained unchanged.

## Source-isolated exact replay

A four-page, 64-position, 206-symbol-safe fixture was imprinted into one field.

The source was then deleted before replay.

Measured replay:

```
status=PAGE_CONDITIONED_EXACT_REPLAY_PASS
recovered_bytes=256
query_frames=824
positive_responses=256
negative_responses=52480
nonboolean_responses=0
data_nodes_continuously_powered=PASS
selector_revisit=PASS
final_switch_state=NEITHER
```

The recovered SHA-256 matched the deleted source.

The frozen field hash was unchanged before and after all observations:

```
bc4f2d08921651049e709515eed9d6d08206110c328a4fbc0a4c9465daffcc0d
```

Therefore:

```
field_imprint_unchanged_by_observation=PASS
```

## Scope

The 284-byte field file is a conformance-only serialization of the explicit page-conditioned truth fixture. It is not a compression result.

This test establishes the node/selector semantics that the fixed-capacity TruCompression imprint law must preserve.
