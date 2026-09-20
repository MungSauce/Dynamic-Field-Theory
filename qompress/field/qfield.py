from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Sequence, Tuple


@dataclass(frozen=True, order=True)
class QEvent:
    """One legal signed activation at one 1-based relation location."""
    location: int
    sign: int

    def validate(self, n: int) -> None:
        if not 1 <= self.location <= n:
            raise ValueError(f"location must be in [1,{n}]")
        if self.sign not in (-1, 1):
            raise ValueError("sign must be -1 or +1")


class ControlClass(ValueError):
    """Raised when a state lies in quotient class 0 (reserved for VOID/control)."""


class ReversibleQField:
    """
    Branch-optimal reversible integer Q-field.

    Production-candidate law:
        T_(r,sigma)(X) = U(B_N X + sigma e_r)

    E = 2N+1
    phi(x) = sum(r*x_r) mod E
    U = I + 2*1*v^T

    v is the canonical balanced sign vector (+1 first half, -1 second half).
    """

    def __init__(self, n: int):
        n = int(n)
        if n <= 0 or n % 2:
            raise ValueError("N must be a positive even integer")
        self.n = n
        self.E = 2 * n + 1
        half = n // 2
        self.v = tuple([1] * half + [-1] * half)

    @property
    def origin(self) -> Tuple[int, ...]:
        return (0,) * self.n

    @property
    def events(self) -> Tuple[QEvent, ...]:
        return tuple(
            QEvent(r, s)
            for r in range(1, self.n + 1)
            for s in (-1, 1)
        )

    def _vec(self, x: Sequence[int]) -> Tuple[int, ...]:
        if len(x) != self.n:
            raise ValueError(f"expected vector length {self.n}")
        return tuple(int(a) for a in x)

    def phi(self, x: Sequence[int]) -> int:
        x = self._vec(x)
        return sum((i + 1) * a for i, a in enumerate(x)) % self.E

    def apply_B(self, x: Sequence[int]) -> Tuple[int, ...]:
        x = self._vec(x)
        first = self.E * x[0] - sum(
            (i + 1) * x[i] for i in range(1, self.n)
        )
        return (first,) + x[1:]

    def apply_B_inv_kernel(self, y: Sequence[int]) -> Tuple[int, ...]:
        """Inverse B on vectors known to lie in B Z^N = ker(phi)."""
        y = self._vec(y)
        numerator = y[0] + sum(
            (i + 1) * y[i] for i in range(1, self.n)
        )
        if numerator % self.E:
            raise ValueError("vector is not in B_N lattice / ker(phi)")
        return (numerator // self.E,) + y[1:]

    def _dot_v(self, x: Sequence[int]) -> int:
        x = self._vec(x)
        return sum(a * b for a, b in zip(self.v, x))

    def apply_U(self, x: Sequence[int]) -> Tuple[int, ...]:
        x = self._vec(x)
        delta = 2 * self._dot_v(x)
        return tuple(a + delta for a in x)

    def apply_U_inv(self, y: Sequence[int]) -> Tuple[int, ...]:
        y = self._vec(y)
        # v^T Ux = v^T x because v^T 1 = 0.
        delta = 2 * self._dot_v(y)
        return tuple(a - delta for a in y)

    def add_event(
        self, x: Sequence[int], event: QEvent
    ) -> Tuple[int, ...]:
        event.validate(self.n)
        out = list(self._vec(x))
        out[event.location - 1] += event.sign
        return tuple(out)

    def subtract_event(
        self, x: Sequence[int], event: QEvent
    ) -> Tuple[int, ...]:
        event.validate(self.n)
        out = list(self._vec(x))
        out[event.location - 1] -= event.sign
        return tuple(out)

    def forward(
        self, x: Sequence[int], event: QEvent
    ) -> Tuple[int, ...]:
        event.validate(self.n)
        z = self.add_event(self.apply_B(x), event)
        return self.apply_U(z)

    def event_from_residue(self, k: int) -> QEvent:
        k %= self.E
        if k == 0:
            raise ControlClass(
                "quotient class 0 is reserved for VOID/control"
            )
        if 1 <= k <= self.n:
            return QEvent(k, +1)
        return QEvent(self.E - k, -1)

    def reverse(
        self, y: Sequence[int]
    ) -> Tuple[QEvent, Tuple[int, ...]]:
        z = self.apply_U_inv(y)
        event = self.event_from_residue(self.phi(z))
        lattice = self.subtract_event(z, event)
        return event, self.apply_B_inv_kernel(lattice)

    def wind(self, history: Iterable[QEvent]) -> Tuple[int, ...]:
        x = self.origin
        for event in history:
            x = self.forward(x, event)
        return x

    def unwind(
        self, terminal: Sequence[int], steps: int | None = None
    ):
        y = self._vec(terminal)
        count = 0
        while y != self.origin:
            if steps is not None and count >= steps:
                return
            event, prev = self.reverse(y)
            yield event, prev
            y = prev
            count += 1

    def event_signature(self, event: QEvent) -> int:
        event.validate(self.n)
        u_r = 2 * self.n if event.location == 1 else -event.location
        return event.sign * u_r

    def native_event_impact(
        self, event: QEvent
    ) -> Tuple[int, ...]:
        return self.forward(self.origin, event)
