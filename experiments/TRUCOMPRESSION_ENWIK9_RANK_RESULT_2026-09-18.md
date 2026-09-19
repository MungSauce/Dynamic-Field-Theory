# TruCompression v6 — Canonical enwik9 Rank Measurement

**Date:** 2026-09-18  
**Status:** MEASURED / FAILED-LAW FOR ENWIK9 COMPRESSION  
**Workflow run:** 35414901907  
**Source:** canonical enwik9  
**Source bytes:** 1,000,000,000  
**Source MD5:** e206c3450ac99950df65bf70ef61a12d

## Source validation

The workflow downloaded the canonical enwik9 ZIP from Matt Mahoney's benchmark host and verified:

```
source_bytes=1000000000
terminal_count=206
page_count=1000
page_size=1000000
```

The MD5 gate matched the canonical value before measurement.

## Exact v6 relation ranks

Using the canonical v6 ascending-byte terminal-code assignment:

```
rank_bit0=1000
rank_bit1=1000
rank_bit2=1000
rank_bit3=1000
rank_bit4=1000
rank_bit5=1000
rank_bit6=1000
rank_bit7=1000
```

All eight page-by-position binary fields reached the maximum possible rank of 1000.

The probe established full rank after examining 743,216 of the 1,000,000 position columns; once all eight ranks reached 1000, no later column could increase them.

## Minimum exact v6 machine

The v6 law requires:

```
minimum_relation_lanes=1000
minimum_payload_bytes=1001000000
minimum_artifact_bytes=1001000512
source_ratio=1.001
```

Therefore the smallest exact fixed-rank v6 machine for canonical enwik9 is:

```
1,001,000,512 bytes
```

which is 1,000,512 bytes larger than the original 1,000,000,000-byte source.

## Interpretation

The TruCompute/HMC machine architecture remains operational and independently conformed.

The specific v6 relation law:

```
M_b(q,i) = XOR_r [P_b(q,r) AND N_b(r,i)]
```

does not compress enwik9. The canonical corpus has full page-rank in every encoded bitplane under this factorization, forcing R=1000.

Therefore:

- HMC selector architecture: CANONICAL / PASS;
- continuous-run lifecycle: PASS;
- source-isolated exact replay mechanism: PASS;
- fixed-rank GF(2) relation law: FAILED-LAW for enwik9 compression;
- enwik9 compression claim: FAIL;
- machine growth or hidden residual: none.

This failure is preserved as a constraint on the next relation law. Any successor must exploit structure not captured by global linear page-position factorization while preserving the fixed HMC interface and complete source-dependent accounting.
