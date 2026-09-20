from __future__ import annotations

from dataclasses import dataclass

WIDTH = 256
HEIGHT = 256
POSITIONS = WIDTH * HEIGHT
ACTIVE_BYTES = POSITIONS // 4      # four 2-bit Q formations per host byte
ALLOC_BYTES = POSITIONS // 8       # one allocation bit per position


def relation_index(x: int, y: int) -> int:
    x = int(x); y = int(y)
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
    """
    Fixed 256x256 native node.

    Host representation is an implementation detail:
      - 2 bits per active Q formation
      - 1 allocation bit per position

    Native semantics remain 65,536 Q positions, not a byte array.
    """
    active: bytearray
    allocated: bytearray

    @classmethod
    def void(cls) -> "QNode256":
        return cls(bytearray(ACTIVE_BYTES), bytearray(ALLOC_BYTES))

    @classmethod
    def origin(cls) -> "QNode256":
        node = cls.void()
        node.allocated[:] = b"\xff" * ALLOC_BYTES
        # QState.PRIMED_ZERO host code is 01. Repeated four times => 0b01010101.
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
            self.allocated[b] &= (~mask) & 0xff

    def is_allocated(self, x: int, y: int) -> bool:
        return self._alloc_get(relation_index(x, y))

    def allocate(self, x: int, y: int, host_code: int = 0b01) -> None:
        i = relation_index(x, y)
        self._alloc_set(i, True)
        self.set_code(x, y, host_code)

    def deallocate(self, x: int, y: int) -> None:
        i = relation_index(x, y)
        self._alloc_set(i, False)

    def get_code(self, x: int, y: int) -> int:
        i = relation_index(x, y)
        if not self._alloc_get(i):
            raise ValueError("structural VOID")
        shift = (i & 3) * 2
        return (self.active[i >> 2] >> shift) & 0b11

    def set_code(self, x: int, y: int, code: int) -> None:
        i = relation_index(x, y)
        if not self._alloc_get(i):
            raise ValueError("structural VOID")
        code = int(code)
        if not 0 <= code <= 3:
            raise ValueError("Q formation host code must be 0..3")
        b = i >> 2
        shift = (i & 3) * 2
        mask = 0b11 << shift
        self.active[b] = (self.active[b] & (~mask & 0xff)) | (code << shift)

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
            if self._alloc_get(i):
                shift = (i & 3) * 2
                code = (self.active[i >> 2] >> shift) & 0b11
                if code != 0b01:
                    return False
        return True

    def clone(self) -> "QNode256":
        return QNode256(bytearray(self.active), bytearray(self.allocated))

    def host_storage_bytes(self) -> int:
        return len(self.active) + len(self.allocated)
