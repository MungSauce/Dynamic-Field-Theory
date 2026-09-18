# HMC Lock-and-Key Count Solver v1

This benchmark measures the literal-key architecture directly.

For context order k, the lock stores the exact multiset of remaining next-symbol transition counts for every observed k-byte context, plus the first k seed bytes and source length.

During decode:
1. The current context is known from reconstructed output.
2. If exactly one outgoing character page still has a positive remaining count, it must fire and costs zero key characters.
3. If two or more pages remain possible, the decoder knows it is ambiguous and consumes exactly one literal byte from the key stream.
4. The chosen transition count is decremented and reconstruction continues.

Thus key positions are not stored. The decoder itself determines every key-consumption point. Exact replay is possible from lock + key alone.

All source-derived transition counts are serialized and counted as lock bytes.


---

> **Canonical replication notice (2026-09-18):** This is predecessor/lineage material. Current protocol: [EIS-K32 Formal Replication v1.0](../compression/EIS_K32_FORMAL_REPLICATION_V1.md).
