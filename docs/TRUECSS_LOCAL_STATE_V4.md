# trueCSS 3bit V4 — Local State Law

Status: CANONICAL EXPERIMENTAL STATE LAW

This revision changes only the primitive local state algebra. All established trueCSS geometry, chronology, graph, source-isolation, and cold-replay requirements remain frozen.

## Primitive mapping

The local machine interprets ordinary binary inputs as signed live states:

```
binary 0 = -1
binary 1 = +1
```

The state is represented by two physical presence bits:

```
00 = neither polarity present = DEAD / STOP
01 = negative polarity present = -1 = binary 0
10 = positive polarity present = +1 = binary 1
11 = both polarities present = LIVE ZERO
```

The two zero cases are not equivalent.

```
11:
-1 + +1 = 0
both contributions remain present
=> LIVE ZERO

00:
neither contribution exists
=> DEAD / STOP
```

Therefore:

```
LIVE ZERO != DEAD ZERO
```

even though the live state's arithmetic net is zero.

## Local observation

For every non-dead cell:

```
net = positive_present - negative_present
```

Therefore:

```
01 -> -1
10 -> +1
11 -> 0 with activity
```

`00` does not evaluate as an ordinary arithmetic zero. It is the absence of a live state and is treated by the local machine as STOP.

## Composition

The reference implementation composes contributions by presence.

A negative contribution activates the negative rail.
A positive contribution activates the positive rail.
If both rails become present, the cell becomes LIVE ZERO.

The reference implementation treats repeated presence of the same polarity as idempotent:

```
-1 with -1 -> -1
+1 with +1 -> +1
-1 with +1 -> LIVE ZERO
+1 with -1 -> LIVE ZERO
```

This idempotence is an implementation rule for presence accumulation; it does not change the primitive mapping above.

## DCT-frozen trueCSS integration

Unchanged:
- exactly 1,000,000 reusable physical nodes;
- fixed graph topology;
- one active chronological page condition;
- one-million-position conceptual pages;
- no page bank;
- no page-specific snapshot payload;
- no residual or correction stream;
- no source-sized selector;
- one target taught at a time;
- no chronology-dependent node-count growth;
- no chronology-dependent node-width growth;
- exact source-isolated cold replay.

Changed component:
- primitive relation-cell state law only.

The graph and resolver must now preserve four physical occupancy conditions:
dead, negative-only, positive-only, and balanced-live.

No later optimization may collapse `11` and `00` merely because both can be described with a numerical zero.

## Public binary representation

The public replication surface can describe the machine entirely as an ordinary two-bit state table:

```
00 = stop
01 = state A
10 = state B
11 = active balanced state
```

The reference implementation remains reproducible from those binary codes without requiring the conceptual derivation of the state law.

## Acceptance test

A conforming local runtime must demonstrate at minimum:

```
encode(0) -> 01
encode(1) -> 10
combine(01,10) -> 11
net(11) -> 0
00 -> STOP
```

and must never treat `00` and `11` as interchangeable states.
