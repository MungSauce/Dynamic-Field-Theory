from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Iterator, Tuple

from qompress.senary.directed_integer import DirectedQInteger


WIDTH = 256
HEIGHT = 256
POSITIONS = WIDTH * HEIGHT


@dataclass(frozen=True)
class NativeNodeStats:
    positions: int
    non_origin_positions: int
    active_nonzero_positions: int
    total_active_magnitude: int


class QNode256:
    """
    Native 256 x 256 Q-node.

    The full node is logically allocated. Primed zero 0[-+] is the canonical
    origin at every position. The emulator stores only deviations from origin,
    but that sparsity is an implementation detail, not a change to node size.

    A terminal node is completely determined by the final DirectedQInteger at
    all 65,536 positions.
    """

    width = WIDTH
    height = HEIGHT
    positions = POSITIONS

    def __init__(self) -> None:
        self._overrides: Dict[int, DirectedQInteger] = {}

    @staticmethod
    def index(x: int, y: int) -> int:
        x = int(x)
        y = int(y)
        if not 0 <= x < WIDTH or not 0 <= y < HEIGHT:
            raise IndexError((x, y))
        return y * WIDTH + x

    @staticmethod
    def xy(index: int) -> Tuple[int, int]:
        index = int(index)
        if not 0 <= index < POSITIONS:
            raise IndexError(index)
        return (index % WIDTH, index // WIDTH)

    def get_index(self, index: int) -> DirectedQInteger:
        index = int(index)
        if not 0 <= index < POSITIONS:
            raise IndexError(index)
        return self._overrides.get(index, DirectedQInteger.primed_zero())

    def set_index(self, index: int, value: DirectedQInteger) -> None:
        index = int(index)
        if not 0 <= index < POSITIONS:
            raise IndexError(index)
        if value == DirectedQInteger.primed_zero():
            self._overrides.pop(index, None)
        else:
            self._overrides[index] = value

    def get(self, x: int, y: int) -> DirectedQInteger:
        return self.get_index(self.index(x, y))

    def set(self, x: int, y: int, value: DirectedQInteger) -> None:
        self.set_index(self.index(x, y), value)

    @property
    def is_pi0(self) -> bool:
        return not self._overrides

    def iter_non_origin(self) -> Iterator[Tuple[int, DirectedQInteger]]:
        for i in sorted(self._overrides):
            yield i, self._overrides[i]

    def stats(self) -> NativeNodeStats:
        nonzero = 0
        magnitude = 0
        for v in self._overrides.values():
            if v.value != 0:
                nonzero += 1
                magnitude += v.magnitude
        return NativeNodeStats(
            positions=POSITIONS,
            non_origin_positions=len(self._overrides),
            active_nonzero_positions=nonzero,
            total_active_magnitude=magnitude,
        )

    def terminal_configuration(self) -> Tuple[Tuple[int, int, str], ...]:
        """
        Sparse canonical view of the terminal configuration.

        Each tuple is:
            (position index, whole-number magnitude, formation)

        formation is -, +, -+, or +-.
        Primed-zero positions are implicit because node topology is fixed.
        """
        return tuple(
            (i, v.magnitude, v.formation)
            for i, v in self.iter_non_origin()
        )
