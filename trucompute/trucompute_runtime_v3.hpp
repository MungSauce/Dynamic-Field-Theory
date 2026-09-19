#pragma once
#include <cstdint>
#include <initializer_list>
#include <stdexcept>

namespace trucompute {

// Canonical four-condition split-state law.
//
// 00 = NEITHER: no contribution / not addressed. This is not numeric zero.
// 01 = NEG: negative resolved contribution.
// 10 = POS: positive resolved contribution.
// 11 = STATELESS_ZERO: addressed and active, but net sum is exactly zero.
//
// The crucial invariant is:
//     STATELESS_ZERO != NEITHER
//
// Both may appear "quiet" externally, but only STATELESS_ZERO is a real
// computed zero. NEITHER means there was no participating state to resolve.
enum class State : uint8_t {
    NEITHER        = 0b00,
    NEG            = 0b01,
    POS            = 0b10,
    STATELESS_ZERO = 0b11
};

constexpr bool neg_present(State s){ return (uint8_t(s) & 0b01) != 0; }
constexpr bool pos_present(State s){ return (uint8_t(s) & 0b10) != 0; }

constexpr bool neither(State s){
    return s == State::NEITHER;
}

constexpr bool stateless(State s){
    return s == State::STATELESS_ZERO;
}

constexpr bool resolved(State s){
    return s == State::NEG || s == State::POS;
}

constexpr int activity(State s){
    return (pos_present(s) ? 1 : 0) + (neg_present(s) ? 1 : 0);
}

// Numeric net is defined only for a participating state.
// NEITHER has no numeric result and must never be silently treated as zero.
inline int net(State s){
    if (neither(s))
        throw std::runtime_error("TruCompute NEITHER has no numeric net");
    return (pos_present(s) ? 1 : 0) - (neg_present(s) ? 1 : 0);
}

// Presence composition. Opposing present polarities produce STATELESS_ZERO.
constexpr State combine(State a, State b){
    return State(uint8_t(a) | uint8_t(b));
}

constexpr State polarity(bool positive){
    return positive ? State::POS : State::NEG;
}

// Runtime settlement law:
// - no participating contributions => NEITHER
// - participating contributions whose sum is zero => STATELESS_ZERO
// - negative sum => NEG
// - positive sum => POS
inline State settle_sum(int sum, bool participated){
    if (!participated) return State::NEITHER;
    if (sum < 0) return State::NEG;
    if (sum > 0) return State::POS;
    return State::STATELESS_ZERO;
}

inline State settle(std::initializer_list<int> contributions){
    if (contributions.size() == 0) return State::NEITHER;
    int sum = 0;
    for (int x : contributions) sum += x;
    return settle_sum(sum, true);
}

// Every circuit element is structurally present at all times.
// Its retained orientation is one bit.
// If it is not addressed, its observable condition is NEITHER.
// Activation exposes its already-retained polarity; activation never creates it.
struct Element {
    bool retained_positive = false;

    constexpr bool structurally_live() const { return true; }

    constexpr State observe(bool activated, bool phase=false) const {
        if (!activated) return State::NEITHER;
        const bool expressed_positive = retained_positive ^ phase;
        return expressed_positive ? State::POS : State::NEG;
    }
};

} // namespace trucompute
