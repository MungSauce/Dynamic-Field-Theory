from itertools import product, permutations
from qfield import QEvent, ReversibleQField


def test_roundtrip_single_event_n4():
    f = ReversibleQField(4)
    x = (7, -2, 0, 11)
    for e in f.events:
        y = f.forward(x, e)
        got, prev = f.reverse(y)
        assert got == e
        assert prev == x


def test_every_single_event_changes_whole_field():
    for n in (2, 4, 6, 8):
        f = ReversibleQField(n)
        for e in f.events:
            y = f.native_event_impact(e)
            assert all(v != 0 for v in y), (n, e, y)


def test_all_event_residues_are_distinct_nonzero():
    for n in (2, 4, 6, 8):
        f = ReversibleQField(n)
        residues = set()
        for e in f.events:
            z = f.add_event(f.apply_B(f.origin), e)
            k = f.phi(z)
            assert k != 0
            assert k not in residues
            residues.add(k)
        assert len(residues) == 2 * n
        assert residues == set(range(1, 2 * n + 1))


def test_pairwise_order_sensitive_n4():
    f = ReversibleQField(4)
    x = (3, -1, 5, 2)
    for a, b in permutations(f.events, 2):
        ab = f.forward(f.forward(x, a), b)
        ba = f.forward(f.forward(x, b), a)
        assert ab != ba, (a, b)


def test_history_endpoints_unique_n2_depth5():
    f = ReversibleQField(2)
    seen = {}
    for hist in product(f.events, repeat=5):
        terminal = f.wind(hist)
        assert terminal not in seen
        seen[terminal] = hist
    assert len(seen) == (2 * f.n) ** 5


def test_exact_unwind_n2_depth5():
    f = ReversibleQField(2)
    for hist in product(f.events, repeat=5):
        terminal = f.wind(hist)
        got_rev = []
        y = terminal
        for _ in range(len(hist)):
            e, y = f.reverse(y)
            got_rev.append(e)
        assert y == f.origin
        assert tuple(reversed(got_rev)) == hist


def test_B_inverse_on_kernel():
    for n in (2, 4, 6):
        f = ReversibleQField(n)
        samples = [
            tuple(0 for _ in range(n)),
            tuple(range(n)),
            tuple((-1)**i * (i + 3) for i in range(n)),
        ]
        for x in samples:
            assert f.apply_B_inv_kernel(f.apply_B(x)) == x


def test_U_exact_inverse():
    for n in (2, 4, 6, 8):
        f = ReversibleQField(n)
        samples = [
            tuple(0 for _ in range(n)),
            tuple(range(n)),
            tuple((-1)**i * (i + 3) for i in range(n)),
        ]
        for x in samples:
            assert f.apply_U_inv(f.apply_U(x)) == x
            assert f.apply_U(f.apply_U_inv(x)) == x


def test_signatures_unique():
    for n in (2, 4, 6, 8):
        f = ReversibleQField(n)
        sigs = {f.event_signature(e) for e in f.events}
        assert len(sigs) == 2 * n


if __name__ == "__main__":
    for name, obj in sorted(globals().copy().items()):
        if name.startswith("test_") and callable(obj):
            obj()
            print(name + "=PASS")
    print("REVERSIBLE_QFIELD_CONFORMANCE=PASS")
