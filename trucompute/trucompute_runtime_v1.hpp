#pragma once
#include <cstdint>
#include <stdexcept>

namespace trucompute {

enum class State : uint8_t {
    DEAD_STOP = 0b00,
    NEG       = 0b01,
    POS       = 0b10,
    LIVE_ZERO = 0b11
};

constexpr bool neg_present(State s){ return (uint8_t(s) & 0b01) != 0; }
constexpr bool pos_present(State s){ return (uint8_t(s) & 0b10) != 0; }
constexpr bool dead(State s){ return s == State::DEAD_STOP; }
constexpr bool live_zero(State s){ return s == State::LIVE_ZERO; }

inline int net(State s){
    if(dead(s)) throw std::runtime_error("TruCompute DEAD_STOP");
    return (pos_present(s)?1:0) - (neg_present(s)?1:0);
}

constexpr int activity(State s){
    return (pos_present(s)?1:0) + (neg_present(s)?1:0);
}

constexpr State combine(State a, State b){
    return State(uint8_t(a) | uint8_t(b));
}

inline State encode_binary(uint8_t b){
    if(b==0) return State::NEG;
    if(b==1) return State::POS;
    throw std::runtime_error("binary input must be 0 or 1");
}

} // namespace trucompute
