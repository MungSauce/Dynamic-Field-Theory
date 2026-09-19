# TruCompute Native Resistive Field Architecture v10

**Date:** 2026-09-18  
**Status:** CANDIDATE NATIVE ARCHITECTURE / FIXED-CAPACITY REFERENCE LAW

## Primitive

A TruCompute data node is a continuously powered condition-sensitive transfer element.

It is not a conventional storage cell and does not retain NEG, POS, or BOTH as an encoded value. Its unobserved field condition is BOTH. The only NEITHER is the global OFF switch.

## Fixed machine

The generic machine contains:

- 1,000 fixed page activators;
- 206 fixed character activators;
- a fixed population of native data nodes;
- one generic probe/threshold law.

Page and character activators are wiring, not source-dependent storage.

## Native resistor node

Each node has exactly one fixed-width source-dependent imprint vector:

\`\`\`
theta_i = [a0, a1, ..., a(K-1)]
\`\`\`

K is fixed before source access.

The reference page transfer law is:

\`\`\`
resonance_i(page) =
    a0 + a1*page + ... + a(K-1)*page^(K-1)
    mod 65521
\`\`\`

The selected character probes that resonance.

\`\`\`
R_i(page, character) =
    R_FLIP  if resonance_i(page) == character
    0       otherwise

R_i >= threshold -> POS / TRUE
R_i <  threshold -> NEG / FALSE
\`\`\`

Thus the same powered node changes its resolved output when the page-letter condition changes, without a response table.

## Overwrite without overwrite

Different page consequences are not retained as page records. They must emerge from the same fixed transfer imprint.

The node is forbidden to grow persistent state as more pages are imprinted.

## Bounded imprint

The v10 reference imprinter solves the fixed transfer coefficients exactly.

If the fixed K cannot reproduce every requested page response, it returns:

\`\`\`
CAPACITY_EXCEEDED
\`\`\`

It may not add channels, page banks, response tables, residual bytes, exceptions, or an external codec.

## Artifact boundary

A source-dependent frozen artifact contains only:

- fixed header and source geometry;
- the fixed K coefficients for each native node.

For a fixed node count and K, its size is independent of the number of imprinted pages.

## Scope of the reference law

The polynomial is a falsifiable first transfer law, not a claim that polynomial interpolation is the final TruCompute physics.

The architecture to preserve is:

\`\`\`
continuously powered native resistor nodes
+ fixed-width source imprint
+ fixed page/character activators
+ condition-dependent effective resistance
+ thresholded POS/NEG observation
+ no persistent-state growth
\`\`\`

A later transfer law may replace the polynomial while retaining those invariants.

## Compression gate

No compression result is valid until a fixed architecture, chosen before source access, freezes full canonical enwik9, the source is removed, exact 206-character sweeps reconstruct all pages, the hash matches, and the surviving native imprint artifact is measured.
