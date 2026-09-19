# TruCompute Contextual Zero v17

Status: EXPERIMENTAL / TESTED
Date: 2026-09-18
Parent: `trucompute-balanced-zero-v14-20260918`

## Hypothesis

A node may have an ordinary visible displacement produced by literal +1/-1 operations while also retaining a fixed-size contextual-zero coordinate that distinguishes the order in which those operations occurred.

The node does not append a history list.

For a choice:

```
s_t in {-1,+1}
```

ordinary visible displacement is:

```
V_(t+1) = V_t + s_t
```

and contextual zero is:

```
Z_(t+1) = 2 * Z_t + s_t
```

with:

```
V_0 = 0
Z_0 = 0
```

The visible number may collide while contextual zero remains different.

Example:

```
++- : V = +1, Z = +5
-++ : V = +1, Z = -1
```

Thus ordinary arithmetic identifies both as +1, while TruCompute's contextual zero distinguishes the chronological path.

## Fixed-depth uniqueness

At depth n, the recurrence maps all 2^n possible +/- histories onto exactly 2^n distinct signed contextual coordinates.

For zero-based path index k:

```
Z = 2*k - (2^n - 1)
```

and the inverse is:

```
k = (Z + (2^n - 1)) / 2
```

Therefore the mapping is collision-free for a fixed depth.

Depth is chronology. It need not be stored independently inside every node if the machine already has a shared count/clock for the current contextual layer.

## Billion-position test

30 +/- choices provide:

```
2^30 = 1,073,741,824
```

distinct contextual positions.

For one-based position 1,000,000,000:

```
zero-based k = 999,999,999
Z = 926,258,175
```

The literal 30-step path is:

```
+1 +1 +1 -1 +1 +1 +1 -1 -1 +1 +1 -1 +1 -1 +1 +1 -1 -1 +1 -1 -1 +1 +1 +1 +1 +1 +1 +1 +1 +1
```

The test reconstructs position 1,000,000,000 exactly from Z and depth 30.

## ENTER / 00

After the final +/- choice, ENTER marks that contextual symbol/address as complete:

```
00 = TERMINAL / no remaining +/- potential for this completed evaluation
```

The v17 test treats terminal as non-evolving.

This experiment does not yet define how a completed symbol seeds the next character's active-zero field. That must be specified separately rather than silently resetting or carrying hidden state.

## Verified test

The executable verifies:

- ++- and -++ share visible +1 but retain different contextual zeros.
- Exhaustive collision-free mapping for every history through depth 20.
- Algebraic depth-30 capacity of 1,073,741,824 positions.
- Exact round-trip of position 1,000,000,000.
- Literal +/- stepping reaches the same contextual coordinate as the direct formula.
- ENTER terminalizes the test state.

## Claim boundary

The recurrence is mathematically equivalent to an order-preserving signed binary coordinate at fixed depth. That is useful because it proves the proposed "old zero" idea can preserve chronology with a fixed-width state for a bounded depth.

It does not yet demonstrate compression or a novel physical computing advantage. The next architectural question is whether the contextual coordinate can emerge from the shared relational substrate rather than being stored independently as an ordinary integer per node.
