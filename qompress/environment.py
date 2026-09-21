from __future__ import annotations

from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path
from typing import Iterable, Iterator

WIDTH = 256
HEIGHT = 256
NODE_POSITIONS = WIDTH * HEIGHT
BYTE_ALPHABET = 256
NATIVE_CAPACITY_BITS = 2 * NODE_POSITIONS

# One byte = one Q-turn. All 256 quotient classes are direct byte events.
# No ESC, VOID, or second event is inserted into the byte chronology.
QUOTIENT = BYTE_ALPHABET
MAX_EVENT_RELATION = BYTE_ALPHABET
HALF = NODE_POSITIONS // 2
assert MAX_EVENT_RELATION <= HALF

# Byte-branch-optimal lattice:
# phi(x) = x_1 + sum_{r=2..255} r*x_r (mod 256)
# (Bx)_1 = 256*x_1 - sum_{r=2..255} r*x_r
# (Bx)_r = x_r for r>=2.
# Relations 1..255 occupy residues 1..255; relation 256 occupies residue 0.
def _coeff(relation: int) -> int:
    return relation if 1 <= relation <= 255 else 0


_COEFF_SUM = sum(range(2, 256))
_MAGIC = b"QNPY05\x00\x01"
_SERIALIZED_INTEGER_COUNT = 2 + (MAX_EVENT_RELATION - 1)


class QompressStateError(ValueError):
    pass


def _zigzag_encode(value: int) -> int:
    value = int(value)
    return value << 1 if value >= 0 else ((-value) << 1) - 1


def _zigzag_decode(value: int) -> int:
    return value >> 1 if (value & 1) == 0 else -((value >> 1) + 1)


def _write_uvarint(out: bytearray, value: int) -> None:
    if value < 0:
        raise ValueError("uvarint requires nonnegative integer")
    while value >= 0x80:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    out.append(value)


