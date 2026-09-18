# MSF Recording v4 — SUPERSEDED / INVALID AS CURRENT SIGNAL MODEL

Status: **SUPERSEDED**

This experiment attempted to force one scalar value per timestamp by radix-packing the complete set of instrument states into one arbitrarily wide integer.

That does **not** satisfy the current MSF recording definition. It is a state snapshot encoded as a scalar, not an audio-adjacent sampled composite field. It is retained only as falsification/lineage evidence.

Do not use its byte results as MSF signal-compression results.

Current implementation line: `msf_recording_v32/`

The current line instead uses:
- one page per instrument;
- two simultaneous directional notes;
- one shared lexicon;
- deterministic Walsh/DSSS-like instrument signatures;
- literal superposition into a composite sample field;
- deterministic PRN chip scrambling after mixing;
- an Audience that despreads and matched-filters the recording.
