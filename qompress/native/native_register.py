from __future__ import annotations

from dataclasses import dataclass
from typing import Sequence, Tuple

from qompress.senary.senary_machine import (
    QState,
    REFLEX_DIGIT,
    STATE_FROM_REFLEX_DIGIT,
)


def zigzag_encode(value: int) -> int:
    value = int(value)
    return value * 2 if value >= 0 else (-value * 2) - 1


def zigzag_decode(code: int) -> int:
    code = int(code)
    if code < 0:
        raise ValueError("zigzag code must be nonnegative")
    return code // 2 if code % 2 == 0 else -((code + 1) // 2)


def integer_to_qstates(value: int) -> Tuple[QState, ...]:
    """
    Canonical mechanical base-4 sequence for one signed whole integer.

    The integer is mapped bijectively to N by zig-zag encoding, written base 4,
    most-significant digit first, and every digit is realized as one active Q
    formation. Zero is exactly one PRIMED_ZERO Q-cell.
    """
    n = zigzag_encode(value)
    if n == 0:
        return (STATE_FROM_REFLEX_DIGIT[0],)

    digits = []
    while n:
        digits.append(n & 0b11)
        n >>= 2
    digits.reverse()
    return tuple(STATE_FROM_REFLEX_DIGIT[d] for d in digits)


def qstates_to_integer(states: Sequence[QState]) -> int:
    if not states:
        raise ValueError("an integer register must contain at least one Q-cell")
    code = 0
    for q in states:
        code = code * 4 + REFLEX_DIGIT[QState(q)]

    if len(states) > 1 and REFLEX_DIGIT[QState(states[0])] == 0:
        raise ValueError("noncanonical leading primed-zero digit")

    return zigzag_decode(code)


@dataclass(frozen=True)
class NativeFieldState:
    """
    Terminal Q-field represented as native mechanical Q-registers.

    One register is used per mathematical field coordinate. Register boundaries
    belong to fixed machine topology; no integer value is stored beside the
    Q-state sequence.
    """
    registers: Tuple[Tuple[QState, ...], ...]

    @classmethod
    def from_coordinates(cls, coords: Sequence[int]) -> "NativeFieldState":
        if not coords:
            raise ValueError("field must contain at least one coordinate")
        return cls(tuple(integer_to_qstates(v) for v in coords))

    @property
    def coordinates(self) -> Tuple[int, ...]:
        return tuple(qstates_to_integer(r) for r in self.registers)

    @property
    def occupied_qcells(self) -> int:
        return sum(len(r) for r in self.registers)

    @property
    def host_bits_at_two_bits_per_active_qcell(self) -> int:
        return 2 * self.occupied_qcells

    def flattened(self) -> Tuple[QState, ...]:
        return tuple(q for reg in self.registers for q in reg)
