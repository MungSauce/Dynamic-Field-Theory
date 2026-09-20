# Qompress Reflex Core v0.2

Canonical blank is `0x55 = [-+, -+, -+, -+]`.

At position `i`, button `c` is true exactly when `field[i] == c`.

Observation is immediate:

`delta = old XOR observed`

`observed = old XOR delta`

A zero delta is derived persistence and is not retained. A nonzero delta is corpus-specific information in the v0.2 seed. XOR makes the local transition exactly reversible.

This module implements the reflex rather than assuming that a finite byte contains unlimited arbitrary predecessor history.
