from qomputation.qnode import (
    ACTIVE_BYTES, ALLOC_BYTES, HEIGHT, POSITIONS, WIDTH,
    QNode256, relation_index, relation_xy,
)


def test_dimensions():
    assert WIDTH == 256
    assert HEIGHT == 256
    assert POSITIONS == 65536
    assert ACTIVE_BYTES == 16384
    assert ALLOC_BYTES == 8192


def test_address_bijection():
    for i in (0, 1, 255, 256, 257, 65535):
        x, y = relation_xy(i)
        assert relation_index(x, y) == i


def test_void_and_origin_are_distinct():
    v = QNode256.void()
    o = QNode256.origin()
    assert v.is_void
    assert not v.is_pi0
    assert not o.is_void
    assert o.is_pi0
    assert o.allocated_count == POSITIONS


def test_sparse_position_roundtrip():
    n = QNode256.void()
    n.allocate(17, 93, 0b11)
    assert n.is_allocated(17, 93)
    assert n.get_code(17, 93) == 0b11
    n.set_code(17, 93, 0b10)
    assert n.get_code(17, 93) == 0b10
    n.deallocate(17, 93)
    assert not n.is_allocated(17, 93)


def test_host_reference_footprint():
    n = QNode256.void()
    assert n.host_storage_bytes() == 24576
