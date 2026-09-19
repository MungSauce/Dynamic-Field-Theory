#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace trucompute_stream_v11 {

constexpr uint16_t PRIME=65521;
constexpr uint16_t MAX_PAGES=1000;
constexpr uint16_t CHARACTER_COUNT=206;
constexpr uint16_t R_LOW=0;
constexpr uint16_t R_FLIP=1000;
constexpr uint16_t R_THRESHOLD=500;

enum class Unresolved:uint8_t { BOTH };
enum class Resolved:int8_t { NEG=-1, POS=+1 };
enum class SwitchState:uint8_t { NEITHER, ON };

inline uint16_t mod_mul(uint32_t a,uint32_t b){
    return uint16_t((uint64_t(a)*uint64_t(b))%PRIME);
}
inline uint16_t mod_pow(uint16_t a,uint32_t e){
    uint32_t base=a,out=1;
    while(e){
        if(e&1u) out=mod_mul(out,base);
        base=mod_mul(base,base);
        e>>=1u;
    }
    return uint16_t(out);
}
inline uint16_t mod_inv(uint16_t a){
    if(!a) throw std::runtime_error("zero inverse");
    return mod_pow(a,PRIME-2);
}
inline uint16_t mod_sub(uint16_t a,uint16_t b){
    return uint16_t((uint32_t(a)+PRIME-b)%PRIME);
}

struct Probe{
    uint16_t page=0;
    uint16_t character=0;
};

class TerminalMap {
    std::array<int16_t,256> forward_{};
    std::array<uint8_t,CHARACTER_COUNT> reverse_{};
    uint16_t count_=0;
public:
    TerminalMap(){ forward_.fill(-1); }

    uint16_t encode(uint8_t raw){
        int16_t known=forward_[raw];
        if(known>=0) return uint16_t(known);
        if(count_>=CHARACTER_COUNT) throw std::runtime_error("terminal overflow");
        forward_[raw]=int16_t(count_);
        reverse_[count_]=raw;
        return count_++;
    }

    uint8_t decode(uint16_t code) const{
        if(code>=count_) throw std::runtime_error("terminal decode outside map");
        return reverse_[code];
    }

    uint16_t count() const { return count_; }
    const std::array<uint8_t,CHARACTER_COUNT>& reverse() const { return reverse_; }

    void restore(uint16_t count,const std::array<uint8_t,CHARACTER_COUNT>& reverse){
        if(count>CHARACTER_COUNT) throw std::runtime_error("bad terminal count");
        forward_.fill(-1);
        reverse_=reverse;
        count_=count;
        for(uint16_t i=0;i<count_;++i){
            if(forward_[reverse_[i]]>=0) throw std::runtime_error("duplicate terminal");
            forward_[reverse_[i]]=int16_t(i);
        }
    }
};

template<size_t CHANNELS>
class ResistorNode {
    static_assert(CHANNELS>0,"CHANNELS");
    // Newton-basis coefficients. Fixed width before source access.
    std::array<uint16_t,CHANNELS> coeff_{};

    static uint16_t basis(uint16_t page,size_t order){
        uint16_t out=1;
        for(size_t r=0;r<order;++r)
            out=mod_mul(out,uint16_t(page-uint16_t(r)));
        return out;
    }

public:
    constexpr bool always_powered() const { return true; }
    constexpr Unresolved unresolved_condition() const { return Unresolved::BOTH; }
    static constexpr size_t channel_count(){ return CHANNELS; }

    const std::array<uint16_t,CHANNELS>& imprint() const { return coeff_; }
    void set_imprint(const std::array<uint16_t,CHANNELS>& x){
        for(auto v:x) if(v>=PRIME) throw std::runtime_error("coefficient outside field");
        coeff_=x;
    }

    uint16_t resonance(uint16_t page) const{
        if(page>=MAX_PAGES) throw std::runtime_error("page outside bank");
        uint32_t acc=0;
        for(size_t j=0;j<CHANNELS;++j)
            acc=(acc + mod_mul(coeff_[j],basis(page,j)))%PRIME;
        return uint16_t(acc);
    }

