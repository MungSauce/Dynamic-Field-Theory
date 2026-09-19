# TruCompute topology-only identity probe v19

Status: PARTIAL / LIMIT FOUND
Date: 2026-09-18
Parent: `trucompute-shared-context-field-v18-20260918`

## Question

Can the v18 contextual-zero field remove numeric node identity completely and let fixed relational topology alone produce the node distinctions?

## Probe A — sparse local topology

A directed rooted path of 1,000,000 nodes is used. Nodes have no logical numeric address in the update rule. Each round refines a node only from its own previous structural class and its predecessor's class.

After 30 rounds:

```
local_sparse_path_distinguishable_classes = 32
```

So a sparse local topology with one-edge-per-step causal propagation does not produce million-way identity in 30 contextual events. It would require much more time or richer connectivity.

## Probe B — topology-rich relation signatures

Each node is given only structural +/- relations to 20 shared contextual anchors. The evolution rule never reads the simulator storage index.

Because:

```
2^20 = 1,048,576
```

20 independent relation distinctions are sufficient for one million unique nodes.

The contextual recurrence:

```
Z_(t+1) = 2 Z_t + s_t
```

then produces one million unique contextual zeros.

The simulator storage order is randomly permuted and recomputed. Results remain attached to the same relational signature:

```
numeric_node_id_used_by_update_rule = NO
permutation_invariance = PASS
topology_signature_unique_contexts = 1,000,000
```

Therefore identity can be relational rather than numeric.

## But the address information did not disappear

The topology-rich construction carries:

```
20 relation bits/node
20,000,000 relation bits for 1,000,000 nodes
```

before ordinary graph representation overhead.

So v19 removes an explicit numeric address from the update law but merely moves address-equivalent distinguishability into relational topology.

For a single node to distinguish every possible 30-step +/- history:

```
2^30 = 1,073,741,824 histories
```

there must be at least 30 bits of distinguishability somewhere in the total machine state/topology/context. The old-zero recurrence does not evade that bound; it is an order-preserving encoding of those choices.

## Result

```
numeric node ID in update law: removed
million-way relational identity: achieved
permutation invariance: achieved
address-equivalent information eliminated: NO
fixed one-bit/node invariant preserved: NO, if each node independently retains 30-step exact history
```

The useful result is architectural: chronology can live in relational state instead of a conventional address register.

The failed claim is stronger: exact independent history/address information cannot disappear entirely. If it is not in a node register, it must be present in topology, distributed state, timing, or another contextual degree of freedom.

## Next target

The next useful experiment should not add more per-node signature bits. It should test a sparse dynamic relational field in which chronology is shared across nodes and only differences are retained. That is the plausible route to reuse/compression: exploit redundancy in the histories rather than trying to encode arbitrary independent histories for free.
