from __future__ import annotations

import random

from qompress.environment import (
    NATIVE_CAPACITY_BITS,
    NODE_POSITIONS,
    QUOTIENT,
    QompressQNode,
    QompressSnapshot,
)


def test_locked_geometry():
    assert QompressQNode.width == 256
    assert QompressQNode.height == 256
    assert NODE_POSITIONS == 65_536
    assert NATIVE_CAPACITY_BITS == 131_072
    assert QUOTIENT == 256
    assert NATIVE_CAPACITY_BITS // 8 == 16_384


def test_all_256_bytes_are_exactly_one_turn():
    for b in range(256):
        q = QompressQNode()
        q.turn(b)
        assert q.reverse_turn() == b
        assert q.is_primed_zero


def test_order_changes_terminal_state():
    ab = QompressQNode()
    ba = QompressQNode()
    ab.wind(b"AB")
    ba.wind(b"BA")
    assert ab.snapshot() != ba.snapshot()


def test_cold_roundtrip_examples():
    samples = (
        b"",
        b"a",
        b"hello world",
        bytes(range(256)),
        b"abracadabra" * 100,
    )
    for source in samples:
        q = QompressQNode()
        q.wind(source)
        terminal = q.snapshot().canonical_bytes()

        cold = QompressQNode.from_bytes(terminal)
        assert cold.decode_bytes() == source
        assert cold.is_primed_zero


def test_random_roundtrips():
    rng = random.Random(0x51504D50)
    for n in (1, 2, 3, 31, 257, 1024):
        source = bytes(rng.randrange(256) for _ in range(n))
        q = QompressQNode()
        q.wind(source)
        cold = QompressQNode(q.snapshot())
        assert cold.decode_bytes() == source


def test_seed_has_no_route_length_hash_or_history():
    q = QompressQNode()
    q.wind(b"chronology is state")
    raw = q.snapshot().canonical_bytes()
    assert QompressSnapshot.from_canonical_bytes(raw) == q.snapshot()

    audit = q.audit()
    assert audit["stored_turn_count"] is False
    assert audit["stored_route"] is False
    assert audit["stored_history"] is False
    assert audit["stored_source_hash"] is False


def test_first_turn_has_whole_field_consequence():
    q = QompressQNode()
    before = q.snapshot()
    q.turn(ord("A"))
    after = q.snapshot()

    sample_relations = (1, 2, 65, 66, 256, 257, 32768, 65536)
    assert all(
        before.coordinate(r) != after.coordinate(r)
        for r in sample_relations
    )


def test_dense_reference_equivalence():
    # Independent direct-vector check of the lazy state chart.
    n = 65_536
    half = n // 2
    v = [1] * half + [-1] * half

    def coeff(r: int) -> int:
        return r if 1 <= r <= 255 else 0

    def dense_turn(x: list[int], byte: int) -> list[int]:
        r = byte + 1
        z = x.copy()
        z[0] = 256 * x[0] - sum(
            coeff(rr) * x[rr - 1] for rr in range(2, 257)
        )
        z[r - 1] += 1
        delta = 2 * sum(vv * zz for vv, zz in zip(v, z))
        return [zz + delta for zz in z]

    q = QompressQNode()
    x = [0] * n
    for byte in (0, 255, 65):
        q.turn(byte)
        x = dense_turn(x, byte)
        assert q.snapshot().materialize() == tuple(x)


def test_integer_precision_growth_is_counted():
    q = QompressQNode()
    q.wind(bytes(range(256)) * 16)
    audit = q.audit()
    assert audit["host_terminal_bytes"] > 0
    assert isinstance(audit["native_fixed_fit"], bool)


if __name__ == "__main__":
    tests = [
        value
        for name, value in sorted(globals().items())
        if name.startswith("test_") and callable(value)
    ]
    for test in tests:
        test()
        print(test.__name__ + "=PASS")
    print(f"TOTAL={len(tests)} PASS")
