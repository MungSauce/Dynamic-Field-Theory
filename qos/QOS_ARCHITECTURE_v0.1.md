# Q-OS Architecture v0.1

Status: BOOTABLE PREREQUISITE KERNEL
Date: 2026-09-19

## Position

Q-OS is an operating system. It is not part of TruCompute, trueCSS, EIS, Epsilon, Terrorium, or compression. Those systems may later target Q-OS through its stable quaternary execution interface.

The first hardware target is conventional x86 silicon. Binary is permitted only below the Q hardware-abstraction boundary. Q-OS semantics above that boundary are temporal-vector semantics.

## Kernel primitives

1. Four allocated Q-states encoded in two host bits.
2. `Null` represented by deallocation, not by a Q-state code.
3. Packed state RAM: four Q-state slots per byte.
4. Separate allocation topology for structural presence.
5. Directed topology edges.
6. Positive and negative strikes.
7. Sparse FIFO delta scheduler: unchanged nodes produce no new work.
8. Deterministic local interaction operators: Cascade, Deepen, Cancel, Sever.
9. QBIN executable format generated from Q-ASM topology descriptions.

## Clock boundary

The x86 host remains physically clocked. Q-OS does not expose a global software tick as its execution primitive. The Q-kernel advances only while Q-events exist.

## Memory model

For allocated node `i`, state is stored in a packed two-bit slot. Structural existence is tracked independently. An unallocated address is `Null` and cannot receive strikes or participate in topology.

This separation is mandatory:

`Null != - != -+ != +- != +`

## Execution model

The kernel queue contains only changes to be attempted:

`event = (node, strike)`

Resolution is local:

`new_state = T(old_state, strike)`

When `new_state == old_state`, execution ends for that path. When a delta occurs, outgoing edges transform and propagate the strike.

## Q-ASM / QBIN

Q-ASM describes geometry rather than conditional control flow. v0.1 directives are:

- `NODE id state`
- `EDGE from to CASCADE|DEEPEN|CANCEL`
- `STRIKE id +|-`
- `SEVER_NODE id`
- `SEVER_EDGE from to`
- `SETTLE [event_budget]`
- `EXPECT id state` (conformance/debug directive)
- `END`

The assembler emits compact QBIN bytecode. The boot kernel executes QBIN directly.

## Boot target

The reference kernel is a freestanding 32-bit x86 Multiboot2 image. GRUB is only the boot transport. After entry, the kernel executes without a host operating system or standard library.

The kernel does not embed the Q program. GRUB loads a separately compiled `boot.qbin` Multiboot2 module; Q-OS discovers that module at boot and executes it through the QBIN loader. The conformance module creates a three-node topology, applies a strike, settles sparse propagation, verifies all three terminal states, and reports through serial output.

## Non-goals for v0.1

Networking/RF is intentionally excluded. No claim is made that commodity RAM physically possesses momentum; the temporal-vector law is the machine semantics implemented by Q-OS.

## Prerequisite completion gate

The Q-OS computational core is considered established when CI demonstrates all of the following from a cold build:

- exact state-code mapping;
- full eight-entry strike table;
- Null/deallocation isolation;
- Cascade and Cancel propagation;
- Q-ASM -> QBIN compilation;
- freestanding kernel link;
- Multiboot2 recognition;
- QEMU boot;
- QBIN execution inside the booted kernel;
- zero failed expectations;
- `QOS_BOOT_CONFORMANCE=PASS` on serial output.
