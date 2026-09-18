# TRUECSS ACT206 V1 — Canonical enwik9 50 MB probe

Status: OBSERVED / CANONICAL SOURCE VERIFIED / ALL THREE V1 LAWS FAILED BEFORE FREEZE

Canonical source:
- bytes: 1,000,000,000
- MD5: e206c3450ac99950df65bf70ef61a12d
- SHA-256: 159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc
- verification: PASS before training

State budget:
- total ceiling: 50,000,000 bytes
- reserved header/accounting: 512 bytes
- ACT206 carrier bytes: 49,999,488
- terminal states: 206
- one-target-at-a-time imprint: enforced
- page-bank bytes: 0
- residual bytes: 0
- correction bytes: 0

Results:

| Law | Exact bytes before first contradiction | Page | Key | Terminals seen |
|---|---:|---:|---:|---:|
| count | 20,915,345 | 20 | 915,345 | 202 |
| page_recursive | 15,121,190 | 15 | 121,190 | 201 |
| context_recursive | 24,744,565 | 24 | 744,565 | 203 |

Best V1 law: context_recursive at 24,744,565 exact sequential imprints.

Interpretation:
- This is not a TRUECSS PASS.
- No V1 law reached freeze, source removal, or cold replay.
- The tested 50 MB shared ACT206 field can absorb tens of millions of exact chronological constraints without page banks or correction streams.
- These specific pair-relation routing laws are falsified for full canonical enwik9.
- V2 should increase conditional relational expressivity per retained carrier without increasing the 50 MB source-dependent ceiling.

Workflow run: 35303727964


---

> **Canonical replication notice (2026-09-18):** This is predecessor/lineage material. Current protocol: [EIS-K32 Formal Replication v1.0](../compression/EIS_K32_FORMAL_REPLICATION_V1.md).
