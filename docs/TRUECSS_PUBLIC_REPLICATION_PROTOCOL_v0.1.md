# trueCSS — Public Replication Protocol v0.2

**Date:** 2026-09-18  
**Project:** Epsilonic / trueCSS  
**Status:** Experimental public replication specification. No compression result is claimed until the complete source-isolated replay gate passes.

## 1. Purpose

This document defines a public, reproducible test of a fixed-capacity relational graph.

The experiment asks whether one fixed graph of reusable physical nodes can reconstruct a chronology much larger than itself without allocating a new stored cell for every source position.

This public specification describes the retained graph and resolver entirely in conventional binary storage terms.

## 2. Canonical test source

Primary corpus:

- canonical enwik9;
- source size: 1,000,000,000 bytes;
- SHA-256: `159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc`;
- MD5: `e206c3450ac99950df65bf70ef61a12d`;
- observed terminal alphabet: 206 byte values.

First applicability gate:

- first 100,000,000 bytes of canonical enwik9;
- expected SHA-256: `2b49720ec4d78c3c9fabaee6e4179a5e997302b3a70029f30f2d582218c024a8`.

A full trueCSS claim requires the complete canonical 1 GB source-removal and replay gate. The 100 MB stage is an engineering applicability test.

## 3. Frozen architecture

The following remain fixed during this experiment:

- exactly 1,000,000 reusable physical nodes;
- one active chronological page condition at a time;
- conceptual page length of 1,000,000 source positions;
- one source target exposed to the teaching law at a time;
- no serialized page bank;
- no page-specific snapshot payload;
- no residual stream;
- no correction stream;
- no source-sized position selector;
- no chronology-dependent node-count growth;
- no chronology-dependent node-width growth;
- no hidden source-specific decoder constants;
- exact source-isolated cold replay.

If a relation law fails, the law may be changed in a later experiment. These architecture conditions remain unchanged.

## 4. Physical graph representation

Each physical node owns four fixed relation slots.

Every relation slot is stored as exactly two bits.

The public state table is:

| 2-bit code | Public meaning |
|---|---|
| `00` | active orientation A |
| `01` | inactive |
| `10` | active orientation B |
| `11` | reserved / invalid |

The destination of each relation slot is regenerated from its node index and slot index by a source-independent topology function. The destination table is therefore not retained as source-dependent storage.

Four two-bit relation slots occupy exactly one byte:

```
4 slots × 2 bits = 8 bits = 1 byte per node
```

The fixed graph-state payload is therefore:

```
1,000,000 nodes × 1 byte = 1,000,000 bytes
```

before header and explicitly counted metadata.

## 5. Valid relation-pair states

A routed decision uses two relation slots.

An inactive pair is:

```
01 01
```

An active complementary pair is one of:

```
00 10
10 00
```

The following are not valid active decision pairs:

```
00 00
10 10
01 00
00 01
01 10
10 01
11 xx
xx 11
```

Thus the reader does not reduce a pair to a single stored bit. It checks the complete two-slot pattern.

## 6. Relative orientation

The two valid active pair orders are distinguishable, but the reader does not assign universal semantic meaning to either code by itself.

Each routed decision therefore also uses a third active relation slot as a reference.

For relation slots `a`, `b`, and `reference`:

1. `a` and `b` must form one valid active complementary pair;
2. `reference` must be active;
3. the decision value is determined by whether `a` has the same or opposite active orientation as `reference`;
4. a source-independent modifier generated from chronology/context may invert the interpreted decision.

This keeps the stored representation relational rather than assigning a permanent source meaning to an individual graph slot.

## 7. Terminal representation

Canonical enwik9 uses 206 observed terminal byte identities.

The reference implementation uses eight binary relational decisions per terminal:

```
2^8 = 256
```

which is sufficient for all 206 observed terminals.

The mapping from observed source bytes to terminal codes is source-dependent metadata and is included in the frozen artifact.

## 8. Chronological conditioning

For source position `t`:

```
q = floor(t / 1,000,000)
k = t mod 1,000,000
```

The generic routing law combines:

- page condition `q`;
- local position `k`;
- recursively regenerated prior-output context;
- terminal-decision index.

Those values deterministically generate the relation slots used by the current decision.

The page condition does not select a stored page.

Instead:

```
same fixed graph
+ different q/context
-> different routed relation set
```

## 9. Teaching procedure

For every source byte, in chronological order:

1. compute `q`, `k`, and current deterministic context;
2. expose only the current terminal to the teaching rule;
3. map the terminal to its eight-decision code;
4. for each decision, deterministically generate `a`, `b`, and `reference`;
5. require `a` and `b` to form a valid active complementary pair;
6. require the orientation of `a` relative to `reference` to match the target after the generic modifier;
7. preserve every previous constraint;
8. advance only if the new constraint is compatible with the same fixed graph.

Training may use temporary solver state. That state is discarded before cold replay and is not part of the retained representation.

## 10. Contradiction rule

If a new target cannot be made compatible with the existing fixed graph constraints, the tested routing/settling law fails.

The run must record:

- exact source bytes absorbed before contradiction;
- page number;
- local page position;
- decision slot;
- terminals observed;
- physical node count;
- fixed node width.

A contradiction does not authorize adding page payloads, residual correction data, larger nodes, or chronology-growing storage.

## 11. Freeze

If the requested source span is absorbed without contradiction:

