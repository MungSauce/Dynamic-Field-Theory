# Epsilonic Tonal Compression (ETC) v0.1

## Objective

Test whether canonical enwik9 can be losslessly represented as a machine-readable MP3 in which each of the corpus's 206 observed byte values maps to one deterministic audible/signal symbol.

ETC is not credited for merely renaming bytes as sound. A candidate passes only if:

MP3 + ETC decoder + required mapping/metadata
-> deterministic audio decode
-> 1,000,000,000 reconstructed source bytes
-> canonical SHA-256 exactly.

Canonical enwik9:
- bytes: 1,000,000,000
- SHA-256: 159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc
- MD5: e206c3450ac99950df65bf70ef61a12d
- observed source alphabet: 206 byte values.

## ETC symbol families

The first sweep tests several literal one-character/one-sound encodings:

1. LEVEL
   - one fixed carrier/noise basis;
   - each source character selects one of 206 amplitude levels;
   - symbol duration is S PCM samples.

2. TONE
   - each source character selects one of 206 deterministic tonal signatures;
   - signatures use the same synthesis law with character-index-selected parameter;
   - symbol duration is S PCM samples.

3. NOISE-SIGNATURE
   - each source character selects one deterministic short pseudo-noise signature;
   - all 206 signatures are generated from a fixed generic formula and index;
   - no corpus-specific waveform dictionary is hidden.

The lexicon is explicit and counted. For canonical enwik9 it is the sorted set of 206 byte values (206 bytes).

## Channel calibration

MP3 is lossy, so ETC includes a fixed generic calibration preamble. The preamble contains known generated symbol sequences and is independent of enwik9 content. The tonal decoder uses it only to estimate MP3 channel effects such as amplitude scaling, delay, and quantization.

If a source-dependent correction table is required, it counts toward the carrier.

## Search

The GitHub harness sweeps:
- samples per source symbol;
- MP3 sample rate;
- MP3 bitrate;
- ETC symbol family.

Candidates are tested progressively:
- short prefix;
- larger held-out prefix;
- full canonical 1 GB only after zero-error prefix gates.

Primary measurement:
- MP3 bytes;
- ETC decoder bytes;
- explicit lexicon/metadata bytes;
- any required MP3 recovery binary/library bytes for Hutter-oriented accounting.

## Acceptance

An ETC candidate is exact only when the full reconstructed 1 GB source matches canonical SHA-256.

Lossy-but-readable human audio does not count.
A low bit-error-rate does not count.
A deterministic correction stream may make a candidate exact, but every correction bit counts.

## Research question

Can the lossy MP3 representation preserve 206 machine-distinguishable tonal symbols at a sufficiently high symbol rate that:

complete ETC representation < current compression frontier?

If not, record the minimum exact bitrate/symbol duration boundary and failure mode rather than treating a lossy near-match as success.
