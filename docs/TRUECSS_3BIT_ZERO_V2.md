# trueCSS 3bit V2 — Zero-Field Signed Binary

Status: EXPERIMENTAL / corrected interpretation

V1 tested ordinary balanced ternary relations. That was not the intended mechanism and is retained only as superseded evidence.

## The intended 3bit computation

The substrate begins conceptually at zero.

A physical node may be:

- `0` — neutral/uncommitted field state;
- `-1` — negative polarity;
- `+1` — positive polarity.

The binary information is not "0 versus 1 magnitude." It is orientation around zero.

For one logical bit:

```
logical 0 -> (-1,+1)
logical 1 -> (+1,-1)
```

Both have net field value zero:

```
-1 + +1 = 0
+1 + -1 = 0
```

The bit exists in the **relationship/orientation**, not in added magnitude.

A generic source-independent modifier may invert the orientation without changing the binary meaning after the decoder removes that modifier.

## DCT-frozen trueCSS architecture

Unchanged:
- exactly 1,000,000 reusable physical nodes;
- one active chronological page condition;
- one million source positions per conceptual page;
- one target taught at a time;
- no page bank;
- no residual/correction stream;
- no source-sized selector;
- fixed node count;
- cold replay from frozen substrate only.

Changed:
- node/state algebra and its corresponding generic relational resolver only.

## Terminal representation

Canonical enwik9 has 206 observed terminals.

Eight signed-binary orientation relations provide:

```
2^8 = 256
```

possible terminal codes, enough for all 206 observed terminals.

The source-dependent byte-to-terminal map is serialized and counted.

## Training law

For every terminal bit, the generic chronology/context law chooses two physical nodes and a source-independent orientation modifier.

Training imposes only the required **relative polarity** between those nodes.

Absolute node polarity is not taught. During freeze each connected relational component receives an arbitrary +1 reference orientation and every member becomes +1 or -1 according to the learned relative constraints. Nodes never used by the learned relation field remain 0.

This is why zero is the conceptual substrate: information is learned as departures/orientations relative to a neutral field, not as accumulated positive state.

## Applicability measurement

The 100 MB probe asks whether this zero-field signed-binary relation law survives farther in the same fixed one-million-node trueCSS substrate.

Report:
- bytes absorbed before first relational contradiction;
- bit slot and page/key of contradiction;
- active versus neutral node count if freeze is reached;
- exact pre-freeze generic replay;
- exact cold replay after source removal.

A contradiction falsifies this relation law only. It does not authorize changing the trueCSS architecture.
