# TruCompute Keyboard-Imprint Architecture v12

The source parser and the decompressor now share one physical/logical interface: the fixed bank of 206 character buttons.

## Write path

For each incoming source character:

1. read one raw character;
2. locate or hand-tune the matching physical character button;
3. derive current page and position from chronology;
4. press that character button;
5. tune only the current continuously powered resistor node under that page/button condition;
6. advance one position.

No encoded terminal stream is created.

No separate terminal map exists.

The character buttons themselves carry their tuned labels, so any source-dependent labelling is part of the final machine and must be serialized/countable in a software implementation.

## Read path

For each page:

1. select the page;
2. press each of the 206 physical character buttons;
3. all data nodes resolve NEG/POS;
4. each position has exactly one POS response;
5. the label on that winning physical button is the recovered source character.

The writer and reader therefore use the same keyboard.

## Final product

The output of imprinting is not a payload interpreted by a separate machine.

The output is the hand-tuned machine:

- fixed character-button bank, including any tuned labels;
- fixed page-selector bank;
- fixed population of resistor nodes;
- each resistor node's bounded transfer imprint.

## No-growth rule

The current reference node still has a fixed 8-channel transfer law.

The character parser is not allowed to create page records, character records, exceptions, residual streams, or extra channels.

When the fixed native transfer law can no longer incorporate the next typed character, imprint must terminate with CAPACITY_EXCEEDED.

The transfer law remains replaceable research. The keyboard/write/read architecture is the intended invariant.
