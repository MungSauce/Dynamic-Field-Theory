from __future__ import annotations

from enum import Enum, IntEnum


class QFormation(IntEnum):
    """Four active native Q formations using proven Q-OS host codes."""
    NEG = 0b00
    PRIMED_ZERO = 0b01
    DECAY_ZERO = 0b10
    POS = 0b11


class QSemantic(Enum):
    """Six machine-level semantic conditions."""
    VOID = "Ø"
    NEG = "-"
    PRIMED_ZERO = "-+"
    DECAY_ZERO = "+-"
    POS = "+"
    PI0 = "Π0"


SYMBOL = {
    QFormation.NEG: "-",
    QFormation.PRIMED_ZERO: "-+",
    QFormation.DECAY_ZERO: "+-",
    QFormation.POS: "+",
}

FORMATION_FROM_SYMBOL = {v: k for k, v in SYMBOL.items()}

# Native integer digit order is not host code order. Primed Zero is digit zero
# so the all-primed-zero register has numeric value zero.
INTEGER_DIGIT = {
    QFormation.PRIMED_ZERO: 0,
    QFormation.NEG: 1,
    QFormation.DECAY_ZERO: 2,
    QFormation.POS: 3,
}
FORMATION_FROM_INTEGER_DIGIT = {v: k for k, v in INTEGER_DIGIT.items()}
