# TruCompute Temporal Freeze v15

Status: EXPERIMENTAL ARCHITECTURE BRANCH
Date: 2026-09-18
Parent: `trucompute-balanced-zero-v14-20260918`

## Corrected four-state law

TruCompute's powered natural state is the active balanced condition:

```
11 = ACTIVE_ZERO = (+1,-1)
```

It is a live circuit state with participation and net value zero. It is never empty, absent, or off.

The four observable codes are:

| code | state | meaning |
|---|---|---|
| 00 | FROZEN_ZERO | no power; freeze prior active condition in time; isolate from propagation |
| 01 | NEG | powered, + rail inhibited, -1 expressed |
| 10 | POS | powered, - rail inhibited, +1 expressed |
| 11 | ACTIVE_ZERO | powered, both +1 and -1 present, net zero |

Therefore:

```
ACTIVE_ZERO != FROZEN_ZERO
11 = active equilibrium
00 = temporal insulation
```

## Natural condition

Every fresh powered cell begins as:

```
(+1,-1) -> ACTIVE_ZERO
```

Computation is symmetry breaking of that powered equilibrium. POS and NEG are produced by conditionally inhibiting one rail.

## Freeze-in-time semantics

00 does not erase the previous condition and does not mean both rails were destroyed.

Instead:

```
active condition X
-> remove power / freeze
-> externally 00
-> X remains retained but cannot evolve or drive the field
-> restore power
-> X resumes
```

Examples:

```
POS -> freeze -> 00[retains POS] -> thaw -> POS
NEG -> freeze -> 00[retains NEG] -> thaw -> NEG
ACTIVE_ZERO -> freeze -> 00[retains ACTIVE_ZERO] -> thaw -> ACTIVE_ZERO
```

This makes 00 a temporal state rather than a numeric state.

A crucial implementation consequence is that 00 alone is not a complete Markov state. To satisfy "freeze as-is", the physical implementation must retain the immediately preceding active rail condition while power is absent. The software model represents that explicitly as retained rail inhibition plus a power gate.

## Conditional insulation

A frozen cell is a causal boundary while it remains 00:

- it cannot be changed by a new collapse;
- it does not actively drive neighboring cells;
- relation propagation cannot traverse it;
- its retained condition becomes visible again only after power is restored.

This allows already-resolved portions of a relational field to be insulated while another powered region is reused under a different reference condition.

## Current implementation

```
trucompute/trucompute_temporal_freeze_v15.hpp
trucompute/trucompute_temporal_freeze_v15.cpp
```

Build:

```bash
g++ -O2 -std=c++17 -Wall -Wextra -pedantic \
  trucompute/trucompute_temporal_freeze_v15.cpp \
  -o trucompute_temporal_v15
```

Conformance:

```bash
./trucompute_temporal_v15
```

Expected:

```
TRUCOMPUTE_TEMPORAL_FREEZE_CONFORMANCE=PASS
11=ACTIVE_ZERO(+1,-1)_POWERED
10=POS_POWERED
01=NEG_POWERED
00=FROZEN_IN_TIME_UNPOWERED
freeze_preserves_previous_active_condition=PASS
frozen_cell_cannot_evolve=PASS
00_insulates_relation_propagation=PASS
```

## Claim boundary

This is an executable software model of the proposed machine law on conventional binary hardware. It tests the semantics and temporal behavior; it is not yet a physical dual-rail circuit implementation.
