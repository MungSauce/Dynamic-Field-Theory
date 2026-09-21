from qompress.node.qnode256 import QNode256, POSITIONS
from qompress.senary.directed_integer import DirectedQInteger


def test_node_dimensions():
    assert QNode256.width == 256
    assert QNode256.height == 256
    assert QNode256.positions == 65536


def test_index_roundtrip():
    for x, y in [(0,0), (255,0), (0,255), (255,255), (17,201)]:
        i = QNode256.index(x,y)
        assert QNode256.xy(i) == (x,y)


def test_origin_is_all_primed_zero():
    n = QNode256()
    assert n.is_pi0
    assert n.get(0,0) == DirectedQInteger.primed_zero()
    assert n.get(255,255) == DirectedQInteger.primed_zero()


def test_signed_counts_and_directed_zeros_are_distinct():
    n = QNode256()
    n.set(1, 1, DirectedQInteger(-37))
    n.set(2, 2, DirectedQInteger(+91))
    n.set(3, 3, DirectedQInteger.decay_zero())

    assert n.get(1,1).formation == "-"
    assert n.get(1,1).magnitude == 37
    assert n.get(2,2).formation == "+"
    assert n.get(2,2).magnitude == 91
    assert n.get(3,3).formation == "+-"
    assert n.get(3,3).value == 0
    assert n.get(0,0).formation == "-+"

    assert n.get(0,0).numerical_eq(n.get(3,3))
    assert n.get(0,0) != n.get(3,3)


def test_terminal_configuration_contains_only_native_final_state():
    n = QNode256()
    n.set_index(5, DirectedQInteger(-4))
    n.set_index(8, DirectedQInteger(+7))
    n.set_index(13, DirectedQInteger.decay_zero())

    assert n.terminal_configuration() == (
        (5, 4, "-"),
        (8, 7, "+"),
        (13, 0, "+-"),
    )

    s = n.stats()
    assert s.positions == POSITIONS
    assert s.non_origin_positions == 3
    assert s.active_nonzero_positions == 2
    assert s.total_active_magnitude == 11


def test_mechanical_zero_reflex():
    assert DirectedQInteger.primed_zero().reverse_from_zero() == DirectedQInteger(-1)
    assert DirectedQInteger.decay_zero().reverse_from_zero() == DirectedQInteger(+1)
    assert DirectedQInteger(-1).settle_one_toward_zero() == DirectedQInteger.primed_zero()
    assert DirectedQInteger(+1).settle_one_toward_zero() == DirectedQInteger.decay_zero()


if __name__ == "__main__":
    for name, obj in sorted(globals().copy().items()):
        if name.startswith("test_") and callable(obj):
            obj()
            print(name + "=PASS")
    print("QNODE256_CONFORMANCE=PASS")
