# TruCompute One-Hot Activator Native Field v11

**Date:** 2026-09-18  
**Status:** CANDIDATE NATIVE ARCHITECTURE

## Control law

Page and character controls are fixed one-hot button banks.

Pressing a button performs one control operation:

\`\`\`
pressed button -> TRUE
every peer button in that bank -> FALSE
\`\`\`

Therefore:

\`\`\`
page bank      = 1 TRUE + 999 FALSE
character bank = 1 TRUE + 205 FALSE
\`\`\`

There is no third runtime label in either bank.

The button banks are generic machine wiring and are not source-dependent storage.

## Data field law

Every data node is continuously powered.

For the page button and character button that are currently TRUE, every data node returns exactly one Boolean answer:

\`\`\`
TRUE  = selected page has selected character at this position
FALSE = selected page does not have selected character at this position
\`\`\`

No active data-node observation returns BOTH or NEITHER.

The unresolved/native field interpretation may motivate the hardware model, but it is not serialized or exposed as another operational node value.

## Native resistor implementation

The v11 reference node is a fixed-width condition-sensitive resistor element.

A page button selects the node's transfer condition.  
A character button probes that condition.

\`\`\`
matching page resonance -> enough effective resistance -> TRUE
nonmatching resonance    -> no effective resistance     -> FALSE
\`\`\`

The source-dependent node state is only a fixed-width transfer imprint.

No page-response table, character-response table, selector-response cache, residual stream, or dynamically growing channel set is permitted.

## Fixed-capacity rule

The imprint width is fixed before source access.

If the selected transfer law cannot represent the source exactly within that width, imprint must terminate with:

\`\`\`
CAPACITY_EXCEEDED
\`\`\`

It may not allocate additional state.

## Artifact boundary

Frozen source-dependent artifact:

- architecture header;
- fixed-width transfer imprint for each data node.

Excluded as generic runtime:

- 1,000 page buttons;
- 206 character buttons;
- one-hot label switching law;
- data-node probe law;
- sweep controller.

This prevents runtime button states from being counted as source storage while also preventing source information from being hidden in the control wiring.
