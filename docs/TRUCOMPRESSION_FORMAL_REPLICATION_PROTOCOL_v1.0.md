# TruCompression — Formal Replication Protocol v1.0

**Date:** 2026-09-18  
**Project:** Epsilonic / TruCompute / TruCompression  
**Status:** CANONICAL MACHINE SPECIFICATION / EXPERIMENTAL IMPRINT LAW

## 1. Definition

TruCompression is a fixed-capacity storage experiment implemented as a native program of the TruCompute split-state runtime.

The compressed artifact is the tuned machine itself.

The machine geometry is fixed before the source is opened. Imprinting may only tune state already present in the machine. If an imprint law requires another node, wider node state, another page object, a residual stream, a source-sized index, or any other source-dependent retained storage outside the frozen machine, that law fails TruCompression.

## 2. Layer boundary

The stack is:

```
physical host CPU
    -> TruCompute runtime law
        -> fixed TruCompression machine
            -> imprint/read program
```

TruCompute is the runtime dependency. TruCompression is the program/machine constructed inside it.

The current reusable dependency is:

`trucompute/trucompute_runtime_v1.hpp`

The current machine implementation is:

`trucompression/trucompression_native_v1.cpp`

Earlier trueCSS four-slot graph experiments remain preserved as historical lineage. They are not this machine.

## 3. Fixed physical machine

Before any document is seen:

- physical TruCompute elements: exactly **1,000,000**;
- retained polarity per element: exactly **one bit**;
- retained machine payload: exactly **1,000,000 bits = 125,000 bytes**;
- conceptual page span: **1,000,000 source positions**;
- generic character buttons: **256**, fixed order `0..255`;
- node count never grows;
- node width never grows.

The 256-button surface is source independent. Canonical enwik9 historically uses only 206 byte values; unused buttons simply return FALSE everywhere for a correctly tuned page.

## 4. Split-state semantics

A retained storage element holds only one polarity bit. The opposite polarity is implied.

At interrogation time a result is expressed through TruCompute states:

- `POS`: TRUE / selected character is present at this position;
- `NEG`: FALSE / selected character is not present at this position;
- `LIVE_ZERO`: active continuation with no resolved payload answer;
- `DEAD_STOP`: halt/disconnect condition.

`LIVE_ZERO` and `DEAD_STOP` are runtime behaviors, not extra stored payload values.

## 5. Page voltage

For chronology position `t`:

```
page  q = floor(t / 1,000,000)
local k = t mod 1,000,000
```

A page is not stored.

Applying page voltage `q` changes the expressed state/routing of the same physical million-element machine through a fixed source-independent page law.

No page bank, page embedding table, page snapshot, or per-page million-node copy is permitted.

## 6. Character voltage

A character button asks exactly one question at one local position:

```
Is highlighted byte c present at position k under page voltage q?
```

The response must be structurally binary:

```
POS = TRUE
NEG = FALSE
```

For every valid page and local position, exactly one character button must return POS.

Formally:

```
sum_c [Press(q,k,c) == POS] = 1
```

This is a replay invariant.

## 7. Fixed read order

Read/decode order is frozen and source independent.

For every page:

1. apply page voltage `q`;
2. push character buttons in ascending order `0,1,...,255`;
3. collect the million-position TRUE/FALSE response pattern for each button;
4. require exactly one TRUE character for each valid position;
5. after the final character button, emit `NEXT_DATASET` if another page remains;
6. otherwise emit `DONE`.

No source-derived button ordering is permitted.

## 8. Write/read separation

### Write / imprint time

The source is present.

The imprint law may inspect the current source target and may alter only the existing internal tuning of the fixed million-element machine.

Temporary solver state is allowed only during imprinting and must be destroyed before replay.

### Read / replay time

The source is absent.

Replay receives only:

- the frozen machine artifact;
- generic TruCompute runtime/program code;
- the counted source length stored in the artifact header.

Replay must not receive:

- the original source;
- an imprint solver;
- source-derived page files;
- source-derived position lists;
- residual/correction streams;
- hidden dictionaries;
- network access or other side information.

Write and read therefore occur at different times and use different permissions.

## 9. Current v1 imprint law

The current v1 law is deliberately simple and falsifiable.

For each source bit, a generic deterministic route selects a pair of the fixed one million elements. The page voltage contributes a generic phase. During write time a temporary parity solver asks the two retained polarities to satisfy the requested source bit.

