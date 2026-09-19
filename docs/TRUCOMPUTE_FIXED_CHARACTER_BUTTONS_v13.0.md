# TruCompute Fixed Character-Button Architecture v13

The 206 character buttons are permanent machine structure.

They all exist before source access:

\`\`\`
C0 ... C205
\`\`\`

Their identities and coupling never change.

The source parser cannot:

- create a new character button;
- label a button from source data;
- remap one button to another symbol;
- store a terminal map;
- bypass the button interface and write a hidden character code directly into a node.

## Write operation

For each incoming machine symbol:

\`\`\`
(page, position) = chronology
button = fixed_keyboard.press(symbol)
node[position].tune(page, button)
\`\`\`

The source changes only the native resistor-node transfer imprint.

## Read operation

For a selected page:

\`\`\`
for C0 ... C205:
    press fixed button
    observe all continuously-powered nodes
\`\`\`

At every valid position exactly one button produces POS and the other 205 produce NEG.

## Final product

The frozen product is the hand-tuned resistor field.

The 206-button keyboard is generic permanent machinery and contains no source-dependent state.

The current fixed-width transfer law is still only a bounded reference law. It may be replaced without changing the permanent-button architecture.

## Raw-source frontend boundary

v13 defines the native machine alphabet as button identities 0..205.

Any adapter from an external byte/text encoding into these 206 native identities must itself be fixed before source access. A source-derived adapter would be source-dependent state and cannot be hidden outside the measured artifact.
