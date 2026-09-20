from __future__ import annotations

from .qnode import HEIGHT, POSITIONS, WIDTH


def to_index(x: int, y: int) -> int:
    x = int(x); y = int(y)
    if not 0 <= x < WIDTH or not 0 <= y < HEIGHT:
        raise IndexError((x, y))
    return y * WIDTH + x


def to_xy(index: int) -> tuple[int, int]:
    index = int(index)
    if not 0 <= index < POSITIONS:
        raise IndexError(index)
    return index % WIDTH, index // WIDTH
