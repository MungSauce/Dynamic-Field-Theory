from itertools import product

from qompress.field.qfield import ReversibleQField
from qompress.native.native_register import NativeFieldState, integer_to_qstates, qstates_to_integer, zigzag_encode, zigzag_decode


def test_integer_roundtrip():
    for x in range(-10000, 10001):
        assert zigzag_decode(zigzag_encode(x)) == x
        assert qstates_to_integer(integer_to_qstates(x)) == x


def test_history_survives_native_bridge():
    f = ReversibleQField(2)
    for hist in product(f.events, repeat=5):
        terminal = f.wind(hist)
        native = NativeFieldState.from_coordinates(terminal)
        assert native.coordinates == terminal
        y = native.coordinates
        got = []
        for _ in hist:
            event, y = f.reverse(y)
            got.append(event)
        assert y == f.origin
        assert tuple(reversed(got)) == hist
