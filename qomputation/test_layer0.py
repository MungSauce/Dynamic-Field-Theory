from qomputation.qinteger import decode_integer, encode_integer, zigzag_decode, zigzag_encode
from qomputation.qreflex import reverse_settle, settle
from qomputation.qstate import QFormation, QSemantic


def test_six_semantic_conditions():
    assert {x.value for x in QSemantic} == {"Ø", "-", "-+", "+-", "+", "Π0"}


def test_host_codes():
    assert int(QFormation.NEG) == 0b00
    assert int(QFormation.PRIMED_ZERO) == 0b01
    assert int(QFormation.DECAY_ZERO) == 0b10
    assert int(QFormation.POS) == 0b11


def test_reflex_inverse():
    assert reverse_settle(settle(QFormation.NEG)) is QFormation.NEG
    assert reverse_settle(settle(QFormation.POS)) is QFormation.POS


def test_zigzag_integer_bijection():
    for value in range(-10000, 10001):
        assert zigzag_decode(zigzag_encode(value)) == value
        assert decode_integer(encode_integer(value)) == value
