# MSF Recording v3.2 — spread-spectrum composite recording

This revision implements the current design as an audio/radio-adjacent machine signal.

- Orchestra/Encoder: one page per instrument; two simultaneous directional notes; one shared lexicon.
- Instrument identity: deterministic Walsh spreading signature (DSSS/CDMA-like timbre).
- Composite recording: all instruments are superposed into one baseband field.
- Postmix field: deterministic PRN chip scrambling makes the retained signal noise-like.
- .msf payload: only packed composite signal amplitudes.
- Audience/Decoder: PRN despreading/descrambling, matched filtering against the Walsh instrument bank, shared-lexicon interpretation, page reconstruction.

The PRN layer is not cryptographic encryption. It makes the waveform non-lane-readable without the generic Audience law; secrecy is a separate concern.

Primary compression measurement remains recorded signal payload bytes / source bytes.
