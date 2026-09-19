#pragma once
#include <cstdint>
#include <initializer_list>
#include <stdexcept>

namespace trucompute {

enum class State : uint8_t {
    DEAD_STOP = 0b00, // structurally present, not addressed / no drive
    NEG       = 0b01,
    POS       = 0b10,
    LIVE_ZERO = 0b11  // addressed, active contributions sum to zero: stateless
};

constexpr bool neg_present(State s){ return (uint8_t(s) & 0b01) != 0; }
constexpr bool pos_present(State s){ return (uint8_t(s) & 0b10) != 0; }
constexpr bool dead(State s){ return s == State::DEAD_STOP; }
constexpr bool stateless(State s){ return s == State::LIVE_ZERO; }

inline int net(State s){
    if(dead(s)) return 0;
    return (pos_present(s)?1:0) - (neg_present(s)?1:0);
}

constexpr int activity(State s){
    return (pos_present(s)?1:0) + (neg_present(s)?1:0);
}

constexpr State combine(State a, State b){
    return State(uint8_t(a) | uint8_t(b));
}

constexpr State polarity(bool positive){
    return positive ? State::POS : State::NEG;
}

// Runtime settlement law for a structurally-live circuit.
// No addressed contribution => DEAD_STOP.
// Addressed contributions summing to zero => LIVE_ZERO / STATELESS.
// Negative/positive sums resolve to NEG/POS.
inline State settle_sum(int sum, bool addressed=true){
    if(!addressed) return State::DEAD_STOP;
    if(sum < 0) return State::NEG;
    if(sum > 0) return State::POS;
    return State::LIVE_ZERO;
}

inline State settle(std::initializer_list<int> contributions){
    if(contributions.size()==0) return State::DEAD_STOP;
    int sum=0;
    for(int x: contributions) sum += x;
    return settle_sum(sum, true);
}

struct Element {
    bool retained_positive = false; // one retained tuning bit

    constexpr bool structurally_live() const { return true; }

    // Activation does not create the element. It gates its already-retained
    // orientation onto the active circuit. Unaddressed signal is zero/dead.
    constexpr State observe(bool activated, bool phase=false) const {
        if(!activated) return State::DEAD_STOP;
        const bool expressed_positive = retained_positive ^ phase;
        return expressed_positive ? State::POS : State::NEG;
    }
};

} // namespace trucompute
