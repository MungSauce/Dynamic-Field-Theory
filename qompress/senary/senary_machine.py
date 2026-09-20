from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, IntEnum
from typing import Iterable, Iterator, Optional, Sequence, Tuple


class QState(IntEnum):
    """
    Canonical active Q-OS states.

    Host codes are preserved from the proven Q-OS baseline:
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
    Six machine-level semantic conditions.

    IMPORTANT:
    PI0 is not an independent local storage digit. It is a derived whole-field
    condition: every allocated primitive is PRIMED_ZERO.
    """
    VOID = "Ø"
    NEG = "-"
    PRIMED_ZERO = "-+"
    DECAY_ZERO = "+-"
    POS = "+"
    PI0 = "Π0"


# QReflex/terminal-state digit ordering. This is deliberately different from
# host two-bit codes so that all-primed-zero serializes to integer 0.
REFLEX_DIGIT = {
    QState.PRIMED_ZERO: 0,
    QState.NEG: 1,
    QState.DECAY_ZERO: 2,
    QState.POS: 3,
}
STATE_FROM_REFLEX_DIGIT = {v: k for k, v in REFLEX_DIGIT.items()}


@dataclass(frozen=True)
class SenarySnapshot:
    """
    Canonical whole-number snapshot of a sparse senary machine.

    capacity is a fixed machine parameter, not document entropy when the
    executable fixes it.

    allocation_mask:
        bit i == 1 iff primitive i is allocated.

    payload:
        allocated active states packed in ascending primitive-index order as a
        base-4 whole number using REFLEX_DIGIT. Because PRIMED_ZERO maps to 0,
        an all-primed-zero allocated field has payload == 0.

    This is a machine snapshot primitive, not yet the final Qompression seed
    contract.
    """
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
    def is_void(self) -> bool:
        return self.allocation_mask == 0

    @property
    def is_pi0(self) -> bool:
        # Pi0 requires an allocated field. Empty structural VOID is not Pi0.
        return self.allocation_mask != 0 and self.payload == 0


class SenaryMachine:
    """
    Sparse Q-machine environment with six semantic conditions:

        Ø, -, -+, +-, +, Π0

    Local storage has only five contextual possibilities:
        Ø or one of the four active Q states.

    Π0 is derived globally when every allocated primitive is -+.

    No Qompression compression law is embedded here. This layer exists so later
    reversible/coupled laws have the correct substrate semantics.
    """

    def __init__(
        self,
        capacity: int,
        allocated: Optional[Iterable[int]] = None,
    ) -> None:
        capacity = int(capacity)
        if capacity <= 0:
            raise ValueError("capacity must be positive")
        self.capacity = capacity
        self._cells: list[Optional[QState]] = [None] * capacity

        if allocated is not None:
            for i in allocated:
                self.allocate(i)

    @classmethod
    def void(cls, capacity: int) -> "SenaryMachine":
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

    def allocate(
        self,
        index: int,
        state: QState = QState.PRIMED_ZERO,
    ) -> None:
        index = self._check_index(index)
        self._cells[index] = QState(state)

    def deallocate(self, index: int) -> None:
        index = self._check_index(index)
        self._cells[index] = None

    def set_active(self, index: int, state: QState) -> None:
        index = self._check_index(index)
        if self._cells[index] is None:
            raise ValueError("cannot set active state on structural VOID")
        self._cells[index] = QState(state)

    def get_active(self, index: int) -> QState:
        index = self._check_index(index)
        q = self._cells[index]
        if q is None:
            raise ValueError("primitive is structural VOID")
        return q

    def local_semantic(self, index: int) -> SenarySemantic:
        index = self._check_index(index)
        q = self._cells[index]
        if q is None:
            return SenarySemantic.VOID
        if q is QState.NEG:
            return SenarySemantic.NEG
        if q is QState.PRIMED_ZERO:
            return SenarySemantic.PRIMED_ZERO
        if q is QState.DECAY_ZERO:
            return SenarySemantic.DECAY_ZERO
        if q is QState.POS:
            return SenarySemantic.POS
        raise AssertionError(q)

    @property
    def allocated_indices(self) -> Tuple[int, ...]:
        return tuple(i for i, q in enumerate(self._cells) if q is not None)

    @property
    def allocated_count(self) -> int:
        return len(self.allocated_indices)

    @property
    def is_void(self) -> bool:
        return self.allocated_count == 0

    @property
    def is_pi0(self) -> bool:
        idx = self.allocated_indices
        return bool(idx) and all(
            self._cells[i] is QState.PRIMED_ZERO for i in idx
        )

    @property
    def global_semantic(self) -> Optional[SenarySemantic]:
        if self.is_void:
            return SenarySemantic.VOID
        if self.is_pi0:
            return SenarySemantic.PI0
        return None

    def semantic_tuple(self) -> Tuple[str, ...]:
        """
        Local contextual representation. PI0 is intentionally not emitted as a
        cell value; callers inspect is_pi0/global_semantic for that condition.
        """
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
        """
        Primitive encoding-direction reflex from the v0.2 directed-zero law.

            -  -> -+
            +  -> +-

        Deeper chronology is deliberately not invented here.
        """
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
        """
        Primitive reversal-direction reflex.

            -+ -> -
            +- -> +
        """
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
    """Pack exactly four active Q cells using canonical host 2-bit codes."""
    if len(cells) != 4:
        raise ValueError("a TruQ node contains exactly four active Q cells")
    out = 0
    for lane, cell in enumerate(cells):
        q = QState(cell)
        out |= int(q) << (2 * lane)
    return out


def unpack_node(value: int) -> Tuple[QState, QState, QState, QState]:
    value = int(value)
    if not 0 <= value <= 0xFF:
        raise ValueError("node byte out of range")
    return tuple(
        QState((value >> (2 * lane)) & 0x3)
        for lane in range(4)
    )  # type: ignore[return-value]
