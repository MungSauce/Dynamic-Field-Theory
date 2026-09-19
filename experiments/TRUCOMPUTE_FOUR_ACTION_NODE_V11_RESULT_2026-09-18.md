# TruCompute Four-Action Native Node v11 — Conformance Result

**Date:** 2026-09-18
**Status:** MEASURED PASS
**Workflow run:** 35419959243

The native node action law is now unified:

```
00 -> NEITHER
01 -> NEG
10 -> BOTH
11 -> POS
```

The two host bits are only an emulator encoding for four native TruCompute actions.

Measured invariants:

```
TRUCOMPUTE_FOUR_ACTION_NODE_V11=PASS
native_action_count=4
host_encoding_bits=2
action_00=NEITHER
action_01=NEG
action_10=BOTH
action_11=POS
off_action=NEITHER
powered_unobserved_action=BOTH
observed_actions=NEG_OR_POS
observation_mutates_imprint=false
page_response_table=false
character_response_table=false
```

The same condition-sensitive resistor node demonstrated all four actions:

- power absent -> NEITHER;
- power present without a page-letter probe -> BOTH;
- nonmatching page-letter probe -> NEG;
- matching page-letter probe -> POS.

Releasing the probe returned the node to BOTH without changing its fixed imprint.

This result is architectural conformance only. It does not establish a compression ratio.
