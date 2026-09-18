# HMC Cascade v1

Experimental lossless codec for enwik-style text.

The stored stream is the activation order of byte/character pages, arithmetic-coded under a model that both encoder and decoder learn online from already reconstructed output.

No trained source model is serialized.

Signals mixed into each activation-bit probability:
- global adaptive byte-tree statistics;
- order-1 byte context;
- hashed order-2 and order-3 contexts;
- repeated-context continuation (8-byte match predictor);
- HMC gap/hazard relation: each symbol learns its most repeated recurrence gap and schedules a low-cost activation hint at that due position.

All state is deterministic and reconstructed online. The only source-dependent retained state is the arithmetic-coded path plus the original length in the header.

This is deliberately a first empirical baseline, not a claim of optimality.


---

> **Canonical replication notice (2026-09-18):** This is predecessor/lineage material. Current protocol: [EIS-K32 Formal Replication v1.0](../compression/EIS_K32_FORMAL_REPLICATION_V1.md).