def _read_uvarint(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while True:
        if offset >= len(data):
            raise QompressStateError("truncated varint")
        b = data[offset]
        offset += 1
        value |= (b & 0x7F) << shift
        if (b & 0x80) == 0:
            return value, offset
        shift += 7
        if shift > max(128, len(data) * 8 + 7):
            raise QompressStateError("invalid varint")


def _signed_varint_size(value: int) -> int:
    u = _zigzag_encode(value)
    size = 1
    while u >= 0x80:
        size += 1
        u >>= 7
    return size


@dataclass(frozen=True)
class QompressSnapshot:
    """
    Canonical reachable-state coordinates for one fixed 256x256 QNode.

    Logical coordinates:
        x_1 = x1
        x_r = global_offset + deviation[r], 2 <= r <= 256
        x_r = global_offset,                 257 <= r <= 65,536

    This is a coordinate chart for the terminal node, not a history stream.
    Integer precision remains visible: exact Python big-int growth is counted
    rather than treated as free native storage.
    """

    x1: int
    global_offset: int
    deviations: tuple[int, ...]

    def __post_init__(self) -> None:
        if len(self.deviations) != MAX_EVENT_RELATION - 1:
            raise ValueError("snapshot must contain exactly d_2..d_256")

    @property
    def is_origin(self) -> bool:
        return (
            self.x1 == 0
            and self.global_offset == 0
            and all(v == 0 for v in self.deviations)
        )

    def coordinate(self, relation: int) -> int:
        relation = int(relation)
        if not 1 <= relation <= NODE_POSITIONS:
            raise IndexError(relation)
        if relation == 1:
            return self.x1
        if relation <= MAX_EVENT_RELATION:
            return self.global_offset + self.deviations[relation - 2]
        return self.global_offset

    def materialize(self) -> tuple[int, ...]:
        head = [self.x1]
        head.extend(self.global_offset + d for d in self.deviations)
        head.extend(
            [self.global_offset] * (NODE_POSITIONS - MAX_EVENT_RELATION)
        )
        return tuple(head)

    def canonical_bytes(self) -> bytes:
        out = bytearray(_MAGIC)
        for value in (self.x1, self.global_offset, *self.deviations):
            _write_uvarint(out, _zigzag_encode(value))
        return bytes(out)

    @classmethod
    def from_canonical_bytes(cls, data: bytes) -> "QompressSnapshot":
        if not data.startswith(_MAGIC):
            raise QompressStateError("not a QNPY05 terminal QNode")
        offset = len(_MAGIC)
        values: list[int] = []
        for _ in range(_SERIALIZED_INTEGER_COUNT):
            u, offset = _read_uvarint(data, offset)
            values.append(_zigzag_decode(u))
        if offset != len(data):
            raise QompressStateError("noncanonical trailing bytes")
        return cls(values[0], values[1], tuple(values[2:]))

    @property
    def canonical_size_bytes(self) -> int:
        return len(_MAGIC) + sum(
            _signed_varint_size(v)
            for v in (self.x1, self.global_offset, *self.deviations)
        )

    @property
    def native_fixed_fit(self) -> bool:
        return self.canonical_size_bytes * 8 <= NATIVE_CAPACITY_BITS

    @property
    def host_sha256(self) -> str:
        return sha256(self.canonical_bytes()).hexdigest()


class QompressQNode:
    """
    Qompress is the environment law acting on one fixed 256x256 QNode.

    There is no encoder-side chronology structure. Each byte is exactly one
    Q-turn. Reverse execution gets the final byte and unique predecessor from
    the current node itself and repeats until Primed Zero.

    The Python reference uses arbitrary-precision integers to test the exact
    law. Precision growth is measured; it is not claimed as free fixed-node
    capacity.
    """

    width = WIDTH
    height = HEIGHT
    positions = NODE_POSITIONS
    alphabet = BYTE_ALPHABET
    quotient = QUOTIENT

    def __init__(self, snapshot: QompressSnapshot | None = None):
        if snapshot is None:
            self.x1 = 0
            self.global_offset = 0
            self.deviations = [0] * (MAX_EVENT_RELATION - 1)
        else:
            self.x1 = int(snapshot.x1)
            self.global_offset = int(snapshot.global_offset)
            self.deviations = list(snapshot.deviations)
        self._rebuild_caches()

    @classmethod
    def primed_zero(cls) -> "QompressQNode":
        return cls()

    @classmethod
    def from_bytes(cls, data: bytes) -> "QompressQNode":
        return cls(QompressSnapshot.from_canonical_bytes(data))

    @classmethod
    def from_file(cls, path: str | Path) -> "QompressQNode":
        return cls.from_bytes(Path(path).read_bytes())

    def _rebuild_caches(self) -> None:
        # Derived acceleration only. None of these values are serialized.
        self._d_coeff = 0
        self._d_v = 0
        self._nonzero_deviations = 0
        for r in range(2, MAX_EVENT_RELATION + 1):
            d = self.deviations[r - 2]
            self._d_coeff += _coeff(r) * d
            self._d_v += d
            if d:
                self._nonzero_deviations += 1

    @property
    def is_primed_zero(self) -> bool:
        return (
            self.x1 == 0
            and self.global_offset == 0
            and self._nonzero_deviations == 0
        )

    def snapshot(self) -> QompressSnapshot:
        return QompressSnapshot(
            self.x1, self.global_offset, tuple(self.deviations)
        )

    def save(self, path: str | Path) -> None:
        Path(path).write_bytes(self.snapshot().canonical_bytes())

    def coordinate(self, relation: int) -> int:
        return self.snapshot().coordinate(relation)

    @staticmethod
    def byte_relation(byte: int) -> int:
        byte = int(byte)
        if not 0 <= byte <= 255:
            raise ValueError("byte must be in [0,255]")
        return byte + 1

    @staticmethod
    def relation_byte(relation: int) -> int:
        relation = int(relation)
        if not 1 <= relation <= 256:
            raise QompressStateError(
                f"relation {relation} is not a legal byte event"
            )
        return relation - 1

    def _v_dot_current(self) -> int:
        # v^T x = x1 - g + D_v because sum_{r=2..N} v_r = -1.
        return self.x1 - self.global_offset + self._d_v

    def turn(self, byte: int) -> None:
        """Apply exactly one byte -> exactly one Q-turn."""
        r = self.byte_relation(byte)

        tail_coeff = self.global_offset * _COEFF_SUM + self._d_coeff
        b1 = QUOTIENT * self.x1 - tail_coeff

        # U = I + 2*1*v^T. Every byte relation is in the positive half.
        v_dot_b = self._v_dot_current() + (b1 - self.x1)
        v_dot_z = v_dot_b + 1
        delta = 2 * v_dot_z

        new_x1 = b1 + (1 if r == 1 else 0) + delta
        new_global = self.global_offset + delta

        if r >= 2:
            i = r - 2
            old = self.deviations[i]
            self.deviations[i] = old + 1
            self._d_coeff += _coeff(r)
            self._d_v += 1
            if old == 0:
                self._nonzero_deviations += 1

        self.x1 = new_x1
        self.global_offset = new_global

    def reverse_turn(self) -> int:
        """Recover the final byte and restore the unique predecessor."""
        if self.is_primed_zero:
            raise QompressStateError("cannot reverse Primed Zero")

        delta = 2 * self._v_dot_current()
        z1 = self.x1 - delta
        z_global = self.global_offset - delta

        residue = (
            z1 + z_global * _COEFF_SUM + self._d_coeff
        ) % QUOTIENT
        r = 256 if residue == 0 else residue
        byte = self.relation_byte(r)

        lattice1 = z1
        if r == 1:
            lattice1 -= 1
        else:
            i = r - 2
            old = self.deviations[i]
            new = old - 1
            self.deviations[i] = new
            self._d_coeff -= _coeff(r)
            self._d_v -= 1
            if old != 0 and new == 0:
                self._nonzero_deviations -= 1

        tail_coeff = z_global * _COEFF_SUM + self._d_coeff
        numerator = lattice1 + tail_coeff
        if numerator % QUOTIENT:
            raise QompressStateError(
                "state has no exact predecessor under the Qompress byte law"
            )

        self.x1 = numerator // QUOTIENT
        self.global_offset = z_global
        return byte

    def wind(self, source: bytes | bytearray | memoryview | Iterable[int]) -> None:
        for b in source:
            self.turn(int(b))

    def unwind(self, max_turns: int | None = None) -> Iterator[int]:
        count = 0
        while not self.is_primed_zero:
            if max_turns is not None and count >= max_turns:
                raise QompressStateError("reverse safety limit reached")
            yield self.reverse_turn()
            count += 1

    def decode_bytes(self, max_turns: int | None = None) -> bytes:
        reverse = bytearray(self.unwind(max_turns=max_turns))
        reverse.reverse()
        return bytes(reverse)

    def decode_to_file(
        self, path: str | Path, max_turns: int | None = None
    ) -> int:
        path = Path(path)
        tmp = path.with_suffix(path.suffix + ".qreverse.tmp")
        count = 0
        with tmp.open("wb") as f:
            for b in self.unwind(max_turns=max_turns):
                f.write(bytes((b,)))
                count += 1

        chunk = 1 << 20
        with tmp.open("rb") as src, path.open("wb") as dst:
            pos = count
            while pos:
                take = min(chunk, pos)
                pos -= take
                src.seek(pos)
                dst.write(src.read(take)[::-1])
        tmp.unlink()
        return count

    def audit(self) -> dict[str, int | bool | str]:
        snap = self.snapshot()
        return {
            "node_width": WIDTH,
            "node_height": HEIGHT,
            "node_positions": NODE_POSITIONS,
            "byte_event_classes": BYTE_ALPHABET,
            "quotient_classes": QUOTIENT,
            "native_unrestricted_byte_turn_capacity": NATIVE_CAPACITY_BITS // 8,
            "native_capacity_bits": NATIVE_CAPACITY_BITS,
            "native_capacity_bytes_ideal": NATIVE_CAPACITY_BITS // 8,
            "host_terminal_bytes": snap.canonical_size_bytes,
            "native_fixed_fit": snap.native_fixed_fit,
            "terminal_sha256": snap.host_sha256,
            "stored_turn_count": False,
            "stored_route": False,
            "stored_history": False,
            "stored_source_hash": False,
        }


def encode_file(
    source_path: str | Path, seed_path: str | Path
) -> dict[str, object]:
    source_path = Path(source_path)
    node = QompressQNode.primed_zero()
    source_hash = sha256()
    turns = 0
    with source_path.open("rb") as f:
        while True:
            block = f.read(1 << 20)
            if not block:
                break
            source_hash.update(block)
            for b in block:
                node.turn(b)
                turns += 1
    node.save(seed_path)
    out = node.audit()
    # Measurement output only; these fields are not stored in the seed.
    out.update({
        "turns_measured": turns,
        "source_bytes_measured": source_path.stat().st_size,
        "source_sha256_measured": source_hash.hexdigest(),
    })
    return out


def decode_file(
    seed_path: str | Path,
    output_path: str | Path,
    max_turns: int | None = None,
) -> dict[str, object]:
    node = QompressQNode.from_file(seed_path)
    terminal_hash = node.snapshot().host_sha256
    turns = node.decode_to_file(output_path, max_turns=max_turns)
    output_path = Path(output_path)
    output_digest = sha256()
    with output_path.open("rb") as f:
        while True:
            block = f.read(1 << 20)
            if not block:
                break
            output_digest.update(block)
    output_hash = output_digest.hexdigest()
    if not node.is_primed_zero:
        raise QompressStateError("decode did not return to Primed Zero")
    return {
        "turns_measured": turns,
        "output_bytes_measured": output_path.stat().st_size,
        "output_sha256_measured": output_hash,
        "terminal_sha256": terminal_hash,
        "returned_to_primed_zero": True,
    }
