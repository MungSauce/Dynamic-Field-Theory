# MSF Continuity Backfill Audit — 2026-09-18

Status: CANONICAL PROCESS AUDIT

## Purpose

This audit reconstructs the MSF implementation lineage under the continuity rule:

> A correction or optimization changes only its declared target and the minimum interfaces required to integrate it. Everything else remains frozen.

The goal is to distinguish genuine incremental improvements from accidental architecture replacement.

## Frozen architecture established by v3

The following are retained unless explicitly named as a future optimization target:

1. Orchestra / Encoder performs the source.
2. Recording / .msf is the retained literal signal artifact.
3. Audience / Decoder receives only the recording plus generic listening law.
4. Source is divided into N deterministic pages.
5. One instrument owns one page.
6. Each page is read inward from both ends.
7. Each full logical timestamp therefore represents up to 2N source positions.
8. Every instrument reuses the same source-symbol lexicon; forward/reverse are directional roles, not separate per-instrument languages.
9. Maximum directional vocabulary is 412 identities for a 206-symbol alphabet.
10. Instrument identity is procedural/source-independent.
11. Instrument contributions are superposed before retention.
12. The retained recording is one continuous 1-D mixed track, never one stored track/lane per instrument.
13. The Audience separates contributors from that mixed recording using the generic modifier/listening law.
14. Cold replay requires source removal and no Composer state at decode.
15. Primary compression metric is retained signal payload bytes / source bytes.
16. Container/framing overhead is separately reported.

## Backfilled revision classification

### v3 — BASELINE ARCHITECTURE / MEASURED

Optimization target: establish the literal orchestra -> recording -> audience architecture.

Changes introduced:
- one-page-per-instrument geometry;
- simultaneous inward reading;
- shared directional lexicon;
- procedural Walsh instrument signatures;
- composite signal recording;
- isolated Audience cold replay.

Measured 100 MB result at N=128:
- timestamps: 390,625;
- source positions per full timestamp: 256;
- recorded signal payload: 100,000,000 bytes;
- signal ratio: 1.000000;
- exact cold replay: PASS.

Interpretation: architecture works. Reference sample representation spends two bytes per composite chip and therefore yields no signal compression.

### v3.1 — VALID SURGICAL OPTIMIZATION / MEASURED LINE

Declared target: signal-sample storage/packing only.

Frozen from v3:
- page geometry;
- one page per instrument;
- two-direction reading;
- lexicon;
- Walsh instrument basis;
- number and meaning of composite signal samples;
- Audience separation;
- cold-replay boundary.

Changed:
- two already-produced composite signal amplitudes are packed into a 31-bit signal code;
- Audience performs the inverse packing before the unchanged signal analysis.

This is a valid isolated optimization because the packer has no page, symbol, instrument, or note semantics.

Known measured 100 MB signal result before continuity backfill:
- signal payload: 96,875,000 bytes;
- signal ratio: 0.968750;
- signal compression: 3.125%;
- exact cold replay: PASS.

The later backfill change restoring accepted instrument range [1,4096] does not affect N=128 behavior, but future formal reruns must verify the same result on the corrected source tree.

### v3.2 — VALID FEATURE LAYER, NOT A COMPRESSION OPTIMIZATION

Declared target after backfill: mixed-track signal presentation/listening, specifically making the retained signal noise-like and non-lane-readable using a deterministic postmix PRN transform.

Frozen from v3.1:
- page geometry;
- directional reading;
- shared lexicon;
- Walsh synthesis/despreading;
- signal sample count;
- 31-bit signal packing;
- compression metric;
- cold replay.

Changed:
- a source-independent reversible PRN scrambling transform is applied after superposition and before packing;
- Audience applies the inverse transform before the pre-existing matched-filter stage.

Expected compression consequence: none by construction, because the PRN transform is bijective and does not reduce sample count or packing width.

Therefore v3.2 must not be described as a compression improvement. It is an integration/recording-obfuscation feature whose acceptance criteria are:
- one-track invariant preserved;
- exact cold replay preserved;
- signal byte count equal to the v3.1 packed-signal baseline;
- retained samples are transformed only after the orchestra mix.

### v4 — SUPERSEDED / ARCHITECTURE DRIFT

Attempted target: reduce samples per logical timestamp.

Actual change: radix-pack complete simultaneous instrument state into one arbitrarily wide scalar.

Why excluded:
- changes the semantic nature of the recording from sampled composite field to encoded state snapshot;
- substitutes a new representation rather than improving the existing mixed-track signal;
- violates the literal mixed-track recording invariant.

v4 remains falsification evidence only. Its measurements do not belong in the current MSF compression lineage.

## Accidental drift discovered and repaired

v3.1/v3.2 changed the accepted instrument-count lower bound from 1 to 16 without the instrument geometry being the optimization target.

Impact on N=128 measurements: none.

Process impact: violation of continuity discipline.

Repair: restored the v3 [1,4096] power-of-two instrument range in both v3.1 and v3.2 signal-field implementations.

## Reconstructed current model

The current model is not v3.2 replacing v3.1.

It is:

v3 baseline architecture
+ v3.1 signal-packing optimization
+ optional v3.2 postmix PRN presentation layer

This distinction matters. A feature layer that does not improve compression must not displace a validated compression optimization.

The next compression experiment therefore begins from the v3.1/v3.2 shared frozen architecture and may modify only the **mixed-track signal basis / temporal-spectral representation**, plus the minimum inverse-listening integration required by the Audience.

It may not alter pages, page ownership, two-direction reading, lexicon semantics, orchestra/recording/audience roles, one-track retention, cold replay, or signal-byte accounting.

## Pre-implementation contract for next signal-basis experiment

Frozen invariants:
- all sixteen frozen architecture points above;
- v3.1 packing remains available as the storage layer unless the experiment explicitly targets packing;
- v3.2 PRN transform remains an optional postmix layer and is not credited as compression.

Single optimization target:
- reduce the number of bits/samples required by the one mixed track to preserve a logical timestamp while retaining exact Audience separability.

Permitted integration changes:
- synthesis law inside the Orchestra signal mixer;
- corresponding generic inverse/detection law inside the Audience;
- format fields strictly required to identify a source-independent signal-law version.

Acceptance criteria:
- exact 100 MB cold replay;
- one retained mixed track;
- no source-specific side state;
- same page/lexicon/time geometry;
- signal payload < 96,875,000 bytes to beat the current measured packing floor;
- report signal bytes independently of container overhead.

## Process conclusion

The model change from the continuity rule is substantial.

Previously, implementation revisions could silently bundle architectural reinterpretation with local optimization, making results difficult to attribute and causing established design elements to disappear.

After backfill, MSF becomes compositional:
- architecture is a frozen substrate;
- packing is one optimization layer;
- postmix presentation is another orthogonal layer;
- signal-basis compression is the current optimization target.

That makes every future measured gain attributable to one controlled change.
