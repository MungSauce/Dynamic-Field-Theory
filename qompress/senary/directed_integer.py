from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class ZeroDirection(str, Enum):
    MINUS_TO_PLUS = "-+"
    PLUS_TO_MINUS = "+-"


@dataclass(frozen=True)
class DirectedQInteger:
    """
    Native Q numeric value.

    Nonzero values are ordinary signed whole-number counts:
        ..., -3, -2, -1, +1, +2, +3, ...

    Numeric zero has two mechanically distinct formations:
        0[-+]  and  0[+-]

    These zeros compare equal numerically but are distinct machine states.
    """
    value: int
    zero_direction: ZeroDirection | None = None

    def __post_init__(self) -> None:
        v = int(self.value)
        object.__setattr__(self, "value", v)
        if v == 0:
            if self.zero_direction is None:
                raise ValueError("zero requires -+ or +- direction")
            object.__setattr__(
                self, "zero_direction", ZeroDirection(self.zero_direction)
            )
        elif self.zero_direction is not None:
            raise ValueError("nonzero count cannot carry a zero direction")

    @classmethod
    def primed_zero(cls) -> "DirectedQInteger":
        return cls(0, ZeroDirection.MINUS_TO_PLUS)

    @classmethod
    def decay_zero(cls) -> "DirectedQInteger":
        return cls(0, ZeroDirection.PLUS_TO_MINUS)

    @property
    def magnitude(self) -> int:
        return abs(self.value)

    @property
    def formation(self) -> str:
        if self.value < 0:
            return "-"
        if self.value > 0:
            return "+"
        assert self.zero_direction is not None
        return self.zero_direction.value

    @property
    def is_zero(self) -> bool:
        return self.value == 0

    def numerical_eq(self, other: "DirectedQInteger") -> bool:
        return self.value == other.value

    def reverse_from_zero(self) -> "DirectedQInteger":
        """
        Primitive reverse reflex:
            0[-+] -> -1
            0[+-] -> +1
        """
        if self.value != 0:
            raise ValueError("reverse_from_zero requires directed zero")
        if self.zero_direction is ZeroDirection.MINUS_TO_PLUS:
            return DirectedQInteger(-1)
        return DirectedQInteger(+1)

    def settle_one_toward_zero(self) -> "DirectedQInteger":
        """
        One mechanical count step toward zero.

        -1 settles to 0[-+]
        +1 settles to 0[+-]
        Larger magnitudes decrement without losing sign.
        """
        if self.value < -1:
            return DirectedQInteger(self.value + 1)
        if self.value == -1:
            return DirectedQInteger.primed_zero()
        if self.value > 1:
            return DirectedQInteger(self.value - 1)
        if self.value == 1:
            return DirectedQInteger.decay_zero()
        raise ValueError("already at directed zero")
