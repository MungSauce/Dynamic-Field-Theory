# Q-OS QBIN ABI v0.1

Status: CANONICAL APPLICATION FORMAT
Date: 2026-09-19

QBIN is the first executable format targeting Q-OS. It is deliberately small: it describes Q-memory allocation, topology, external strikes, structural severing, and settlement. There is no conditional branch opcode.

All integers are little-endian.

| Opcode | Name | Payload |
|---|---|---|
| `0x01` | NODE | `u16 node, u8 qstate` |
| `0x02` | EDGE | `u16 from, u16 to, u8 mode` |
| `0x03` | STRIKE | `u16 node, i8 polarity` |
| `0x04` | SEVER_NODE | `u16 node` |
| `0x05` | SEVER_EDGE | `u16 from, u16 to` |
| `0x06` | SETTLE | `u32 event_budget` |
| `0x07` | EXPECT | `u16 node, u8 qstate` |
| `0xff` | END | none |

Q-state codes are fixed by Q-OS:

- `0` = `-`
- `1` = `-+`
- `2` = `+-`
- `3` = `+`

Edge modes:

- `0` = CASCADE
- `1` = DEEPEN
- `2` = CANCEL

Strike polarity is signed: `+1` positive, `-1` negative.

`EXPECT` is a conformance/debug directive. It does not introduce conditional control flow; it checks the terminal state produced by the topology.

A QBIN module is independent of the kernel binary. The reference Multiboot2 loader passes the module to Q-OS at boot, and Q-OS executes it through the same QBIN interpreter that hosted conformance tests exercise.
