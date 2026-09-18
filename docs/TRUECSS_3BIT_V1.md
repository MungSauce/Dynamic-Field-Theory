# trueCSS 3bit V1

Status: EXPERIMENTAL / applicability test

## Name

The project label is **trueCSS 3bit**.

Technically the node algebra has three balanced states:

```
-1, 0, +1
```

The name is retained as the project/design name.

## DCT-preserved architecture

Frozen:
- exactly 1,000,000 reusable physical nodes;
- one active chronological page condition q;
- page length = 1,000,000 source positions;
- one target exposed at a time;
- no stored page bank;
- no residual/correction stream;
- no source-sized position selector;
- no node-count growth;
- no dynamic node-width growth;
- cold replay from frozen substrate only.

Changed component:
- node/state algebra only.

## 3bit computation

Each physical node has one of three balanced states:

```
-1, 0, +1
```

The computation intentionally uses polarity and cancellation. Generic relations have the form:

```
ca*A + cb*B + salt = target_trit  (mod 3)
```

where ca and cb are +1 or -1.

Thus +1 and -1 can cancel to the zero relation while preserving relational orientation through which nodes and signs participated.

A terminal is represented by five ternary relational decisions:

```
3^5 = 243 >= 206
```

so all 206 trueCSS terminal states fit without changing the output alphabet.

## What this test asks

Does the balanced three-state relational algebra let the fixed one-million-node trueCSS substrate absorb useful chronology while preserving exact replay?

It does **not** change page geometry, add a new compressor, add a residual stream, or reinterpret CSS as EIS/MSF.

## Measurement

The probe records:
- exact bytes absorbed before first contradiction;
- page/key/trit slot of contradiction;
- terminal count;
- fixed physical node count;
- if the corpus span survives: frozen state bytes and exact cold replay.

Frozen node states are packed with two physical bits per node for the executable artifact (four node trits per byte). This is serialization accounting only; the logical computation remains balanced three-state.

The applicability comparison is primarily contradiction depth and exact replay under unchanged architecture, not a claim that 3-state nodes carry three binary bits.
