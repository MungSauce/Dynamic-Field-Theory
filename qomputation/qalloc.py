from __future__ import annotations

from dataclasses import dataclass

from .qnode import POSITIONS, QNode256
from .qstate import QFormation


@dataclass
class QAllocator:
    node: QNode256

    def allocate_first(
        self,
        formation: QFormation = QFormation.PRIMED_ZERO,
    ) -> tuple[int, int]:
        for i in range(POSITIONS):
            x = i & 0xFF
            y = i >> 8
            if not self.node.is_allocated(x, y):
                self.node.allocate(x, y, formation)
                return x, y
        raise MemoryError("QNode256 is fully allocated")

    def release(self, x: int, y: int) -> None:
        self.node.deallocate(x, y)

    @property
    def free_count(self) -> int:
        return POSITIONS - self.node.allocated_count
