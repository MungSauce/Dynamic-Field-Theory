# MSF Recording v4 — one composite sample per timestamp

This revision fixes the measured v3 error: v3 stored N signal samples per timestamp, which canceled the N-way page/time reduction.

v4 records exactly **one scalar machine-signal sample per timestamp**.

The architecture remains unchanged:
- orchestra/Encoder performs the source;
- one instrument owns one deterministic page;
- every page is read from both ends simultaneously;
- every instrument reuses the same shared directional lexicon (up to 412 note identities);
- instrument identity is a generic procedural modifier/basis;
- only the resulting signal sample is recorded;
- the audience/Decoder receives the recording, not the orchestra state.

The reference basis gives each instrument a procedural positional gain inside one scalar superposition. That makes the sample exactly invertible and lets us measure the actual precision/byte cost of one composite sample rather than hiding N stored samples behind the word "signal".

Primary score = recorded signal payload bytes / source bytes.
