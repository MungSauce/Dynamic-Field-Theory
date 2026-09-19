#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

// trueCSS local state law.
// Two physical presence bits:
// bit 0 = negative rail present
// bit 1 = positive rail present
//
// 00 = DEAD_STOP  (neither present)
// 01 = NEG        (-1, logical binary 0)
// 10 = POS        (+1, logical binary 1)
// 11 = LIVE_ZERO  (-1 and +1 both present; arithmetic net 0)

enum class Cell : uint8_t {
    DEAD_STOP = 0b00,
    NEG       = 0b01,
    POS       = 0b10,
    LIVE_ZERO = 0b11
};

static bool neg_present(Cell s){ return (uint8_t(s) & 0b01) != 0; }
static bool pos_present(Cell s){ return (uint8_t(s) & 0b10) != 0; }
static bool is_dead(Cell s){ return s == Cell::DEAD_STOP; }
static bool is_live_zero(Cell s){ return s == Cell::LIVE_ZERO; }

// Arithmetic observation of the field.
// NEG=-1, POS=+1, LIVE_ZERO=0.
// DEAD_STOP is not arithmetic zero: execution stops before arithmetic use.
static int net(Cell s){
    if(is_dead(s)) throw std::runtime_error("DEAD_STOP has no live arithmetic value");
    return (pos_present(s)?1:0) - (neg_present(s)?1:0);
}

// Presence-composition law. It records which polarities are present.
// Repeated same-polarity contribution is idempotent.
// Opposing contributions coexist and therefore produce LIVE_ZERO.
static Cell combine(Cell a, Cell b){
    return Cell(uint8_t(a) | uint8_t(b));
}

static Cell encode_binary(uint8_t bit){
    if(bit==0) return Cell::NEG; // 0 = -1
    if(bit==1) return Cell::POS; // 1 = +1
    throw std::runtime_error("binary input must be 0 or 1");
}

static uint8_t decode_binary(Cell s){
    if(s==Cell::NEG) return 0;
    if(s==Cell::POS) return 1;
    if(s==Cell::LIVE_ZERO) throw std::runtime_error("LIVE_ZERO is balanced activity, not a single binary value");
    throw std::runtime_error("DEAD_STOP");
}

static const char* name(Cell s){
    switch(s){
        case Cell::DEAD_STOP:return "DEAD_STOP";
        case Cell::NEG:return "NEG";
        case Cell::POS:return "POS";
        case Cell::LIVE_ZERO:return "LIVE_ZERO";
    }
    return "INVALID";
}

int main(){
    Cell z=encode_binary(0);
    Cell o=encode_binary(1);
    Cell live=combine(z,o);

    std::cout<<"binary_0="<<name(z)<<" net="<<net(z)<<"\n";
    std::cout<<"binary_1="<<name(o)<<" net="<<net(o)<<"\n";
    std::cout<<"combine_0_1="<<name(live)<<" net="<<net(live)
             <<" neg_present="<<neg_present(live)
             <<" pos_present="<<pos_present(live)<<"\n";
    std::cout<<"neither="<<name(Cell::DEAD_STOP)<<" halt="<<is_dead(Cell::DEAD_STOP)<<"\n";

    if(z!=Cell::NEG) return 2;
    if(o!=Cell::POS) return 3;
    if(live!=Cell::LIVE_ZERO || net(live)!=0) return 4;
    if(!is_dead(Cell::DEAD_STOP)) return 5;
    return 0;
}
