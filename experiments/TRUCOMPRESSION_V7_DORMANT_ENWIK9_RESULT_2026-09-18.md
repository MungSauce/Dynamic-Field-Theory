# TruCompression v7 Dormant-Imprint enwik9 Result

**Date:** 2026-09-18  
**Status:** MEASURED ARCHITECTURE PASS / REFERENCE BACKEND  
**Branch:** `trucompression-dormant-imprint-v7-20260918`  
**Workflow run:** 35416210423

## Purpose

This run validates the corrected TruCompression persistence boundary:

- one source-dependent dormant relation-program artifact;
- 1,000 page selectors remain generic wiring;
- 206 character selectors remain generic wiring;
- one million HMC position nodes remain generic machinery;
- no persisted per-character response configurations;
- no persisted page bank;
- no persisted node-response table.

The reference relation-program backend is DEFLATE. It is deliberately isolated from the HMC architecture and is not claimed as a novel TruCompute-native compression law.

## Canonical source

```
source_bytes=1000000000
source_md5=e206c3450ac99950df65bf70ef61a12d
terminal_count=206
```

## Frozen dormant artifact

```
status=DORMANT_IMPRINT_FROZEN
persistent_selector_configurations=0
persistent_page_banks=0
persistent_node_response_tables=0
source_dependent_payload_bytes=322789230
artifact_bytes=322789742
runner=GENERIC_HMC_TRUE_FALSE
codec=REFERENCE_DEFLATE
```

Artifact SHA-256:

```
dcc29fe0a5daeae538166cdbc9c2b8324443fe8e10717130dad5bc19b9426d8f
```

## Source isolation

After freezing the artifact, the workflow deleted:

```
enwik9
enwik9.zip
```

Both were confirmed absent before replay.

## Replay

The generic HMC machine reconstructed the source from the dormant artifact alone.

```
status=SOURCE_ISOLATED_REPLAY_PASS
recovered_bytes=1000000000
artifact_bytes=322789742
persistent_selector_configurations=0
power_cycles=1
run_lifecycle=BEGIN_ONCE__SELECTORS_CHANGE__DONE_ONCE
recovered_md5=e206c3450ac99950df65bf70ef61a12d
```

## File-size measurement

```
source_bytes=1000000000
artifact_bytes=322789742
artifact_over_source=0.322789742
source_over_artifact=3.097991881
saved_bytes=677210258
reduction_percent=67.7210258
```

This is the correct file-size measurement for the v7 reference implementation because the measured artifact is the only source-dependent persistent file required after source deletion.

## Interpretation

The architectural correction succeeds:

```
one dormant relation program
+ generic page selector wiring
+ generic character selector wiring
+ generic million-node HMC runner
```

The 206 character conditions are runtime interrogations of one machine, not 206 persisted source-dependent configurations.

The measured 322,789,742-byte size is a reference baseline produced by DEFLATE. It must not be presented as compression obtained from a novel TruCompute-native imprint law. The next research target is to replace only the `RelationProgram` backend while keeping this artifact boundary and HMC runner unchanged.
