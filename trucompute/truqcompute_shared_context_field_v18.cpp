#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace trucompute::shared_v18 {

constexpr std::uint32_t DEPTH = 30;
constexpr std::uint64_t FIELD_NODES = 1'000'000ULL;
constexpr std::uint64_t GLOBAL_POSITION_ONE_BASED = 1'000'000'000ULL;
constexpr std::uint64_t GLOBAL_CODE = GLOBAL_POSITION_ONE_BASED - 1ULL;

struct NodeState {
    std::int32_t visible = 0;
    std::int64_t zero_context = 0;
};

inline int sign_from_bit(bool bit) {
    return bit ? +1 : -1;
}

inline bool bit_from_sign(int sign) {
    if (sign != -1 && sign != +1)
        throw std::invalid_argument("sign must be +/-1");
    return sign > 0;
}

// Fixed structural relation of physical node i to contextual layer t.
// No per-history table is stored: relation is derived from physical identity.
inline int relation_sign(std::uint64_t node, std::uint32_t t) {
    const std::uint32_t shift = DEPTH - 1U - t;
    return sign_from_bit(((node >> shift) & 1ULL) != 0);
}

// Shared global context signal for layer t.
inline int global_sign(std::uint32_t t) {
    const std::uint32_t shift = DEPTH - 1U - t;
    return sign_from_bit(((GLOBAL_CODE >> shift) & 1ULL) != 0);
}

// Every active node receives exactly one literal +1 or -1 contribution.
// Its visible displacement is ordinary arithmetic.
// Its old zero is recursively folded into the new contextual zero.
inline void step(NodeState& node, int local_sign) {
    node.visible += local_sign;
    node.zero_context = 2 * node.zero_context + local_sign;
}

inline NodeState evolve_node(std::uint64_t node_index) {
    if (node_index >= (1ULL << DEPTH))
        throw std::out_of_range("node identity exceeds depth capacity");

    NodeState state;
    for (std::uint32_t t = 0; t < DEPTH; ++t) {
        const int local = global_sign(t) * relation_sign(node_index, t);
        step(state, local);
    }
    return state;
}

inline std::int64_t offset() {
    return (std::int64_t{1} << DEPTH) - 1;
}

// Recover physical node identity from contextual zero while the shared
// global context is known. This is the inverse of the field evolution.
inline std::uint64_t recover_node(std::int64_t z) {
    const std::int64_t numerator = z + offset();
    if (numerator < 0 || (numerator & 1))
        throw std::invalid_argument("invalid contextual zero");

    std::uint64_t local_code = static_cast<std::uint64_t>(numerator / 2);
    if (local_code >= (1ULL << DEPTH))
        throw std::out_of_range("context outside field depth");

    std::uint64_t node = 0;
    for (std::uint32_t t = 0; t < DEPTH; ++t) {
        const std::uint32_t shift = DEPTH - 1U - t;
        const int local = sign_from_bit(((local_code >> shift) & 1ULL) != 0);
        const int rel = local * global_sign(t);
        if (bit_from_sign(rel))
            node |= (1ULL << shift);
    }
    return node;
}

} // namespace trucompute::shared_v18

using namespace trucompute::shared_v18;

static void require(bool ok, const char* msg) {
    if (!ok) {
        std::cerr << "FAIL: " << msg << '\n';
        std::exit(1);
    }
}

int main() {
    std::vector<std::int64_t> contexts;
    contexts.reserve(FIELD_NODES);

    std::unordered_map<std::int32_t, std::pair<std::uint64_t, std::int64_t>> first_visible;
    bool found_visible_collision = false;
    std::uint64_t collision_a = 0, collision_b = 0;
    std::int32_t collision_visible = 0;
    std::int64_t collision_za = 0, collision_zb = 0;

    std::int32_t min_visible = std::numeric_limits<std::int32_t>::max();
    std::int32_t max_visible = std::numeric_limits<std::int32_t>::min();

    for (std::uint64_t i = 0; i < FIELD_NODES; ++i) {
        const NodeState s = evolve_node(i);
        contexts.push_back(s.zero_context);

        min_visible = std::min(min_visible, s.visible);
        max_visible = std::max(max_visible, s.visible);

        require(recover_node(s.zero_context) == i,
                "context failed to recover physical node identity");

        if (!found_visible_collision) {
            auto [it, inserted] = first_visible.emplace(
                s.visible, std::make_pair(i, s.zero_context));
            if (!inserted && it->second.second != s.zero_context) {
                found_visible_collision = true;
                collision_a = it->second.first;
                collision_b = i;
                collision_visible = s.visible;
                collision_za = it->second.second;
                collision_zb = s.zero_context;
            }
        }
    }

    std::sort(contexts.begin(), contexts.end());
    require(std::adjacent_find(contexts.begin(), contexts.end()) == contexts.end(),
            "context collision detected in million-node field");
    require(found_visible_collision,
            "expected ordinary visible values to collide");

    // Analytical capacity: relation bit-vector is node identity, local sign is
    // XNOR(global bit, relation bit). XNOR by a fixed global vector is a
    // permutation over all 2^DEPTH bit-vectors, so all full-depth contextual
    // zeros are unique across the entire addressable field.
    const std::uint64_t full_capacity = 1ULL << DEPTH;

    // Boundary and billion-scale recovery checks.
    require(recover_node(evolve_node(0).zero_context) == 0,
            "node 0 inverse failed");
    require(recover_node(evolve_node(FIELD_NODES - 1).zero_context) == FIELD_NODES - 1,
            "millionth physical node inverse failed");
    require(recover_node(evolve_node(GLOBAL_CODE).zero_context) == GLOBAL_CODE,
            "billionth-context node inverse failed");

    std::cout << "TRUCOMPUTE_SHARED_CONTEXT_FIELD_V18=PASS\n";
    std::cout << "field_nodes_tested=" << FIELD_NODES << "\n";
    std::cout << "context_depth=" << DEPTH << "\n";
    std::cout << "analytical_capacity=" << full_capacity << "\n";
    std::cout << "million_node_context_collisions=0\n";
    std::cout << "node_inverse_roundtrip=PASS\n";
    std::cout << "visible_range=[" << min_visible << "," << max_visible << "]\n";
    std::cout << "ordinary_visible_collision: node=" << collision_a
              << " and node=" << collision_b
              << " both visible=" << collision_visible
              << " but contexts=" << collision_za
              << "," << collision_zb << "\n";
    std::cout << "shared_global_position=" << GLOBAL_POSITION_ONE_BASED << "\n";
    std::cout << "relation_storage=DERIVED_FROM_FIXED_PHYSICAL_IDENTITY\n";
    std::cout << "history_list_storage=NONE\n";
    return 0;
}
