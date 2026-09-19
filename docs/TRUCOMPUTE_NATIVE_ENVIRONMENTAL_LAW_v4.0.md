# TruCompute Native Environmental Law v4

**Date:** 2026-09-18  
**Status:** CANONICAL CORRECTION / IMPLEMENTED

## Correction

TruCompute is defined by its environmental law first.

A node is not a binary, trinary, quaternary, or enum-valued storage cell that is later interpreted as TruCompute.

A node is only an identity/location in a TruCompute environment.

```
Node = identity
Observation = Law(Environment, Node, Selector)
```

The canonical observations remain:

- NEITHER — no contribution participated;
- NEG — participating field resolved negative;
- STATELESS_ZERO — contributions participated and exactly balanced;
- POS — participating field resolved positive.

Those are outcomes produced by the law. They are not retained node values.

## Native order

```
TruCompute environmental law
-> native node identity
-> CSS relations/influences
-> selector-conditioned participation
-> observation
```

This supersedes implementations whose primitive object was a stored 2-bit state later named NEITHER/NEG/POS/STATELESS_ZERO.

## Selector law

Page and character controls modify the conditions under which the environment is resolved.

Changing a selector may change the observation of the same node without mutating that node.

This is the foundation required by CSS overlay / "different colours and tinted glasses":

- repeated source layers may address the same substrate;
- selector context changes which influences participate;
- nonselected influences remain nonparticipating rather than being overwritten;
- an apparent collision is not automatically a contradiction merely because two conditions imply different observations.

## Host implementation

A conventional CPU will necessarily encode the emulator using binary memory.

That host representation is implementation machinery, not the ontology of the abstract TruCompute node.

The reference runtime therefore forbids retained state fields inside `Node`. Concrete environments implement source-independent law and source-dependent CSS imprint separately, and `Observation` exists only as the result of resolving the current environment.

## Acceptance

A conforming implementation must demonstrate:

1. a node contains identity only;
2. an inactive selector resolves NEITHER without mutating the node;
3. changing selector context can change the observation of the same node;
4. participating opposing contributions resolve STATELESS_ZERO;
5. STATELESS_ZERO and NEITHER remain distinct;
6. no observation enum/value is retained as node state.

## Compression implication

This correction alone does not establish a compression ratio.

The next TruCompression implementation must build CSS overlay directly on this law. The measured artifact must contain only the source-dependent dormant CSS imprint required for exact source-isolated replay. Page and character selector consequences may not be serialized as separate response tables.
