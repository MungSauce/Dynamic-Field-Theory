# MSF Recording Architecture v3

This directory implements the current three-object model literally:

1. **Orchestra / Encoder** performs the source.
2. **Recording / .msf** is the retained composite signal.
3. **Audience / Decoder** listens only to the recording.

Each instrument owns one deterministic page. Every page is read inward from both ends. Every instrument uses the same source-symbol lexicon; forward and reverse are the two directional note roles, giving at most 412 directional note identities for a 206-symbol alphabet.

Instrument identity is a procedural Walsh signature. Think of the same note source passed through a different deterministic synth/effect chain. At a timestamp the orchestra multiplies each instrument's two note coordinates by its signature and superposes every instrument. The .msf stores only those resulting composite complex machine samples.

The Audience regenerates the same signatures and correlates against the recording. It does not import the orchestra encoder and does not receive a serialized list of instrument states.

For N instruments:
- pages = N
- source roles per full timestamp = 2N
- track length ~= L/(2N)
- recording samples per timestamp = N complex machine samples in this reference realization

The last line is deliberately measured rather than hidden: this reference signal basis pays N stored samples per timestamp. The experiment establishes whether later signal/transport refinements can lower that sample cost while preserving exact separability.