1. collapse the temporary training solver into the fixed two-bit relation slots;
2. leave unused slots at `01`;
3. settle participating slots to `00` or `10`;
4. serialize exactly one graph byte per physical node;
5. append only explicitly counted metadata;
6. verify replay using the generic reader while the source is still available;
7. remove the source and all training-only state;
8. perform cold replay from the frozen artifact.

## 12. Cold replay

The reader receives only:

- frozen graph bytes;
- counted terminal-map metadata;
- generic topology law;
- generic chronological routing law;
- generic relation resolver.

For each chronological position the reader:

1. regenerates the same routed relation triples;
2. verifies that each required relation pair is one of `00 10` or `10 00`;
3. verifies that the reference relation is active;
4. compares the first relation's orientation with the reference orientation;
5. applies the generic modifier;
6. reconstructs eight decision bits;
7. resolves the terminal code;
8. maps the terminal code to the exact source byte;
9. regenerates the next context;
10. advances.

No source semantics, external dictionary, network access, hidden page state, or correction stream is permitted.

## 13. Exactness gate

A run passes only when:

- the complete requested source span is reconstructed;
- every reconstructed byte equals the corresponding source byte;
- reconstructed SHA-256 equals source SHA-256;
- no source-derived object outside the measured frozen artifact is used;
- physical node count remains exactly 1,000,000;
- retained graph width remains exactly one byte per node;
- no page bank, residual stream, or correction stream exists.

One incorrect byte is failure.

## 14. Size accounting

Carrier-only frozen size includes:

- 1,000,000 graph bytes;
- file header;
- byte/terminal mapping;
- every other source-dependent metadata byte required for replay.

Temporary training structures are excluded only if they are destroyed before replay and are not required by the reader.

For benchmarks requiring complete package accounting, decoder/program bytes must also be reported according to that benchmark's rules.

## 15. Evidence terminology

**CANONICAL** — current authoritative architecture or protocol.

**MEASURED** — directly executed result with source identity, byte accounting, and replay outcome.

**EXPERIMENTAL** — implemented mechanism that has not passed the complete canonical gate.

**PROJECTED** — derived or extrapolated quantity not directly measured at the stated corpus size.

**FAILED** — the tested relation law or replay failed a required gate.

**trueCSS PASS** — reserved for a specification-conformant full canonical freeze, source removal, exact cold replay, byte equality, and SHA-256 match.

## 16. Continuity discipline

Every revision must declare:

- frozen invariants;
- one optimization target;
- minimum integration changes;
- measurable acceptance criterion.

A local optimization may not silently replace another settled part of the architecture.

A change outside that declared scope becomes a separate architecture branch.

## 17. Reference implementation

Repository:

`MungSauce/Dynamic-Field-Theory`

Current experimental implementation:

`experiments/truecss_3bit_graph_v3.cpp`

Current 100 MB workflow:

`.github/workflows/truecss-3bit-graph-100mb.yml`

The implementation filenames are historical development identifiers. The public protocol is defined by the binary representation and behavioral gates in this document.

## 18. Replication commands

Clone:

```bash
git clone https://github.com/MungSauce/Dynamic-Field-Theory.git
cd Dynamic-Field-Theory
git checkout truecss-3bit-20260918
```

Fetch canonical enwik9:

```bash
curl -L --fail --retry 4 --retry-delay 5 \
  -o enwik9.zip https://mattmahoney.net/dc/enwik9.zip
```

Extract the canonical 100 MB prefix:

```bash
python3 - <<'PY'
import hashlib
import zipfile

with zipfile.ZipFile("enwik9.zip") as z, \
     z.open("enwik9") as src, \
     open("enwik100m", "wb") as out:
    left = 100_000_000
    h = hashlib.sha256()
    while left:
        chunk = src.read(min(8 << 20, left))
        if not chunk:
            raise SystemExit("unexpected EOF")
        out.write(chunk)
        h.update(chunk)
        left -= len(chunk)

print(h.hexdigest())
PY
```

Expected SHA-256:

```
2b49720ec4d78c3c9fabaee6e4179a5e997302b3a70029f30f2d582218c024a8
```

Build:

```bash
g++ -O3 -std=c++17 \
  experiments/truecss_3bit_graph_v3.cpp \
  -o truecss_graph
```

Teach:

```bash
./truecss_graph train \
  enwik100m \
  truecss_graph.state \
  100000000
```

If teaching reports `status=FROZEN`, delete the source before replay:

```bash
rm enwik100m
```

Cold replay:

```bash
./truecss_graph replay \
  truecss_graph.state \
  recovered.bin
```

A successful 100 MB run must report:

```
status=REPLAY_PASS
recovered_bytes=100000000
```

and the independently calculated recovered SHA-256 must equal:

```
2b49720ec4d78c3c9fabaee6e4179a5e997302b3a70029f30f2d582218c024a8
```

## 19. Current public claim boundary

At publication of this protocol:

- the architecture is specified;
- the reference implementation exists;
- the 100 MB validation workflow exists;
- source-removal and exact-replay requirements are explicit;
- pending workflow results are not reported as measured results;
- no full canonical 1 GB compression pass is claimed.

If the current routing law fails, preserve its failure coordinate and modify only the failed routing/settling component in the next controlled experiment.

## 20. Replication principle

The public trueCSS experiment can be summarized entirely in conventional binary-storage terms:

```
fixed 1,000,000-node graph
× four 2-bit relation states per node
× chronology-conditioned routing
× relative relation decoding
-> exact source chronology
```

The experiment succeeds only if that fixed retained graph can reproduce the requested source exactly after the source and all training-only state are removed.
