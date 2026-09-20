import itertools
from senary_machine import (
    QState,
    SenaryMachine,
    SenarySemantic,
    pack_node,
    unpack_node,
)


def test_semantic_vocabulary_is_six():
    assert {x.value for x in SenarySemantic} == {
        "∅", "-", "-+", "+-", "+", "Π0"
    }


def test_qos_host_codes_preserved():
    assert int(QState.NEG) == 0b00
    assert int(QState.PRIMED_ZERO) == 0b01
    assert int(QState.DECAY_ZERO) == 0b10
    assert int(QState.POS) == 0b11


def test_structural_null_is_not_pi0():
    m = SenaryMachine.blank(8)
    assert m.is_structural_null
    assert not m.is_pi0
    assert m.global_semantic is SenarySemantic.STRUCTURAL_NULL


def test_pi0_is_global_not_local_digit():
    m = SenaryMachine.origin(8)
    assert not m.is_structural_null
    assert m.is_pi0
    assert m.global_semantic is SenarySemantic.PI0
    assert all(x == "-+" for x in m.semantic_tuple())


def test_sparse_pi0_uses_allocated_primitives_only():
    m = SenaryMachine.blank(8)
    m.allocate(1)
    m.allocate(6)
    assert m.is_pi0
    assert m.semantic_tuple()[0] == "∅"
    assert m.semantic_tuple()[1] == "-+"
    assert m.semantic_tuple()[6] == "-+"


def test_directed_zero_reflex():
    m = SenaryMachine.origin(2)
    m.set_active(0, QState.NEG)
    assert m.settle_to_directed_zero(0) is QState.PRIMED_ZERO
    assert m.reverse_directed_zero(0) is QState.NEG

    m.set_active(1, QState.POS)
    assert m.settle_to_directed_zero(1) is QState.DECAY_ZERO
    assert m.reverse_directed_zero(1) is QState.POS


def test_node_pack_roundtrip_exhaustive():
    states = list(QState)
    for cells in itertools.product(states, repeat=4):
        assert unpack_node(pack_node(cells)) == cells


def test_snapshot_roundtrip_exhaustive_capacity_4():
    cap = 4
    choices = [None, *list(QState)]
    seen = set()
    for formation in itertools.product(choices, repeat=cap):
        m = SenaryMachine.blank(cap)
        for i, q in enumerate(formation):
            if q is not None:
                m.allocate(i, q)

        snap = m.snapshot()
        rebuilt = SenaryMachine.from_snapshot(snap)
        assert rebuilt.semantic_tuple() == m.semantic_tuple()
        assert rebuilt.is_pi0 == m.is_pi0

        key = (snap.allocation_mask, snap.payload)
        assert key not in seen
        seen.add(key)

    assert len(seen) == 5 ** cap


def test_all_primed_zero_payload_is_integer_zero():
    for n in range(1, 16):
        m = SenaryMachine.origin(n)
        snap = m.snapshot()
        assert snap.payload == 0
        assert snap.is_pi0


if __name__ == "__main__":
    for name, obj in sorted(globals().copy().items()):
        if name.startswith("test_") and callable(obj):
            obj()
            print(name + "=PASS")
    print("SENARY_MACHINE_CONFORMANCE=PASS")