After imprinting, the solver is discarded.

The frozen artifact contains only:

```
64-byte header
+ 125,000 bytes of machine polarity
= 125,064 bytes
```

The route topology is generic code and does not grow with the source.

If a new relation contradicts already-imprinted relations, v1 reports `IMPRINT_CONTRADICTION` and stops. It may not grow the machine.

## 10. Artifact accounting

Current v1 header records only generic format information and exact source length.

No trailing bytes are allowed after the fixed machine payload.

The reader rejects an artifact containing extra retained payload.

For research accounting:

```
carrier = complete frozen .truc artifact
```

For benchmark accounting, required custom decoder/runtime/program bytes must additionally be counted according to that benchmark's rules.

## 11. Exact replay gate

A candidate PASS requires all of:

1. machine geometry frozen before source access;
2. exactly 1,000,000 retained polarity bits;
3. no source-dependent growth;
4. source removed from the replay environment;
5. fixed page sequence;
6. fixed character-button sequence;
7. exactly one TRUE character per reconstructed position;
8. exact recovered byte count;
9. recovered bytes equal source bytes;
10. recovered cryptographic hash equals the separately recorded source hash.

One incorrect byte is failure.

## 12. Current measured evidence

Measured locally against the v1 implementation:

### Runtime and machine self-test

```
TRUCOMPUTE_CONFORMANCE=PASS
TRUCOMPRESSION_MACHINE=PASS
physical_nodes=1000000
retained_polarity_bits=1000000
button_order=0..255
```

### 18,600-byte text test

- imprint: PASS;
- frozen artifact: 125,064 bytes;
- direct replay: exact;
- literal fixed-order `0..255` button replay: exact.

This is a machine-mechanics test, not compression because the artifact is larger than the source.

### Deterministic 50,000-byte pseudo-random test

- imprint: PASS;
- frozen artifact: 125,064 bytes;
- replay: byte-exact.

### Deterministic 70,000-byte extension

With the same seeded pseudo-random prefix generator, v1 reports:

```
status=IMPRINT_CONTRADICTION
byte=59248
page=0
local=59248
bit=4
node_count=1000000
```

This is a measured failure of the **v1 imprint/routing law**.

It is not permission to change machine capacity.

## 13. Interpretation of the v1 failure

The current machine/control architecture remains the test subject.

The v1 routing law behaves like bounded parity storage and does not yet exploit enough document regularity to qualify as useful compression.

Therefore the next experiment may change only the imprint/routing/settling law while preserving:

- TruCompute dependency;
- exactly one million physical elements;
- one retained polarity bit per element;
- fixed geometry before source access;
- page voltage rather than stored pages;
- fixed character-button order;
- source-separated replay;
- complete artifact accounting;
- no-growth failure behavior.

## 14. Canonical enwik9 target

Canonical source:

- bytes: **1,000,000,000**;
- SHA-256: `159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc`;
- MD5: `e206c3450ac99950df65bf70ef61a12d`.

No enwik9 TruCompression PASS is currently claimed.

A canonical result exists only after full imprint, freeze, source removal, replay, byte identity, hash identity, and complete retained-size accounting.

## 15. Evidence labels

- **CANONICAL** — settled machine/protocol invariant.
- **MEASURED** — directly executed result.
- **EXPERIMENTAL** — implemented but not through the full target gate.
- **PROJECTED** — extrapolated, not directly measured.
- **FAILED-LAW** — tested tuning/routing law violated an acceptance gate while machine invariants stayed frozen.
- **TRUCOMPRESSION PASS** — reserved for a complete fixed-machine source-isolated exact reconstruction result.

## 16. Replication commands

From repository root:

```bash
g++ -O3 -std=c++17 trucompression/trucompression_native_v1.cpp -o trucompression
./trucompression selftest

./trucompression imprint SOURCE.bin machine.truc

# Remove or isolate SOURCE.bin before replay.
./trucompression replay machine.truc RECOVERED.bin --strict-buttons
```

Then independently compare source and recovered length and hash.

## 17. DCT/DCL revision rule

A failed imprint law does not redefine the machine.

Every new revision must state:

- frozen invariants;
- the single changed imprint/routing/settling component;
- expected measurable effect;
- exact failure/pass gate;
- lineage from the prior attempt.

The fixed machine is the substrate. The law is what evolves.
