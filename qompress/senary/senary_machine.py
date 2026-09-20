from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, IntEnum
from typing import Iterable, Optional, Sequence, Tuple


class QState(IntEnum):
    """
    Canonical active Q-OS states.

    Host codes:
        -   -> 00
        -+  -> 01
        +-  -> 10
        +   -> 11
    """
    NEG = 0
    PRIMED_ZERO = 1
    DECAY_ZERO = 2
    POS = 3


SYMBOL = {
    QState.NEG: "-",
    QState.PRIMED_ZERO: "-+",
    QState.DECAY_ZERO: "+-",
    QState.POS: "+",
}


class SenarySemantic(Enum):
    """
    Six substrate-level semantic conditions.

    STRUCTURAL_NULL is local absence. PI0 is a derived whole-field condition.
    Control VOID is deliberately NOT in this enum; it belongs to the execution
    language/history.
    """
    STRUCTURAL_NULL = "∅"
    NEG = "-"
    PRIMED_ZERO = "-+"
    DECAY_ZERO = "+-"
    POS = "+"
    PI0 = "Π0"


REFLEX_DIGIT = {
    QState.PRIMED_ZERO: 0,
    QState.NEG: 1,
    QState.DECAY_ZERO: 2,
    QState.POS: 3,
}
STATE_FROM_REFLEX_DIGIT = {v: k for k, v in REFLEX_DIGIT.items()}


@dataclass(frozen=True)
class SenarySnapshot:
    capacity: int
    allocation_mask: int
    payload: int

    def validate(self) -> None:
        if self.capacity <= 0:
            raise ValueError("capacity must be positive")
        if self.allocation_mask < 0:
            raise ValueError("allocation_mask must be nonnegative")
        if self.payload < 0:
            raise ValueError("payload must be nonnegative")
        if self.allocation_mask >> self.capacity:
            raise ValueError("allocation_mask exceeds machine capacity")

        n = self.allocation_mask.bit_count()
        if self.payload >= 4 ** n:
            raise ValueError("payload exceeds allocated active-state capacity")

    @property
    def allocated_count(self) -> int:
        return self.allocation_mask.bit_count()

    @property
    def is_structural_null(self) -> bool:
        return self.allocation_mask == 0

    @property
    def is_pi0(self) -> bool:
        return self.allocation_mask != 0 and self.payload == 0


class SenaryMachine:
    """
    Sparse substrate with:
      local: ∅, -, -+, +-, +
      global: Π0

    Control VOID is part of the Q execution language, not a local cell value.
    """

    def __init__(self, capacity: int, allocated: Optional[Iterable[int]] = None):
        capacity = int(capacity)
        if capacity <= 0:
            raise ValueError("capacity must be positive")
        self.capacity = capacity
        self._cells: list[Optional[QState]] = [None] * capacity
        if allocated is not None:
            for i in allocated:
                self.allocate(i)

    @classmethod
    def blank(cls, capacity: int) -> "SenaryMachine":
        return cls(capacity)

    @classmethod
    def origin(cls, primitives: int) -> "SenaryMachine":
        if primitives <= 0:
            raise ValueError("origin requires at least one allocated primitive")
        return cls(primitives, range(primitives))

    def _check_index(self, index: int) -> int:
        index = int(index)
        if not 0 <= index < self.capacity:
            raise IndexError(index)
        return index

    def allocate(self, index: int, state: QState = QState.PRIMED_ZERO) -> None:
        index = self._check_index(index)
        self._cells[index] = QState(state)

    def deallocate(self, index: int) -> None:
        index = self._check_index(index)
        self._cells[index] = None

    def set_active(self, index: int, state: QState) -> None:
        index = self._check_index(index)
        if self._cells[index] is None:
            raise ValueError("cannot set active state on structural null")
        self._cells[index] = QState(state)

    def get_active(self, index: int) -> QState:
        index = self._check_index(index)
        q = self._cells[index]
        if q is None:
            raise ValueError("primitive is structural null")
        return q

    def local_semantic(self, index: int) -> SenarySemantic:
        index = self._check_index(index)
        q = self._cells[index]
        if q is None:
            return SenarySemantic.STRUCTURAL_NULL
        return {
            QState.NEG: SenarySemantic.NEG,
            QState.PRIMED_ZERO: SenarySemantic.PRIMED_ZERO,
            QState.DECAY_ZERO: SenarySemantic.DECAY_ZERO,
            QState.POS: SenarySemantic.POS,
        }[q]

    @property
    def allocated_indices(self) -> Tuple[int, ...]:
        return tuple(i for i, q in enumerate(self._cells) if q is not None)

    @property
    def allocated_count(self) -> int:
        return len(self.allocated_indices)

    @property
    def is_structural_null(self) -> bool:
        return self.allocated_count == 0

    @property
    def is_pi0(self) -> bool:
        idx = self.allocated_indices
        return bool(idx) and all(self._cells[i] is QState.PRIMED_ZERO for i in idx)

    @property
    def global_semantic(self) -> Optional[SenarySemantic]:
        if self.is_structural_null:
            return SenarySemantic.STRUCTURAL_NULL
        if self.is_pi0:
            return SenarySemantic.PI0
        return None

    def semantic_tuple(self) -> Tuple[str, ...]:
        return tuple(self.local_semantic(i).value for i in range(self.capacity))

    def snapshot(self) -> SenarySnapshot:
        allocation_mask = 0
        payload = 0
        place = 1
        for i, q in enumerate(self._cells):
            if q is None:
                continue
            allocation_mask |= 1 << i
            payload += REFLEX_DIGIT[q] * place
            place *= 4
        snap = SenarySnapshot(self.capacity, allocation_mask, payload)
        snap.validate()
        return snap

    @classmethod
    def from_snapshot(cls, snapshot: SenarySnapshot) -> "SenaryMachine":
        snapshot.validate()
        m = cls(snapshot.capacity)
        value = snapshot.payload
        for i in range(snapshot.capacity):
            if not (snapshot.allocation_mask >> i) & 1:
                continue
            digit = value & 0b11
            value >>= 2
            m.allocate(i, STATE_FROM_REFLEX_DIGIT[digit])
        if value:
            raise ValueError("noncanonical snapshot payload")
        return m

    def settle_to_directed_zero(self, index: int) -> QState:
        q = self.get_active(index)
        if q is QState.NEG:
            nxt = QState.PRIMED_ZERO
        elif q is QState.POS:
            nxt = QState.DECAY_ZERO
        else:
            raise ValueError("only polarized states settle to directed zero")
        self.set_active(index, nxt)
        return nxt

    def reverse_directed_zero(self, index: int) -> QState:
        q = self.get_active(index)
        if q is QState.PRIMED_ZERO:
            prev = QState.NEG
        elif q is QState.DECAY_ZERO:
            prev = QState.POS
        else:
            raise ValueError("only directed-zero states have primitive reflex")
        self.set_active(index, prev)
        return prev


def pack_node(cells: Sequence[QState]) -> int:
    if len(cells) != 4:
        raise ValueError("a TruQ node contains exactly four active Q cells")
    out = 0
    for lane, cell in enumerate(cells):
        out |= int(QState(cell)) << (2 * lane)
    return out


def unpack_node(value: int) -> Tuple[QState, QState, QState, QState]:
    value = int(value)
    if not 0 <= value <= 0xFF:
        raise ValueError("node byte out of range")
    return tuple(QState((value >> (2 * lane)) & 0x3) for lane in range(4))
