# TruCompression enwik9 — Source-Isolated Frozen Artifact Measurement

**Date:** 2026-09-18  
**Status:** MEASURED END-TO-END  
**Workflow run:** 35415594694  
**Artifact:** `enwik9.truc`

## Measurement rule

The compression size is the size of the frozen source-dependent artifact that remains after the original source is removed and that can reproduce the source exactly using the generic TruCompression runtime.

This run measures that artifact directly with `stat`; it is not a rank estimate or structural-element proxy.

## Procedure

1. Download canonical enwik9.
2. Verify:
   - size = 1,000,000,000 bytes
   - MD5 = `e206c3450ac99950df65bf70ef61a12d`
3. Freeze the TruCompression machine into `enwik9.truc`.
4. Measure `enwik9.truc` directly.
5. Hash the frozen artifact.
6. Delete both `enwik9` and `enwik9.zip`.
7. Verify both source files are absent.
8. Reconstruct `enwik9.recovered` from `enwik9.truc` only.
9. Verify recovered size and canonical MD5.

## Actual frozen artifact

```
measured_artifact_bytes=1001000512
artifact_sha256=3e2c6f6947ee02b3d3e9ebede95484add9e8da14cf287f9f5928997ce53216d8
```

Freeze report:

```
status=FROZEN_MACHINE
source_bytes=1000000000
terminal_count=206
relation_lanes=1000
artifact_bytes=1001000512
node_to_node_relations=0
page_bank=0
residual_bytes=0
correction_bytes=0
```

## Source-isolated replay

The workflow deleted the source and archive before replay:

```
rm -f enwik9 enwik9.zip
test ! -e enwik9
test ! -e enwik9.zip
```

Replay then produced:

```
status=SOURCE_ISOLATED_REPLAY_PASS
recovered_bytes=1000000000
artifact_bytes=1001000512
power_cycles=1
run_lifecycle=BEGIN_ONCE__SELECTORS_CHANGE__DONE_ONCE
recovered_md5=e206c3450ac99950df65bf70ef61a12d
```

## Actual compression measurement

```
source_bytes=1000000000
artifact_bytes=1001000512
artifact_over_source=1.001000512
source_over_artifact=0.999000488
size_change_bytes=1000512
size_change_percent=0.1000512
```

Therefore this measured TruCompression v6 artifact is 1,000,512 bytes larger than canonical enwik9.

The end-to-end measured compression result is therefore an expansion of 0.1000512%, not compression, for the current v6 relational encoding.

This result replaces structural-density or rank-derived figures as the authoritative file-size measurement for the current implementation.
