# TruCompression Native Machine Measurement — enwik9

**Date:** 2026-09-18  
**Status:** MEASURED / NATIVE STRUCTURAL METRIC  
**Workflow run:** 35415305438  
**Source:** canonical enwik9  
**Source bytes:** 1,000,000,000  
**Source MD5:** e206c3450ac99950df65bf70ef61a12d

## Fixed machine

Measured canonical TruCompression geometry:

- position nodes: 1,000,000
- page selectors: 1,000
- character selectors: 206
- total control selectors: 1,206
- total fixed structural elements: 1,001,206
- machine growth during read: 0
- power cycles per task: 1

## Native reuse density

The 1,000,000 position nodes are reused across 1,000 full pages.

Measured:

- source positions per position node: 1,000
- source bytes represented per position node: 1,000
- source bytes per total fixed structural element: 998.795

These are native structural-density metrics, not a conventional serialized byte ratio.

## Full HMC read workload

For all 1,000 pages and all 206 character selectors:

- page activations: 1,000
- character activations: 206,000
- selector conditions: 206,000
- node decisions: 206,000,000,000
- YES responses: 1,000,000,000
- NO responses: 205,000,000,000
- NO SIGNAL responses at valid positions: 0

Every source position produces exactly one YES during its page's 206-character sweep.

## Interpretation

This is the correct native-machine measurement of the canonical HMC geometry. It does not serialize the machine's relational configuration into an ordinary byte array.

The structural machine maps a 1,000,000,000-byte source onto a fixed 1,001,206-element computational structure, reusing every position node across 1,000 pages.

However, this structural density must not be presented as an information-theoretic byte-compression ratio until the physical/source-dependent tuning degrees of freedom of each fixed element and relation are specified and bounded. The machine's configurational capacity is part of its physical storage capacity even when no conventional byte array exists.

Thus the currently defensible native result is:

- **position-field reuse:** 1000:1
- **whole-machine structural density:** 998.795 source bytes per fixed structural element
- **physical growth during read:** 0
- **task-level power cycles:** 1

The earlier 1,001,000,512-byte v6 number measures one conventional serialization of a relational configuration and is not this native structural measurement.
