from __future__ import annotations

from dataclasses import dataclass

from .qstate import QFormation

WIDTH = 256
HEIGHT = 256
POSITIONS = WIDTH * HEIGHT
ACTIVE_BYTES = POSITIONS // 4
ALLOC_BYTES = POSITIONS // 8


def relation_index(x: int, y: int) -> int:
    x = int(x)
    y = int(y)
    if not 0 <= x < WIDTH or not 0 <= y < HEIGHT:
        raise IndexError((x, y))
    return y * WIDTH + x


def relation_xy(index: int) -> tuple[int, int]:
    index = int(index)
    if not 0 <= index < POSITIONS:
        raise IndexError(index)
    return index % WIDTH, index // WIDTH


@dataclass
class QNode256:
    """Fixed 256x256 native Q-position node."""
    active: bytearray
    allocated: bytearray

    @classmethod
    def void(cls) -> "QNode256":
        return cls(bytearray(ACTIVE_BYTES), bytearray(ALLOC_BYTES))

    @classmethod
    def origin(cls) -> "QNode256":
        node = cls.void()
        node.allocated[:] = b"\xff" * ALLOC_BYTES
        node.active[:] = b"\x55" * ACTIVE_BYTES
        return node

    def _alloc_get(self, i: int) -> bool:
        return bool((self.allocated[i >> 3] >> (i & 7)) & 1)

    def _alloc_set(self, i: int, value: bool) -> None:
        b = i >> 3
        mask = 1 << (i & 7)
        if value:
            self.allocated[b] |= mask
        else:
            self.allocated[b] &= (~mask) & 0xFF

    def is_allocated(self, x: int, y: int) -> bool:
        return self._alloc_get(relation_index(x, y))

    def allocate(
        self,
        x: int,
        y: int,
        formation: QFormation = QFormation.PRIMED_ZERO,
    ) -> None:
        i = relation_index(x, y)
        self._alloc_set(i, True)
        self.set_formation(x, y, formation)

    def deallocate(self, x: int, y: int) -> None:
        self._alloc_set(relation_index(x, y), False)

    def get_formation(self, x: int, y: int) -> QFormation:
        i = relation_index(x, y)
        if not self._alloc_get(i):
            raise ValueError("structural VOID")
        shift = (i & 3) * 2
        return QFormation((self.active[i >> 2] >> shift) & 0b11)

    def set_formation(self, x: int, y: int, formation: QFormation) -> None:
        i = relation_index(x, y)
        if not self._alloc_get(i):
            raise ValueError("structural VOID")
        code = int(QFormation(formation))
        b = i >> 2
        shift = (i & 3) * 2
        mask = 0b11 << shift
        self.active[b] = (self.active[b] & (~mask & 0xFF)) | (code << shift)

    @property
    def allocated_count(self) -> int:
        return sum(int(b).bit_count() for b in self.allocated)

    @property
    def is_void(self) -> bool:
        return self.allocated_count == 0

    @property
    def is_pi0(self) -> bool:
        if self.is_void:
            return False
        for i in range(POSITIONS):
            if not self._alloc_get(i):
                continue
            shift = (i & 3) * 2
            if ((self.active[i >> 2] >> shift) & 0b11) != int(QFormation.PRIMED_ZERO):
                return False
        return True

    def clone(self) -> "QNode256":
        return QNode256(bytearray(self.active), bytearray(self.allocated))

    def host_storage_bytes(self) -> int:
        return len(self.active) + len(self.allocated)
