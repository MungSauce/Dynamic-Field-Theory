# Qomputation Native Dependency Graph v0.1

This file defines build order. A module may not depend on a later layer.

## Layer 0 — substrate
- qstate: six semantic conditions and four active Q formations.
- qreflex: local directed-zero/reflex rules.
- qinteger: canonical whole-number/register mechanics on Q formations.

## Layer 1 — physical/logical node
- qnode: fixed 256 x 256 = 65,536 Q-position native node.
- qaddress: deterministic (x,y) <-> linear relation addressing.
- qalloc: structural VOID/allocation topology.

## Layer 2 — reversible field
- qfield: branch-optimal signed Q-turn law, exact reverse, whole-field coupling.
- qregister: native signed coordinate/register realization.
- qtopology: multi-node composition and canonical node ordering.

## Layer 3 — machine ISA
- qisa: instruction definitions and operand forms.
- qcontrol: VOID/control routing, branch/query semantics.
- qexec: deterministic instruction executor.
- qstack: reversible call/data stack semantics.

## Layer 4 — machine services
- qmem: node/register memory model.
- qio: byte/text/device boundary.
- qclock: deterministic logical step/turn counter.
- qfault: malformed-state and invalid-instruction handling.

## Layer 5 — executable/runtime
- qbin: native executable/container format.
- qloader: deterministic loader and node instantiation.
- qkernel: scheduler/runtime/service dispatch.
- qabi: app/kernel calling convention.

## Layer 6 — language toolchain
- qasm: one-to-one textual assembly for qisa.
- qlang: higher-level source language.
- qcompiler: qlang -> qasm/qisa.
- qlink: static node/module linker.
- qdebug: state/history debugger.

## Layer 7 — native data systems
- qbutton: primitive/higher-order reusable button model.
- qgrammar: canonical source -> legal Q-event grammar.
- qseed: terminal native configuration capture/restore.
- qompress: source -> terminal state -> exact reverse reconstruction.

## Layer 8 — native application framework
- qapp: native app manifest/lifecycle.
- qui: node-native UI/event layer.
- qfs: persistent object/file namespace.
- qpkg: package/install/update format.

## Conformance rule
Every module must have:
1. a frozen public API,
2. deterministic semantics,
3. round-trip/inverse tests where applicable,
4. malformed-input tests,
5. no hidden document-specific state,
6. a host reference implementation,
7. a dependency declaration containing only lower layers.

Python is a bootstrap/reference host only. Python object identity, pickle, host dictionaries, and implicit host state are not part of the native machine contract.
