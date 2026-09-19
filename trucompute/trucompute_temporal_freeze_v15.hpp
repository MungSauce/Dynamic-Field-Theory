#pragma once

#include <cstddef>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <vector>

namespace trucompute::temporal_v15 {

enum class State : std::uint8_t {
    FROZEN_ZERO = 0b00,
    NEG         = 0b01,
    POS         = 0b10,
    ACTIVE_ZERO = 0b11
};

constexpr std::uint8_t NEG_RAIL = 0b01;
constexpr std::uint8_t POS_RAIL = 0b10;
constexpr std::uint8_t BOTH_RAILS = NEG_RAIL | POS_RAIL;

constexpr bool is_active(State s) { return s != State::FROZEN_ZERO; }
constexpr bool is_resolved(State s) { return s == State::NEG || s == State::POS; }
constexpr bool is_balanced(State s) { return s == State::ACTIVE_ZERO; }

inline int active_net(State s) {
    switch (s) {
        case State::NEG: return -1;
        case State::POS: return +1;
        case State::ACTIVE_ZERO: return 0;
        case State::FROZEN_ZERO:
            throw std::runtime_error("FROZEN_ZERO has no active numeric net");
    }
    throw std::runtime_error("invalid TruCompute state");
}

// Physical substrate is conceptually a balanced dual rail.
// suppression_ records the conditional rail inhibition while powered.
// powered_=false freezes that condition in time and isolates the cell.
// 00 therefore means STOP EVOLVING, not EMPTY and not numeric zero.
class Cell {
public:
    Cell() = default;

    State state() const {
        if (!powered_) return State::FROZEN_ZERO;
        switch (suppression_ & BOTH_RAILS) {
            case 0:        return State::ACTIVE_ZERO;
            case NEG_RAIL: return State::POS;
            case POS_RAIL: return State::NEG;
            default:
                throw std::logic_error("both rails cannot be suppressed while powered");
        }
    }

    // Diagnostic retention view: what condition reappears when power returns.
    // A frozen cell does not actively drive the field.
    State retained_active_state() const {
        switch (suppression_ & BOTH_RAILS) {
            case 0:        return State::ACTIVE_ZERO;
            case NEG_RAIL: return State::POS;
            case POS_RAIL: return State::NEG;
            default:
                throw std::logic_error("invalid retained suppression mask");
        }
    }

    bool powered() const { return powered_; }

    void resolve(State desired) {
        require_powered();
        switch (desired) {
            case State::ACTIVE_ZERO: suppression_ = 0; break;
            case State::POS:         suppression_ = NEG_RAIL; break;
            case State::NEG:         suppression_ = POS_RAIL; break;
            case State::FROZEN_ZERO:
                throw std::invalid_argument("use freeze() for temporal 00");
        }
    }

    void freeze() {
        powered_ = false;
    }

    void thaw() {
        powered_ = true;
    }

    void rebalance() {
        require_powered();
        suppression_ = 0;
    }

private:
    std::uint8_t suppression_ = 0;
    bool powered_ = true;

    void require_powered() const {
        if (!powered_)
            throw std::runtime_error("frozen TruCompute cell cannot evolve");
    }
};

enum class Relation : std::uint8_t { SAME, OPPOSITE };

struct Arc {
    std::size_t other;
    Relation relation;
};

class Frame {
public:
    explicit Frame(std::size_t n) : cells_(n) {}

    std::size_t size() const { return cells_.size(); }
    State state(std::size_t i) const { return at(i).state(); }
    State retained_active_state(std::size_t i) const { return at(i).retained_active_state(); }
    bool powered(std::size_t i) const { return at(i).powered(); }

    void resolve(std::size_t i, State s) { at(i).resolve(s); }
    void freeze(std::size_t i) { at(i).freeze(); }
    void thaw(std::size_t i) { at(i).thaw(); }
    void rebalance(std::size_t i) { at(i).rebalance(); }

    void rebalance_all_powered() {
        for (auto& c : cells_)
            if (c.powered()) c.rebalance();
    }

private:
    std::vector<Cell> cells_;

    Cell& at(std::size_t i) {
        if (i >= cells_.size()) throw std::out_of_range("TruCompute cell index");
        return cells_[i];
    }

    const Cell& at(std::size_t i) const {
        if (i >= cells_.size()) throw std::out_of_range("TruCompute cell index");
        return cells_[i];
    }
};

struct PropagationResult {
    bool consistent = true;
    std::vector<std::size_t> conflicts;
};

class Environment {
public:
    explicit Environment(std::size_t n) : adjacency_(n) {}

    std::size_t size() const { return adjacency_.size(); }
    Frame new_frame() const { return Frame(size()); }

    void connect(std::size_t a, std::size_t b, Relation r) {
        check(a); check(b);
        if (a == b && r == Relation::OPPOSITE)
            throw std::invalid_argument("self-opposite relation is contradictory");
        adjacency_[a].push_back({b, r});
        if (a != b) adjacency_[b].push_back({a, r});
    }

    // A powered reference conditions connected powered cells.
    // FROZEN_ZERO cells are temporal insulators: propagation cannot traverse
    // them and their retained condition cannot change until power returns.
    PropagationResult propagate(Frame& frame, std::size_t reference, State held) const {
        check(reference);
        if (frame.size() != size())
            throw std::invalid_argument("frame/environment size mismatch");
        if (!frame.powered(reference))
            throw std::invalid_argument("frozen reference cannot drive field");
        if (!is_resolved(held))
            throw std::invalid_argument("reference must be POS or NEG");

        PropagationResult result;
        std::vector<int> assignment(size(), 0);
        std::queue<std::size_t> q;

        frame.resolve(reference, held);
        assignment[reference] = held == State::POS ? +1 : -1;
        q.push(reference);

        while (!q.empty()) {
            const auto u = q.front();
            q.pop();
            if (!frame.powered(u)) continue;

            for (const auto& arc : adjacency_[u]) {
                const auto v = arc.other;
                if (!frame.powered(v)) continue;

                const int expected = arc.relation == Relation::SAME
                    ? assignment[u] : -assignment[u];

                const State current = frame.state(v);
                if (assignment[v] == 0) {
                    if (is_resolved(current)) {
                        const int existing = current == State::POS ? +1 : -1;
                        if (existing != expected) {
                            result.consistent = false;
                            result.conflicts.push_back(v);
                            continue;
                        }
                    } else {
                        frame.resolve(v, expected > 0 ? State::POS : State::NEG);
                    }
                    assignment[v] = expected;
                    q.push(v);
                } else if (assignment[v] != expected) {
                    result.consistent = false;
                    result.conflicts.push_back(v);
                }
            }
        }
        return result;
    }

private:
    std::vector<std::vector<Arc>> adjacency_;

    void check(std::size_t i) const {
        if (i >= adjacency_.size())
            throw std::out_of_range("TruCompute cell index");
    }
};

inline const char* state_name(State s) {
    switch (s) {
        case State::FROZEN_ZERO: return "FROZEN_ZERO";
        case State::NEG: return "NEG";
        case State::POS: return "POS";
        case State::ACTIVE_ZERO: return "ACTIVE_ZERO";
    }
    return "INVALID";
}

} // namespace trucompute::temporal_v15
