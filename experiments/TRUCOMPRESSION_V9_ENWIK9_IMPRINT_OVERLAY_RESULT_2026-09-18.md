# TruCompression v9 Native-Law CSS — enwik9 Imprint Overlay Test

**Date:** 2026-09-18  
**Status:** MEASURED FAILED-LAW — IMPRINT/OVERLAY ONLY  
**Workflow run:** 35417361181  
**Branch:** `trucompute-native-law-v4-20260918`

## Canonical source

The workflow verified canonical enwik9 before imprint:

- bytes: 1,000,000,000
- MD5: `e206c3450ac99950df65bf70ef61a12d`
- SHA-256: `159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc`

## Substrate conformance

Before the imprint attempt:

```
TRUCOMPUTE_NATIVE_LAW_V4=PASS
CSS_NATIVE_OVERLAY_V9=PASS
node_retained_state_bytes=0
observations_retained_in_nodes=false
persistent_page_banks=0
persistent_character_tables=0
```

Thus this result does not arise from binary-valued node state.

## Imprint result

The full-corpus imprint attempt produced:

```
status=RELATIONAL_CONTRADICTION
bytes_imprinted=57430
bit_slot=5
page=0
key=57430
terminals_seen=100
physical_nodes=1000000
runtime=TruComputeNativeLawV4
node_retained_state_bytes=0
css_field_bytes=1000000
persistent_page_banks=0
persistent_character_tables=0
```

No frozen artifact was produced. Source-isolated replay was therefore not reached.

## Interpretation

The failed component is the current CSS routing/imprint constraint law.

The native TruCompute environmental law remains valid under this test.

The current imprint solver still imposes global relative-orientation equality constraints on reused relation marks. When a later source target requires an incompatible orientation for a relation already constrained by an earlier target, the solver reports contradiction.

That behavior is incompatible with the intended CSS "overwrite without overwrite" mechanism, where differently conditioned overlays may coexist in the same substrate and selector context determines which contribution participates.

Therefore the next revision must change only the imprint/overlay relation law so that:

```
same substrate location
+ different overlay condition
-> coexisting contributions
```

rather than:

```
same retained relation mark
+ incompatible later target
-> contradiction
```

No change to the native-law node model, fixed million-node geometry, page-bank prohibition, character-table prohibition, or source-isolation gate is authorized by this result.
