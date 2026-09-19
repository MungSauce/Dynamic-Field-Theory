# trueCSS on TruCompute — Integration Test v1.0

Status: EXPERIMENTAL / DCT-PRESERVING INTEGRATION

## Integration rule

This experiment does not change trueCSS architecture.

Frozen trueCSS:
- exactly 1,000,000 reusable physical graph nodes;
- four fixed relation slots per node;
- one active chronological page condition;
- one-million-position conceptual pages;
- one-target-at-a-time teaching;
- no page bank;
- no residual/correction stream;
- no source-sized selector;
- no chronology-dependent node-count or width growth;
- exact source-isolated cold replay.

Changed component:
- execution substrate only.

The retained graph and reader now execute through TruCompute primitive states and operations.

## Runtime boundary

Each retained relation slot is a TruCompute cell:

```
00 = DEAD_STOP
01 = NEG
10 = POS
11 = LIVE_ZERO
```

trueCSS does not inspect integer weights during replay.

It obtains state only through TruCompute operations:

```
combine(a,b)
net(state)
activity(state)
relative(a,reference)
dead(state)
```

A trueCSS routed pair passes only when:

```
combine(a,b) == LIVE_ZERO
net(LIVE_ZERO) == 0
activity(LIVE_ZERO) == 2
```

The decoded decision is the orientation of `a` relative to the routed reference state, followed by the existing source-independent modifier.

## Training boundary

The relation constraint solver remains temporary teaching machinery.

It is allowed to derive the final relative orientations because it is deleted before replay.

Its output is frozen directly into TruCompute graph cells:
- unused relation slot -> DEAD_STOP;
- active orientation one way -> NEG;
- active orientation the other way -> POS.

No solver state survives into the compressed representation.

## Storage

Four TruCompute cells are represented by four two-bit codes packed into one byte per trueCSS physical node.

Therefore:

```
1,000,000 physical nodes
x 1 retained byte/node
= 1,000,000 graph bytes
```

before header and explicitly counted terminal metadata.

This is unchanged from the immediately preceding graph experiment; only runtime semantics are replaced.

## Acceptance

1. TruCompute self-test passes.
2. trueCSS teaching is attempted without changing routing/graph geometry.
3. If the current relation law contradicts, record exact chronology and do not modify TruCompute or unrelated trueCSS architecture.
4. If teaching survives, freeze the TruCompute graph.
5. Remove source and training solver.
6. Cold replay using only TruCompute operations.
7. Require exact byte and hash identity.

The comparison metric against the preceding trueCSS graph run is contradiction depth under otherwise identical architecture.
