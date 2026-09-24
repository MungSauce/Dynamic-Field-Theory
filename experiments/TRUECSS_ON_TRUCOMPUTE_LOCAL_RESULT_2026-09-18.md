# trueCSS on TruCompute — Local Integration Result 2026-09-18

Status: MEASURED LOCAL INTEGRATION / NONCANONICAL CORPUS

## Scope

This run tests trueCSS executing through the TruCompute runtime boundary.

It does **not** alter trueCSS graph geometry, chronology, page rules, storage accounting, or replay boundary.

The only integration change is that frozen relation cells and the replay resolver execute through TruCompute primitive states and operations.

## Local control corpus

Source bytes: 100,000,000

SHA-256:
`a8d5da587e8bb250e3e8803cc1ce0b16c8b0c00f07cdc941c88ced3eeae9c7d4`

Observed byte alphabet: 205

This local control is not the canonical enwik9 100 MB prefix. Results are therefore local integration evidence only and must not be promoted as canonical trueCSS benchmark evidence.

## TruCompute conformance

Result:

`TRUCOMPUTE_CONFORMANCE=PASS`

Validated:
- NEG + POS through presence composition -> LIVE_ZERO;
- LIVE_ZERO net = 0;
- LIVE_ZERO activity = 2;
- DEAD_STOP remains distinct;
- relative orientation between directional live states is reproducible.

## 10,000-byte freeze / cold replay

Teaching:
- bytes imprinted: 10,000
- runtime: TruCompute
- graph bytes: 1,000,000
- frozen artifact bytes: 1,000,219
- pre-freeze replay: PASS

Source was renamed/removed before cold replay.

Cold replay:
- recovered bytes: 10,000
- result: PASS

Original/recovered SHA-256:

`63a6d84ee66c782f5f5da4d7b9a8f2ae8356e9f1e14ad19643b38e659f03ecc8`

Exact byte comparison: PASS.

## 80,000-byte freeze / cold replay

Teaching:
- bytes imprinted: 80,000
- runtime: TruCompute
- graph bytes: 1,000,000
- frozen artifact bytes: 1,000,219
- pre-freeze replay: PASS

Source was renamed/removed before cold replay.

Cold replay:
- recovered bytes: 80,000
- result: PASS

Original/recovered SHA-256:

`525cb54c6ad600625e679b2188b0f2c622be7135b418bfc06602fde734e24fe4`

Exact byte comparison: PASS.

## Full local 100 MB teaching attempt

Result:

`RELATIONAL_CONTRADICTION`

Exact failure coordinate:
- bytes imprinted before contradiction: 84,265
- bit slot: 7
- page: 0
- key: 84,265
- terminals observed: 205
- runtime: TruCompute

No frozen artifact was promoted from the full 100 MB attempt.

## Interpretation

MEASURED:
- trueCSS can execute through TruCompute rather than directly reading conventional signed integer weights;
- the TruCompute state law supports trueCSS freeze and source-removed exact replay;
- at least 80,000 bytes of this local corpus replay exactly from the fixed one-million-byte graph plus counted metadata;
- the current routing/settling law becomes contradictory at byte 84,265 on this local control.

NOT ESTABLISHED:
- canonical 100 MB success;
- canonical 1 GB success;
- Hutter-valid compression;
- any compression advantage from TruCompute itself.

## DCT conclusion

The failed component is now isolated:

`current trueCSS routing / settling law`

The following remain frozen:
- TruCompute primitive state law;
- trueCSS one-million-node graph;
- four fixed relation slots per node;
- page chronology;
- terminal representation;
- no page bank;
- no residual/correction stream;
- fixed retained graph width;
- cold replay boundary.

The next controlled experiment may improve only the routing/settling law and the minimum matching reader integration required by that law.
