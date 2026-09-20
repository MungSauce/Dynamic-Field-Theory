from dataclasses import dataclass
from .chronology_v2 import PRIMED_ZERO_NODE

@dataclass(frozen=True)
class ReflexDelta:
    position: int
    xor_delta: int

class ReflexField:
    """Fixed-width field beginning at canonical all-primed-zero."""
    def __init__(self, width: int, state: bytes | None = None):
        if width <= 0:
            raise ValueError("width must be positive")
        self.width = int(width)
        if state is None:
            self.state = bytearray([PRIMED_ZERO_NODE] * self.width)
        else:
            if len(state) != self.width:
                raise ValueError("state length mismatch")
            self.state = bytearray(state)

    def is_primed_zero(self) -> bool:
        return all(v == PRIMED_ZERO_NODE for v in self.state)

    def button(self, position: int, value: int) -> bool:
        if not 0 <= position < self.width:
            raise IndexError(position)
        if not 0 <= value <= 255:
            raise ValueError(value)
        return self.state[position] == value

    def ingest(self, page: bytes):
        if len(page) != self.width:
            raise ValueError("page width mismatch")
        deltas = []
        for i, x in enumerate(page):
            d = self.state[i] ^ x
            if d:
                deltas.append(ReflexDelta(i, d))
                self.state[i] ^= d
        return deltas

    def apply(self, deltas):
        seen = -1
        for d in deltas:
            if d.position <= seen or d.position >= self.width:
                raise ValueError("noncanonical delta order")
            if not 1 <= d.xor_delta <= 255:
                raise ValueError("zero/invalid delta")
            self.state[d.position] ^= d.xor_delta
            seen = d.position

    def rollback(self, deltas):
        for d in reversed(deltas):
            self.state[d.position] ^= d.xor_delta

    def snapshot(self) -> bytes:
        return bytes(self.state)
