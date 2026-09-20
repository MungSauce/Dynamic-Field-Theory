from pathlib import Path
import math
import tempfile
from qseed_profiler import iter_space_tokens, primitive_event_count, profile


def test_tokenizer_roundtrip():
    data = b"a  bb c\nxx "
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(data)
        p = Path(f.name)
    try:
        with p.open('rb') as fp:
            toks = list(iter_space_tokens(fp, chunk_size=3))
        assert b''.join(toks) == data
        assert toks == [b'a ', b' ', b'bb ', b'c\nxx ']
    finally:
        p.unlink()


def test_primitive_fallback_count():
    assert primitive_event_count(bytes([0, 204, 205, 255])) == 6


def test_profile_basic():
    data = b"cat dog cat dog cat"
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(data)
        p = Path(f.name)
    try:
        r = profile(p)
        assert r.source_bytes == len(data)
        assert r.token_count == 5
        assert r.unique_buttons == 3
        assert r.seed_uniform_bits > 0
        assert r.qcells_uniform == math.ceil(r.seed_uniform_bits / math.log2(19))
    finally:
        p.unlink()
