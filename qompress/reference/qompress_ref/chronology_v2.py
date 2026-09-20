PRIMED_ZERO_NODE = 0x55

class ChronologyCounter:
    """Reversible base-256 chronology over TruQ byte-sized nodes."""
    def __init__(self, node_count: int, state: bytes | None = None):
        if node_count <= 0:
            raise ValueError("node_count must be positive")
        self.node_count = int(node_count)
        if state is None:
            self.state = bytearray([PRIMED_ZERO_NODE] * self.node_count)
        else:
            if len(state) != self.node_count:
                raise ValueError("state length mismatch")
            self.state = bytearray(state)

    @staticmethod
    def _decode_digit(v: int) -> int:
        return (int(v) - PRIMED_ZERO_NODE) & 0xFF

    @staticmethod
    def _encode_digit(d: int) -> int:
        return (int(d) + PRIMED_ZERO_NODE) & 0xFF

    def is_primed_zero(self) -> bool:
        return all(v == PRIMED_ZERO_NODE for v in self.state)

    def index(self) -> int:
        value = 0
        mul = 1
        for v in self.state:
            value += self._decode_digit(v) * mul
            mul *= 256
        return value

    def set_index(self, value: int) -> None:
        if value < 0 or value >= 256 ** self.node_count:
            raise ValueError("index out of range")
        x = int(value)
        for i in range(self.node_count):
            self.state[i] = self._encode_digit(x & 0xFF)
            x >>= 8

    def advance(self) -> None:
        for i in range(self.node_count):
            d = self._decode_digit(self.state[i])
            if d != 255:
                self.state[i] = self._encode_digit(d + 1)
                return
            self.state[i] = PRIMED_ZERO_NODE
        raise OverflowError("chronology exhausted full state cycle")

    def reverse(self) -> None:
        if self.is_primed_zero():
            raise IndexError("already at primed-zero origin")
        for i in range(self.node_count):
            d = self._decode_digit(self.state[i])
            if d != 0:
                self.state[i] = self._encode_digit(d - 1)
                return
            self.state[i] = self._encode_digit(255)

    def snapshot(self) -> bytes:
        return bytes(self.state)
