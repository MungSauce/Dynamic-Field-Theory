# TruCompute Fixed Keyboard v13 — Conformance Result

**Date:** 2026-09-18  
**Status:** MEASURED PASS  
**Workflow run:** 35418884618

## Permanent machine boundary

All 206 character buttons exist before source access.

```
physical_character_buttons=206
all_buttons_exist_before_source=true
button_identity_source_tuned=false
button_coupling_source_tuned=false
source_dependent_button_state_bytes=0
separate_terminal_map=false
```

The source parser may only press one of those already-existing buttons.

The source cannot create, label, remap, or otherwise alter the character-button bank.

## Write path

For each incoming native symbol:

```
(page, position) = chronology
button = permanent_keyboard.press(symbol)
node[position].tune_under_selected_buttons(page, button)
```

Only the resistor-node transfer imprint changes.

## Read path

For each page, all 206 permanent character buttons are pressed in turn.

Every data node responds POS or NEG.

Each position must have exactly one POS button.

## Source-isolated replay

An eight-page / 64-position native-symbol fixture was typed through the permanent keyboard.

Measured freeze:

```
source_symbols=512
pages=8
node_count=64
physical_character_buttons=206
all_buttons_exist_before_source=true
source_dependent_button_state_bytes=0
separate_terminal_map=false
page_buffer_bytes=0
source_copy_bytes_during_imprint=0
page_response_table=false
character_response_table=false
residual_bytes=0
finished_product=HAND_TUNED_RESISTOR_FIELD
prefreeze_exact_replay=PASS
artifact_bytes=1054
```

The source was deleted.

Cold replay:

```
status=FIXED_KEYBOARD_EXACT_REPLAY_PASS
recovered_symbols=512
physical_character_buttons=206
source_dependent_button_state_bytes=0
separate_terminal_map=false
artifact_bytes=1054
```

Recovered SHA-256 matched the deleted source exactly.

## Current boundary

The keyboard architecture is now fixed.

The remaining research target is the node transfer/imprint law.

The current bounded eight-channel reference law remains deliberately replaceable and must not grow storage when it reaches capacity.

For raw external byte/text corpora, any translation into the native 206-button alphabet must be fixed before source access or explicitly counted as source-dependent state.
