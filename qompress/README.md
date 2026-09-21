# Qompress v0.5 — Single-QNode Python Environment

Qompress is defined here as the environment law plus one fixed 256x256 QNode.

Locked source-path invariants:

- one QNode = 65,536 logical positions;
- Primed Zero is the origin;
- one input byte = exactly one Q-turn;
- all 256 byte values are direct event classes;
- no ESC/fallback second turn;
- no route, page stream, event stack, predecessor table, turn count, or source hash in the terminal node;
- reverse recovers the final byte and unique predecessor from the current node and continues until Primed Zero;
- the reference implementation is pure Python / standard library.

Run the conformance suite:

```bash
python qompress/tests/test_single_qnode_v05.py
```

Committee-style cold replay:

```bash
python -m qompress.hutter_reference encode enwik9 terminal.qnode

# Remove enwik9 from the decode workspace.

python -m qompress.hutter_reference decode terminal.qnode data9
python -m qompress.hutter_reference audit terminal.qnode
```

The reference deliberately uses exact Python integers to test the reversible environment law. Their precision growth is counted by the audit and is not treated as free native storage.

See:

`qompress/docs/QOMPRESS_SINGLE_QNODE_ENVIRONMENT_v0.5.md`
