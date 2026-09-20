from enum import IntEnum

class QState(IntEnum):
    NEG = 0
    PRIMED_ZERO = 1
    DECAY_ZERO = 2
    POS = 3

def pack_node(cells):
    if len(cells) != 4:
        raise ValueError("a TruQ node contains exactly four Q-cells")
    out = 0
    for lane, cell in enumerate(cells):
        value = int(cell)
        if not 0 <= value <= 3:
            raise ValueError("Q-cell code out of range")
        out |= value << (2 * lane)
    return out

def unpack_node(value):
    if not 0 <= int(value) <= 0xFF:
        raise ValueError("node byte out of range")
    value = int(value)
    return tuple(QState((value >> (2 * lane)) & 0x3) for lane in range(4))
