# TruCompression HMC v6 — Canonical Conformance Result

**Date:** 2026-09-18  
**Status:** MEASURED PASS  
**Branch:** `trucompression-canonical-v1-20260918`  
**Workflow run:** 35414517770  
**Implementation:** `trucompression/trucompression_hmc_v6.cpp`.replace("trucompression","trucompression")

## Architecture verified

- 1,000,000 HMC position nodes;
- 1,000 page selectors;
- 206 character selectors;
- 1,206 total controls;
- node-to-node document relations: 0;
- BEGIN once / selector changes / DONE once;
- HMC node outputs: YES / NO / NO SIGNAL;
- fixed relation-lane count selected before source access.

## Fixed-size profile test

Profile: R=2 relation lanes.

Blank artifact:

```
2,002,512 bytes
```

Imprinted artifacts under the same profile remained exactly:

```
2,002,512 bytes
```

## Exact replay tests

A 20,000-byte two-terminal source passed literal strict HMC reconstruction after source removal.

A 1,010,000-byte cross-page source passed exact replay across page selectors 0 and 1.

The replay lifecycle reported:

```
power_cycles=1
run_lifecycle=BEGIN_ONCE__SELECTORS_CHANGE__DONE_ONCE
```

## Fixed-capacity falsification gate

A constructed source requiring bitplane rank 3 was imprinted against the fixed R=2 machine.

Measured result:

```
status=CAPACITY_EXCEEDED
bitplane=0
page=2
required_rank_at_least=3
fixed_relation_lanes=2
machine_growth=0
```

No enlarged artifact or residual was created.

## Interpretation

The TruCompression architecture and exact fixed-rank relation law are operational.

This result does not establish that enwik9 or arbitrary 1 GB data fit a small relation-lane profile. That is now an empirical capacity question rather than an unresolved machine-design question.

For a selected profile R, exact success requires every terminal-code bitplane of the page-by-position field to have rank no greater than R.