    // One-character-at-a-time imprint. The page number itself chooses the
    // next available Newton channel. No page record is stored.
    bool imprint_symbol(uint16_t page,uint16_t symbol){
        if(symbol>=CHARACTER_COUNT) throw std::runtime_error("symbol outside 206");
        const uint16_t predicted=resonance(page);

        if(page>=CHANNELS)
            return predicted==symbol;

        // Higher Newton terms vanish on all earlier pages, so updating channel
        // [page] changes this page without changing any previously imprinted page.
        const uint16_t denom=basis(page,page);
        if(denom==0) throw std::runtime_error("zero Newton basis");
        coeff_[page]=mod_mul(mod_sub(symbol,predicted),mod_inv(denom));
        return resonance(page)==symbol;
    }

    uint16_t effective_resistance(const Probe& p) const{
        if(p.character>=CHARACTER_COUNT) throw std::runtime_error("character outside bank");
        return resonance(p.page)==p.character ? R_FLIP : R_LOW;
    }

    Resolved resolve(const Probe& p) const{
        return effective_resistance(p)>=R_THRESHOLD ? Resolved::POS : Resolved::NEG;
    }
};

template<size_t CHANNELS>
class Field {
    bool on_=false;
    std::vector<ResistorNode<CHANNELS>> nodes_;
public:
    explicit Field(uint32_t count):nodes_(count){}

    void power_on(){ on_=true; }
    void power_off(){ on_=false; }
    SwitchState switch_state() const { return on_?SwitchState::ON:SwitchState::NEITHER; }

    size_t node_count() const { return nodes_.size(); }
    ResistorNode<CHANNELS>& node(size_t i){ return nodes_.at(i); }
    const ResistorNode<CHANNELS>& node(size_t i) const { return nodes_.at(i); }
    std::vector<ResistorNode<CHANNELS>>& nodes(){ return nodes_; }
    const std::vector<ResistorNode<CHANNELS>>& nodes() const { return nodes_; }

    std::vector<Resolved> observe(uint16_t page,uint16_t character) const{
        if(!on_) throw std::runtime_error("NEITHER: machine OFF");
        Probe p{page,character};
        std::vector<Resolved> out;
        out.reserve(nodes_.size());
        for(const auto& n:nodes_) out.push_back(n.resolve(p));
        return out;
    }
};

template<size_t CHANNELS>
struct ParseResult{
    bool ok=true;
    uint64_t bytes=0;
    uint16_t page=0;
    uint32_t position=0;
    uint16_t symbol=0;
    uint16_t predicted=0;
};

template<size_t CHANNELS>
class CharacterStreamImprinter {
    Field<CHANNELS>& field_;
    TerminalMap& terminals_;
    const uint32_t positions_;
    uint64_t offset_=0;

public:
    CharacterStreamImprinter(Field<CHANNELS>& field,TerminalMap& terminals,uint32_t positions)
        :field_(field),terminals_(terminals),positions_(positions){
        if(!positions_ || field_.node_count()!=positions_)
            throw std::runtime_error("parser geometry mismatch");
    }

    ParseResult<CHANNELS> push(uint8_t raw){
        const uint16_t page=uint16_t(offset_/positions_);
        const uint32_t position=uint32_t(offset_%positions_);
        if(page>=MAX_PAGES)
            throw std::runtime_error("source exceeds fixed page bank");

        const uint16_t symbol=terminals_.encode(raw);
        auto& node=field_.node(position);
        const uint16_t before=node.resonance(page);

        if(!node.imprint_symbol(page,symbol)){
            return {false,offset_,page,position,symbol,before};
        }

        ++offset_;
        return {true,offset_,page,position,symbol,node.resonance(page)};
    }

    uint64_t bytes() const { return offset_; }
    uint16_t pages_complete() const { return uint16_t(offset_/positions_); }
    bool at_page_boundary() const { return offset_%positions_==0; }
};

} // namespace trucompute_stream_v11
