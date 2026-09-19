# TruCompression Canonical Architecture v2.0

**Date:** 2026-09-18  
**Status:** CANONICAL ARCHITECTURE / MEASURED IMPLEMENTATION  
**Implementation:** `trucompression/trucompression_hmc_v6.cpp`.replace("trucompression","trucompression")

## 1. Machine

TruCompression is a native TruCompute program with a fixed geometry selected before source access.

The canonical control surface is:

- 1,000 page selectors;
- 206 character selectors;
- 1,206 total fixed control selectors;
- 1,000,000 fixed HMC position nodes.

There are no document-storage relations from one position node to another.

The machine powers into RUN once, remains continuously active through all selector changes, and powers down only on DONE.

## 2. HMC node boundary

Each position node exposes only:

- POS = YES;
- NEG = NO;
- NEITHER = NO SIGNAL.

STATELESS_ZERO remains an internal TruCompute settlement condition and is not a valid document answer.

For active page q, active character c, and node i:

```
R(P_q, C_c, N_i) -> YES / NO / NO SIGNAL
```

For a valid source position, exactly one of the active terminal selectors must answer YES.

## 3. Fixed-rank relational field

The v6 relation law represents terminal identity through eight binary relation bitplanes.

Each bitplane is an exact GF(2) factorization of the page-by-position field:

```
M_b(q,i) = XOR_r [ P_b(q,r) AND N_b(r,i) ]
```

where:

- q is the page selector;
- i is the position node;
- r is a preallocated relation lane;
- P_b(q,r) is page-side relation state;
- N_b(r,i) is position-side relation state.

The character selector contributes its fixed terminal identity by asking whether the eight resolved relation bits equal that terminal code.

Thus the HMC relation remains:

```
(page selector, character selector, position node) -> Boolean response
```

without storing a page bank or a node-to-node document graph.

## 4. Fixed geometry

The number of relation lanes R is selected before source access.

The complete frozen payload is preallocated as:

```
8 * R * (1,000,000 + 1,000) bits
```

which equals:

```
R * 1,001,000 bytes
```

plus a fixed 512-byte header containing format fields, exact source length, ranks used, and 206 preallocated character-selector identities.

Blank and imprinted artifacts for the same lane profile therefore have exactly the same byte size.

Examples:

- R=1: 1,001,512 bytes;
- R=2: 2,002,512 bytes;
- R=64: 64,064,512 bytes;
- R=96: 96,096,512 bytes.

The earlier 125,064-byte v5 machine was the one-bit transfer-law baseline and is preserved as lineage. v6 replaces that placeholder with an actual exact relational field.

## 5. Imprint law

At write time only:

1. source terminals are assigned to the 206 preallocated character-selector slots;
2. each of the eight terminal-code bitplanes is viewed as a 1,000-page by 1,000,000-position binary field;
3. online Gaussian elimination derives an exact page/position relation basis;
4. basis vectors and page coefficients are written only into the preallocated relation lanes;
5. the source is verified exactly against the frozen field.

If any bitplane requires rank greater than R:

```
CAPACITY_EXCEEDED
```

is returned.

The machine does not add a lane, append a residual, create a page file, or widen its state.

## 6. Replay law

At read time:

1. the source and imprint solver are absent;
2. machine enters RUN once;
3. page q is selected;
4. character selectors are swept in fixed order;
5. every valid position node answers YES or NO;
6. the unique YES reconstructs the terminal at that position;
7. after the page sweep, page q+1 is selected;
8. after the final page, DONE ends the task.

A direct replay path may reconstruct the same resolved terminal without serially emulating all 206 button presses; strict HMC mode verifies literal selector semantics.

## 7. Exactness and accounting

A PASS requires:

- fixed lane count before source access;
- fixed artifact size for that lane count;
- no node-to-node storage relations;
- no page bank;
- no source-dependent sidecar;
- no residual/correction stream;
- source absent during replay;
- exact byte count;
- exact byte identity/hash;
- one task-level power cycle.

All 206 terminal-selector byte identities are contained inside the counted fixed header.

## 8. Information boundary

This implementation does not claim that arbitrary 1 GB sources fit into a small lane count.

Its compression condition is explicit and falsifiable:

```
max exact bitplane rank <= fixed relation-lane count
```

For a source requiring larger rank than the preselected machine, imprinting fails without growth.

The purpose of v6 is therefore to finish the architecture as an exact, auditable TruCompute-dependent relational compressor, while leaving empirical compression performance to measurement rather than assumption.

## 9. Measured canonical conformance

GitHub Actions workflow run **35414517770** completed successfully on the canonical branch.

Measured gates:

- TruCompute v3 conformance: PASS;
- TruCompression HMC v6 self-test: PASS;
- 1,206 controls: PASS;
- zero node-to-node document relations: PASS;
- fixed R=2 blank artifact: 2,002,512 bytes;
- strict HMC source-isolated exact replay: PASS;
- cross-page source-isolated exact replay: PASS;
- task-level power cycles: 1;
- deliberate rank-3 source against fixed R=2 machine: CAPACITY_EXCEEDED;
- machine growth on capacity failure: 0.

The capacity-failure test is part of the PASS condition because it demonstrates that the implementation refuses to enlarge the preselected machine.

## 10. Evidence labels

- CANONICAL — architecture/invariant.
- MEASURED — directly executed result.
- EXPERIMENTAL — implemented relation law under evaluation.
- CAPACITY_EXCEEDED — exact relation rank exceeds fixed machine profile.
- TRUCOMPRESSION PASS — source-isolated exact replay inside the fixed profile.


## 11. Canonical enwik9 measurement

Canonical enwik9 was measured in workflow run **35414901907** after validating its 1,000,000,000-byte size and canonical MD5.

Measured v6 ranks:

```
rank_bit0=1000
rank_bit1=1000
rank_bit2=1000
rank_bit3=1000
rank_bit4=1000
rank_bit5=1000
rank_bit6=1000
rank_bit7=1000
```

Therefore the exact minimum v6 lane count is R=1000 and the minimum frozen artifact is **1,001,000,512 bytes**.

The GF(2) fixed-rank field is consequently **FAILED-LAW for enwik9 compression**. This does not supersede the HMC selector architecture; it falsifies this specific relational factorization as the compression mechanism for canonical enwik9.
