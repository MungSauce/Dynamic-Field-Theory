# TruQ Primed-Zero Chronology v0.2

Physical `0x55` is logical chronology digit zero.

An N-node chronology field is a reversible little-endian base-256 counter with `256^N` distinct positions. Forward increments one position; reverse decrements one.

Rollback succeeds only when the seed path has been inverted and every chronology node is back at `0x55`.

The chronology field provides causal position. It does not by itself encode arbitrary external payload.
