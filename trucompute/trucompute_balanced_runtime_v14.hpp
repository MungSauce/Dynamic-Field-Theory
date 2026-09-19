#pragma once

#include <cstdint>
#include <cstddef>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace trucompute::balanced_v14 {

// Two rails are always latent in every cell.
// bit 0 = NEG rail present
// bit 1 = POS rail present
//
// 00 = NEITHER/OFF: both rails suppressed in this observation frame
// 01 = NEG: POS rail suppressed
// 10 = POS: NEG rail suppressed
// 11 = NATURAL_ZERO: both rails present, net zero
//
// NATURAL_ZERO is the only resting/base state of a cell.
enum class State : std::uint8_t {
    NEITHER      = 0b00,
    NEG          = 0b01,
    POS          = 0b10,
    NATURAL_ZERO = 0b11
};

constexpr std::uint8_t NEG_RAIL = 0b01;
constexpr std::uint8_t POS_RAIL = 0b10;
constexpr std::uint8_t BOTH_RAILS = NEG_RAIL | POS_RAIL;

constexpr bool neg_present(State s) {
    return (static_cast<std::uint8_t>(s) & NEG_RAIL) != 0;
}

constexpr bool pos_present(State s) {
    return (static_cast<std::uint8_t>(s) & POS_RAIL) != 0;
}

constexpr bool balanced(State s) {
    return s == State::NATURAL_ZERO;
}

constexpr bool resolved(State s) {
    return s == State::NEG || s == State::POS;
}

constexpr bool neither(State s) {
    return s == State::NEITHER;
}

inline int net(State s) {
    if (neither(s)) {
        throw std::runtime_error("NEITHER has no participating numeric net");
    }
    return (pos_present(s) ? 1 : 0) - (neg_present(s) ? 1 : 0);
}

// A structural cell has no mutable scalar polarity. Its substrate is always
// BOTH_RAILS. Computed states are produced only by an observation frame that
// suppresses zero, one, or both rails.
struct Cell {
    static constexpr State natural_state() {
        return State::NATURAL_ZERO;
    }

    static constexpr std::uint8_t latent_rails() {
        return BOTH_RAILS;
    }
};

// A frame is a temporary view over an immutable balanced substrate.
// suppressed_[i] is a two-bit rail-removal mask; it is not cell storage.
class ObservationFrame {
public:
    explicit ObservationFrame(std::size_t cell_count)
        : suppressed_(cell_count, 0) {}

    std::size_t size() const { return suppressed_.size(); }

    State state(std::size_t i) const {
        check(i);
        const std::uint8_t visible =
            BOTH_RAILS & static_cast<std::uint8_t>(~suppressed_[i]);
        return static_cast<State>(visible & BOTH_RAILS);
    }

    // Resolve only by removing rails from the natural (+1,-1) pair.
    void collapse_to(std::size_t i, State desired) {
        check(i);
        switch (desired) {
            case State::NATURAL_ZERO: suppressed_[i] = 0; break;
            case State::POS:          suppressed_[i] = NEG_RAIL; break;
            case State::NEG:          suppressed_[i] = POS_RAIL; break;
            case State::NEITHER:      suppressed_[i] = BOTH_RAILS; break;
        }
    }

    void suppress_negative(std::size_t i) {
        check(i);
        suppressed_[i] |= NEG_RAIL;
    }

    void suppress_positive(std::size_t i) {
        check(i);
        suppressed_[i] |= POS_RAIL;
    }

    void release(std::size_t i) {
        check(i);
        suppressed_[i] = 0;
    }

    void release_all() {
        for (auto &m : suppressed_) m = 0;
    }

    std::uint8_t suppression_mask(std::size_t i) const {
        check(i);
        return suppressed_[i] & BOTH_RAILS;
    }

private:
    std::vector<std::uint8_t> suppressed_;

    void check(std::size_t i) const {
        if (i >= suppressed_.size())
            throw std::out_of_range("TruCompute cell index");
    }
};

enum class Relation : std::uint8_t {
    SAME,
    OPPOSITE
};

struct Projection {
    ObservationFrame frame;
    bool consistent = true;
    std::vector<std::size_t> conflict_nodes;

    explicit Projection(std::size_t n) : frame(n) {}
};

// BalancedEnvironment is the executable semantic environment:
//   persistent substrate: every cell is (+1,-1)
//   persistent information: relations between cells
//   computation: hold one resolved reference and let relations determine
//                which rail is suppressed in reachable cells
//   release: discard the frame; every cell is again naturally (+1,-1)
class BalancedEnvironment {
public:
    explicit BalancedEnvironment(std::size_t cell_count)
        : cells_(cell_count), adjacency_(cell_count) {}

    std::size_t size() const { return cells_.size(); }

    State natural_state(std::size_t i) const {
        check(i);
        return cells_[i].natural_state();
    }

    void connect(std::size_t a, std::size_t b, Relation relation) {
        check(a);
        check(b);
        if (a == b && relation == Relation::OPPOSITE) {
            throw std::invalid_argument("a cell cannot be opposite to itself");
        }
        adjacency_[a].push_back({b, relation});
        if (a != b) adjacency_[b].push_back({a, relation});
    }

    // Hold one node as + or -. The held state is itself a temporary collapse;
    // relations then propagate that reference through the same substrate.
    // Unreachable nodes remain NATURAL_ZERO. Contradictions are reported.
    Projection observe_from(std::size_t reference, State held_state) const {
        check(reference);
        if (!resolved(held_state)) {
            throw std::invalid_argument("reference must be held as POS or NEG");
        }

        Projection out(size());
        std::vector<int> assignment(size(), 0); // -1 NEG, 0 unresolved, +1 POS
        std::queue<std::size_t> q;

        assignment[reference] = (held_state == State::POS) ? +1 : -1;
        q.push(reference);

        while (!q.empty()) {
            const auto u = q.front();
            q.pop();

            for (const auto &arc : adjacency_[u]) {
                const int expected = (arc.relation == Relation::SAME)
                    ? assignment[u]
                    : -assignment[u];

                if (assignment[arc.other] == 0) {
                    assignment[arc.other] = expected;
                    q.push(arc.other);
                } else if (assignment[arc.other] != expected) {
                    out.consistent = false;
                    out.conflict_nodes.push_back(arc.other);
                }
            }
        }

        for (std::size_t i = 0; i < size(); ++i) {
            if (assignment[i] > 0) out.frame.collapse_to(i, State::POS);
            else if (assignment[i] < 0) out.frame.collapse_to(i, State::NEG);
            // assignment == 0 deliberately remains NATURAL_ZERO
        }

        return out;
    }

private:
    struct Arc {
        std::size_t other;
        Relation relation;
    };

    std::vector<Cell> cells_;
    std::vector<std::vector<Arc>> adjacency_;

    void check(std::size_t i) const {
        if (i >= cells_.size())
            throw std::out_of_range("TruCompute cell index");
    }
};

inline const char* state_name(State s) {
    switch (s) {
        case State::NEITHER:      return "NEITHER";
        case State::NEG:          return "NEG";
        case State::POS:          return "POS";
        case State::NATURAL_ZERO: return "NATURAL_ZERO";
    }
    return "INVALID";
}

} // namespace trucompute::balanced_v14
