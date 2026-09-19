# TruCompute Native Resistive Field Architecture v11

**Date:** 2026-09-18  
**Status:** CANDIDATE REAL ARCHITECTURE / BOUNDED REFERENCE TRANSFER LAW

## Native substrate

TruCompute is an uninterrupted three-condition field:

\`\`\`
NEG / BOTH / POS
\`\`\`

There is no OFF, NEITHER, reset-to-zero, clear, or recompute-from-zero phase.

Each data node is continuously present as a native resistor-like field element.

\`\`\`
native substrate condition = BOTH
\`\`\`

A page-letter probe changes the effective resistance of that same node and resolves it directly:

\`\`\`
low resistance                 -> NEG / FALSE
resistance >= flip threshold   -> POS / TRUE
\`\`\`

Changing selectors transitions directly from one probe condition to the next.

## Generic machine

The generic machine contains:

- exactly 1,000 fixed page activators;
- exactly 206 fixed character activators;
- a fixed population of native resistor data nodes;
- one generic transfer and threshold law.

Activator banks are fixed wiring, not source-dependent storage.

## Source-dependent node imprint

Each data node contains one fixed-width imprint vector:

\`\`\`
theta_i = [a_0, a_1, ..., a_(K-1)]
\`\`\`

K is chosen before source access and may not grow during imprint or replay.

The node does not contain:

- a page table;
- a character table;
- a page-letter response table;
- exceptions;
- residual source bytes;
- a sidecar page bank.

## Reference transfer law

v11 supplies a bounded polynomial transfer law as a first executable native resistor law:

\`\`\`
resonance_i(page)
  = a_0 + a_1 page + ... + a_(K-1) page^(K-1)
    mod 65521
\`\`\`

The selected character acts as the probe:

\`\`\`
effective_resistance =
    1024  if resonance_i(page) == character
    0     otherwise
\`\`\`

with flip threshold 512.

This reference law is replaceable. It is not claimed to be the final TruCompute physical transfer law.

## Capacity semantics

The fixed-width imprint is exact or it fails.

If the transfer function cannot reproduce every requested page-conditioned symbol:

\`\`\`
CAPACITY_EXCEEDED
\`\`\`

The architecture is forbidden to widen K, allocate more records, add residuals, or switch codecs after seeing the source.

Therefore representational failure remains visible rather than being hidden as conventional storage growth.

## Continuous observation invariant

For each selector transition:

\`\`\`
(P_q,C_a) -> (P_q,C_b) -> (P_r,C_b)
\`\`\`

the same continuously present nodes resolve directly under the new condition.

No intermediate BOTH/zero reset is inserted computationally. BOTH remains the underlying native substrate throughout.

## Compression gate

This architecture is not a compression claim by itself.

A valid TruCompression result requires a fixed K chosen before source access, full canonical enwik9 imprint, source deletion, exact replay through the 1,000 x 206 selector sweeps, and measurement of the surviving imprint artifact.
