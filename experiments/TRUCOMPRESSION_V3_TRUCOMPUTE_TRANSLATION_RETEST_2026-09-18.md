# TruCompression v3 — Corrected TruCompute Translation Retest

**Date:** 2026-09-18  
**Status:** MEASURED  
**Workflow run:** 35412604875  
**Job:** 105815024284  
**Branch:** `trucompression-gated-v2-20260918`

## Purpose

Retest the fixed one-million-element TruCompression machine after replacing the ambiguous earlier TruCompute zero semantics with the canonical four-condition translation:

```
NEITHER
NEG
STATELESS_ZERO
POS
```

The test intentionally preserves the existing v1 parity imprint/routing law so the effect of the TruCompute translation change can be isolated.

## State-law conformance

Measured output:

```
TRUCOMPUTE_V3_CONFORMANCE=PASS
TRUCOMPRESSION_FOUR_CONDITION_MACHINE=PASS
physical_nodes=1000000
structurally_live_nodes=1000000
inactive_signal=NEITHER
active_sum_zero=STATELESS_ZERO
retained_polarity_bits=1000000
button_order=0..255
```

The runtime therefore distinguishes:

- `NEITHER`: no participating state, no numeric net;
- `STATELESS_ZERO`: active participation with net sum exactly zero.

All one million physical structures remain present.

## Exact replay test

Input: deterministic 50,000-byte pseudo-random corpus.

Measured freeze:

```
status=FROZEN
source_bytes=50000
physical_nodes=1000000
structurally_live_nodes=1000000
retained_polarity_bits=1000000
machine_payload_bytes=125000
artifact_bytes=125064
```

The source was removed before strict replay.

Measured replay:

```
page=0 control=DONE
status=REPLAY_PASS
recovered_bytes=50000
button_order=0..255
```

The workflow independently compared SHA-256 values and passed.

## Fixed-capacity failure test

Input: deterministic 70,000-byte extension generated with the same seed.

Measured result:

```
status=IMPRINT_CONTRADICTION
byte=59248
page=0
local=59248
bit=4
node_count=1000000
machine_growth=0
```

No artifact was emitted after contradiction.

## Interpretation

The corrected TruCompute translation is verified as operational.

The semantic correction from overloaded zero to four distinct conditions did not alter the capacity boundary of the retained v1 parity routing law.

Therefore:

- TruCompute v3 four-condition state translation: **PASS**;
- fixed one-million-element machine and strict read path: **PASS**;
- exact 50K source-isolated replay: **PASS**;
- no-growth invariant on failure: **PASS**;
- current v1 parity imprint/routing law as a massive-document compression law: **FAILED-LAW**.

The next valid experiment changes the imprint/routing/settling law only. The corrected TruCompute state law and fixed machine geometry remain frozen.
