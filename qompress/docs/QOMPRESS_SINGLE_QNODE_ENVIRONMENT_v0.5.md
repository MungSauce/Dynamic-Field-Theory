# Qompress Single-QNode Environment Lock v0.5

Status: implementation lock / Python committee reference  
Date: 2026-09-21

## 1. Product identity

Qompress is not a conventional codec wrapped around a QNode.

**Qompress is the fixed environment law plus one fixed 256x256 QNode.**

There is no second document-specific structure whose job is to remember chronology. Each source byte enters the environment once; that turn reconfigures the existing node. The next configuration depends on the previous configuration by the fixed reversible environmental law. That causal dependence is the chronology.

## 2. Locked invariants

1. One QNode: 256x256 = 65,536 logical positions.
2. Primed Zero is the unique starting/unwound condition.
3. One input byte = exactly one Q-turn.
4. All 256 byte values have direct event classes.
5. No ESC/fallback second event is inserted.
6. A byte turn is one +1 activation at fixed relation `byte+1`.
7. The environment applies a reversible whole-field consequence.
8. The terminal QNode is the only document-specific object needed by reverse execution.
9. Reverse determines the final byte and unique predecessor from the current node itself.
10. Reverse continues until Primed Zero, so source length is not required in the terminal artifact.
11. No route, pages, event stack, predecessor table, source hash, source copy, or chronology log may survive encoding and be available to decoding.
12. The reference environment is pure Python using only the standard library.
13. Fixed topology and fixed information capacity are different claims. Python arbitrary-precision growth must be counted and may not be relabeled as free node history.

## 3. Direct byte quotient law

The older signed-event construction used `2N+1` quotient classes because it allowed a +/- activation at every relation. The locked Qompress byte environment has exactly 256 legal source events and no per-byte control event.

Use all 256 quotient classes:

```text
phi(x) = x_1 + sum_{r=2..255} r*x_r  (mod 256)
```

Define B by:

```text
(Bx)_1 = 256*x_1 - sum_{r=2..255} r*x_r
(Bx)_r = x_r, r >= 2
```

Byte `b` is exactly one event:

```text
c_b = +e_(b+1)
```

Relations 1..255 occupy residues 1..255. Relation 256 occupies residue 0. Therefore every quotient class is one payload byte class; there is no unused ESC/control class in the source chronology.

## 4. Whole-field environment law

Use the proved rank-one integer shear:

```text
U = I + 2*1*v^T
```

where `v` contains 32,768 +1 entries followed by 32,768 -1 entries.

Forward byte turn:

```text
T_b(X) = U(BX + e_(b+1))
```

The event contribution `U e_(b+1)` has nonzero support across all 65,536 logical positions.

Reverse:

```text
Z = U^-1 Y
k = phi(Z)
relation = 256 if k == 0 else k
byte = relation - 1
X = B^-1(Z - e_relation)
```

No external chronology variable appears in the transition or inverse.

## 5. Python representation

The reachable state is evaluated lazily as:

```text
x_1 = x1
x_r = global_offset + d_r   for 2 <= r <= 256
x_r = global_offset         for 257 <= r <= 65,536
```

Only the fixed coordinates `x1`, `global_offset`, and `d_2..d_256` are serialized. Acceleration sums are derived after load and are never serialized.

This lazy chart is mathematically equivalent to applying the dense 65,536-coordinate transition. The conformance suite includes an independent dense-vector equivalence test.

## 6. Cold-replay contract

Committee-style test:

```text
python -m qompress.hutter_reference encode enwik9 terminal.qnode
remove enwik9 from the decode workspace
python -m qompress.hutter_reference decode terminal.qnode data9
compare data9 with independently restored enwik9
```

The decoder receives only:

- the fixed Python program;
- the terminal QNode file.

The source hash printed during encode is measurement output only and is not placed in the terminal file.

## 7. Fixed-capacity gate

The native active local alphabet has four states. Therefore one fully allocated 65,536-position node has:

```text
4^65,536 = 2^131,072
```

possible active terminal configurations: exactly 131,072 bits of ideal finite-state distinguishability.

For unrestricted byte chronology, there are `256^T = 2^(8T)` histories of length T. Therefore a single finite four-state QNode can distinguish all unrestricted raw byte histories only through:

```text
T <= 131,072 / 8 = 16,384 turns.
```

This does not say a structured output can never be longer. It says that any longer exact history must have a legal/contextual description below 8 independent bits per turn on average, or use more counted terminal capacity.

The exact Python lattice uses arbitrary-precision integers to test the reversible environmental law without overflow. Those extra precision bits are real host information. The `audit` command reports their serialized size and whether that concrete image fits the declared finite-node capacity. Modular wraparound is forbidden because it would create collisions.

## 8. Current proof status

Established by code/tests:

- fixed 256x256 topology;
- all 256 bytes use one turn each;
- exact forward/reverse for all 256 one-turn events;
- order-dependent terminal states;
- source-removed cold replay on test corpora;
- reverse terminates at Primed Zero;
- no route/length/hash/history serialized;
- lazy representation equals the direct dense transition on an independent check;
- precision growth is surfaced by the audit rather than hidden.

Not yet established:

- a finite four-state realization that carries one billion enwik9 turns in one QNode;
- an enwik9 terminal representation below the current Hutter record;
- Hutter runtime/resource compliance at the 1GB scale.

Those are the next falsification/engineering gates, not assumptions baked into the reference.
