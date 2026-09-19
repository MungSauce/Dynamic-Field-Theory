#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <string>

namespace trucompute::contextual_v17 {

enum class Choice : int { NEG = -1, POS = +1 };

struct NodeState {
    std::int64_t visible = 0;
    std::int64_t zero_context = 0;
    std::uint32_t depth = 0;
    bool terminal = false;

    void choose(Choice c) {
        if (terminal) throw std::runtime_error("terminal node cannot evolve");
        if (depth >= 62) throw std::overflow_error("context depth exceeds int64 test model");
        const std::int64_t s = static_cast<int>(c);
        visible += s;
        zero_context = 2 * zero_context + s;
        ++depth;
    }

    void enter() { terminal = true; }
};

std::int64_t offset(std::uint32_t depth) {
    if (depth >= 63) throw std::overflow_error("depth too large");
    return (std::int64_t{1} << depth) - 1;
}

std::int64_t context_from_index(std::uint64_t k, std::uint32_t depth) {
    const std::uint64_t count = std::uint64_t{1} << depth;
    if (k >= count) throw std::out_of_range("index outside contextual depth");
    return static_cast<std::int64_t>(2 * k) - offset(depth);
}

std::uint64_t index_from_context(std::int64_t z, std::uint32_t depth) {
    const std::int64_t off = offset(depth);
    const std::int64_t numerator = z + off;
    if (numerator < 0 || (numerator & 1))
        throw std::invalid_argument("invalid contextual zero");
    const auto k = static_cast<std::uint64_t>(numerator / 2);
    if (k >= (std::uint64_t{1} << depth))
        throw std::out_of_range("context outside depth");
    return k;
}

std::string path_for_index(std::uint64_t k, std::uint32_t depth) {
    std::string out;
    for (std::uint32_t i = 0; i < depth; ++i) {
        const std::uint32_t shift = depth - 1 - i;
        const bool bit = ((k >> shift) & 1ULL) != 0;
        if (!out.empty()) out += ' ';
        out += bit ? "+1" : "-1";
    }
    return out;
}

} // namespace trucompute::contextual_v17

using namespace trucompute::contextual_v17;

static void require(bool ok, const char* msg) {
    if (!ok) { std::cerr << "FAIL: " << msg << '\n'; std::exit(1); }
}

int main() {
    NodeState a, b;
    a.choose(Choice::POS); a.choose(Choice::POS); a.choose(Choice::NEG);
    b.choose(Choice::NEG); b.choose(Choice::POS); b.choose(Choice::POS);

    require(a.visible == 1 && b.visible == 1, "ordinary sums should collide at +1");
    require(a.zero_context != b.zero_context, "context must preserve order");
    require(a.zero_context == 5, "unexpected ++- context");
    require(b.zero_context == -1, "unexpected -++ context");

    for (std::uint32_t depth = 1; depth <= 20; ++depth) {
        const std::uint64_t n = std::uint64_t{1} << depth;
        std::unordered_set<std::int64_t> seen;
        seen.reserve(static_cast<std::size_t>(n * 1.3));
        for (std::uint64_t k = 0; k < n; ++k) {
            const auto z = context_from_index(k, depth);
            require(index_from_context(z, depth) == k, "context inverse failed");
            require(seen.insert(z).second, "context collision detected");
        }
        require(seen.size() == n, "unique state count mismatch");
    }

    constexpr std::uint32_t depth = 30;
    constexpr std::uint64_t position_one_based = 1'000'000'000ULL;
    constexpr std::uint64_t k = position_one_based - 1;

    const auto z = context_from_index(k, depth);
    const auto recovered = index_from_context(z, depth) + 1;
    require(recovered == position_one_based, "billionth position failed round-trip");

    NodeState billion;
    for (std::uint32_t i = 0; i < depth; ++i) {
        const auto shift = depth - 1 - i;
        billion.choose(((k >> shift) & 1ULL) ? Choice::POS : Choice::NEG);
    }

    require(billion.zero_context == z, "literal +/- path did not reach contextual coordinate");
    billion.enter();
    require(billion.terminal, "ENTER did not terminalize state");

    std::cout << "TRUCOMPUTE_CONTEXTUAL_ZERO_V17=PASS\n";
    std::cout << "same_sum_different_history: ++- => visible=" << a.visible
              << " context=" << a.zero_context << "; -++ => visible=" << b.visible
              << " context=" << b.zero_context << "\n";
    std::cout << "exhaustive_unique_through_depth=20 PASS\n";
    std::cout << "depth30_capacity=" << (std::uint64_t{1} << depth) << "\n";
    std::cout << "position=" << position_one_based << " context=" << z
              << " recovered_position=" << recovered << "\n";
    std::cout << "visible_sum_at_billionth_path=" << billion.visible << "\n";
    std::cout << "path=" << path_for_index(k, depth) << "\n";
    std::cout << "ENTER=00 terminal PASS\n";
}
