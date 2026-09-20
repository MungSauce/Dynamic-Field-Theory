from __future__ import annotations

from typing import Sequence, Tuple

from .qstate import (
    FORMATION_FROM_INTEGER_DIGIT,
    INTEGER_DIGIT,
    QFormation,
)


def zigzag_encode(value: int) -> int:
    value = int(value)
    return value * 2 if value >= 0 else (-value * 2) - 1


def zigzag_decode(code: int) -> int:
    code = int(code)
    if code < 0:
        raise ValueError("code must be nonnegative")
    return code // 2 if (code & 1) == 0 else -((code + 1) // 2)


def encode_integer(value: int) -> Tuple[QFormation, ...]:
    """
    Canonical signed whole integer as a mechanical sequence of Q formations.
    """
    code = zigzag_encode(value)
    if code == 0:
        return (QFormation.PRIMED_ZERO,)

    digits = []
    while code:
        digits.append(code & 0b11)
        code >>= 2
    digits.reverse()
    return tuple(FORMATION_FROM_INTEGER_DIGIT[d] for d in digits)


def decode_integer(register: Sequence[QFormation]) -> int:
    if not register:
        raise ValueError("empty Q integer register")

    if (
        len(register) > 1
        and INTEGER_DIGIT[QFormation(register[0])] == 0
    ):
        raise ValueError("noncanonical leading primed-zero")

    code = 0
    for formation in register:
        code = code * 4 + INTEGER_DIGIT[QFormation(formation)]
    return zigzag_decode(code)


def qcell_length(value: int) -> int:
    return len(encode_integer(value))
