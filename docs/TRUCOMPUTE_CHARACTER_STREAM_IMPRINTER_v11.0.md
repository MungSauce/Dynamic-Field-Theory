# TruCompute Character-Stream Imprinter v11

The source writer is now a character-by-character parser directly connected to the native resistor field.

For each source byte at absolute offset t:

\`\`\`
page     = floor(t / node_count)
position = t mod node_count
symbol   = terminal_map(byte)

parser.push(byte)
    -> field.node(position)
    -> node.imprint_symbol(page, symbol)
\`\`\`

The parser does not retain a page, a source copy, page response tables, character response tables, or residual bytes.

Each node retains only its pre-sized native transfer imprint.

The reference transfer basis is Newton-form so a character arriving for page q can update coefficient q without altering any earlier page response. Once q reaches the fixed channel count, no channel remains. Later characters must already be implied by the fixed transfer law or imprint fails with CAPACITY_EXCEEDED.

This preserves the original CSS intent—chronological source exposure writes directly into the same physical/logical node population—while using the corrected TruCompute node architecture.

The terminal map is source-dependent metadata and is counted in the frozen artifact.

The current 8-channel transfer law is only a reference law. It is deliberately bounded and is expected to fail on arbitrary sources beyond its representational capacity. Failure must never trigger storage growth.
