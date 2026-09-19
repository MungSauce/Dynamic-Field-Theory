# TruCompute — Primitive State Law v1.0

Status: CANONICAL EXPERIMENTAL RUNTIME LAW

## Role

TruCompute is the local software-defined computational environment used to execute trueCSS under its intended primitive state laws.

trueCSS is the relational graph / chronology architecture.

TruCompute is the machine model underneath it.

The distinction is:

```
TruCompute = computational substrate / runtime law
trueCSS    = architecture executed within that substrate
```

## Primitive mapping

Binary input is interpreted as signed live state:

```
0 = -1
1 = +1
```

The runtime carries two presence rails and therefore four physical occupancy states:

```
00 = neither present      = DEAD / STOP
01 = negative present     = -1 = binary 0
10 = positive present     = +1 = binary 1
11 = both present         = LIVE ZERO
```

## Zero law

TruCompute distinguishes two zero conditions.

Live zero:

```
-1 + +1 = 0
```

Both opposing contributions remain present.

Dead zero:

```
neither -1 nor +1 exists
```

No live computation is present and execution stops for that cell/path.

Therefore:

```
LIVE_ZERO != DEAD_STOP
```

even though LIVE_ZERO has arithmetic net 0.

## Native observables

For a live state:

```
net      = positive_present - negative_present
activity = positive_present + negative_present
```

Thus:

```
01 -> net -1, activity 1
10 -> net +1, activity 1
11 -> net  0, activity 2
00 -> DEAD / STOP
```

Net value alone is insufficient to identify state.

## Presence composition

The baseline runtime composes state by presence:

```
-1 with -1 -> -1
+1 with +1 -> +1
-1 with +1 -> LIVE_ZERO
+1 with -1 -> LIVE_ZERO
```

Repeated same-polarity presence is idempotent in v1.0.

## trueCSS integration boundary

When trueCSS executes inside TruCompute, the following trueCSS architecture remains unchanged:

- exactly 1,000,000 reusable physical graph nodes;
- fixed graph topology;
- one active chronological page condition;
- one-million-position conceptual pages;
- no serialized page bank;
- no residual/correction stream;
- no source-sized selector;
- one target taught at a time;
- no chronology-dependent node-count growth;
- no chronology-dependent node-width growth;
- exact source-isolated cold replay.

TruCompute changes only the primitive computational law used by trueCSS relation cells and resolvers.

## Public binary surface

A public replication may describe TruCompute entirely through the conventional two-bit state table:

```
00 = stop
01 = state A
10 = state B
11 = active balanced state
```

That is sufficient to reproduce the runtime.

The deeper signed interpretation belongs to the internal architecture record.

## Conformance gate

A TruCompute implementation must satisfy:

```
encode(0)              -> 01
encode(1)              -> 10
combine(01,10)         -> 11
net(11)                -> 0
activity(11)           -> 2
00                     -> STOP
00 != 11
```

Any implementation that collapses 00 and 11 is not TruCompute-conformant.
