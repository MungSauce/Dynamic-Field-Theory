# TruCompression Dormant-Imprint Architecture v3.0

**Date:** 2026-09-18  
**Status:** CANDIDATE CANONICAL / REFERENCE BACKEND  
**Branch:** \`trucompression-dormant-imprint-v7-20260918\`  
**Implementation:** \`trucompression/trucompression_hmc_v7.cpp\`

## Correction

The frozen TruCompression artifact must not persist expanded consequences of page and character selection.

The fixed machine is generic:

- 1,000,000 position nodes;
- 1,000 page selectors;
- 206 character selectors;
- one TRUE/FALSE/NO-SIGNAL runner;
- fixed selector wiring and sweep logic.

Those mechanisms do not become 206 source-dependent stored configurations merely because the machine can be interrogated in 206 character modes.

The source-dependent frozen artifact is exactly one dormant relation program.

At runtime:

\`\`\`
RelationProgram + page + position + highlighted_character
    -> YES / NO / NO SIGNAL
\`\`\`

The 206 character buttons are input conditions to one runner.

## Artifact boundary

Counted:

- fixed artifact header;
- every source-dependent byte in the dormant relation program;
- any source-dependent terminal mapping.

Not duplicated into the artifact:

- generic HMC runner code;
- fixed page/character selector mechanics;
- fixed million-node sweep machinery;
- transient live page/node state;
- temporary imprint solver state.

No source-dependent item may be moved into the generic runtime.

## v7 reference backend

v7 initially uses a DEFLATE stream only as a reference dormant-program backend.

This backend is not claimed as a novel TruCompression compression law. Its purpose is to validate the corrected architecture and artifact boundary:

1. exactly one source-dependent dormant payload;
2. zero persisted character-selector configurations;
3. zero page banks;
4. source deletion before replay;
5. generic page/character/node runner reconstructs the source.

The relation-program interface is intentionally separable so a TruCompute-native imprint law can replace DEFLATE without modifying HMC semantics.

## HMC semantics

For active page q and highlighted character c:

\`\`\`
observe(N_i) =
    YES       if active terminal at i equals c
    NO        if active terminal at i differs from c
    NO SIGNAL if i is outside the source or selector state is invalid
\`\`\`

The machine enters RUN once and remains live through selector changes until DONE.

## Measurement rule

The TruCompression file-size measurement is:

\`\`\`
stat(frozen dormant artifact)
\`\`\`

after the original source is deleted, provided the generic runtime can reconstruct the source exactly and match its canonical hash.

The number of selector modes is not multiplied into persistent storage unless source-dependent bytes actually exist for those modes.
