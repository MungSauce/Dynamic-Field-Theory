#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace trucompute_fixedkbd_v13 {

constexpr uint16_t PRIME=65521;
constexpr uint16_t MAX_PAGES=1000;
constexpr uint16_t CHARACTER_BUTTONS=206;
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

class CharacterButton {
    uint16_t id_;
public:
    explicit constexpr CharacterButton(uint16_t id=0):id_(id){}
    constexpr uint16_t id() const { return id_; }
};

// Permanently-coupled machine component.
// All 206 buttons exist before source access and never change identity.
class CharacterKeyboard {
    std::array<CharacterButton,CHARACTER_BUTTONS> buttons_;
public:
    CharacterKeyboard(){
        for(uint16_t i=0;i<CHARACTER_BUTTONS;++i)
            buttons_[i]=CharacterButton(i);
    }

    static constexpr uint16_t physical_button_count(){ return CHARACTER_BUTTONS; }

    const CharacterButton& press(uint16_t id) const{
        if(id>=CHARACTER_BUTTONS)
            throw std::runtime_error("symbol has no permanently coupled character button");
        return buttons_[id];
    }
};

struct Probe{
    uint16_t page=0;
    uint16_t character_button=0;
};

template<size_t CHANNELS>
class ResistorNode {
    static_assert(CHANNELS>0,"CHANNELS");
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

    const std::array<uint16_t,CHANNELS>& imprint() const { return coeff_; }
    void set_imprint(const std::array<uint16_t,CHANNELS>& x){
        for(auto v:x) if(v>=PRIME) throw std::runtime_error("coefficient outside field");
        coeff_=x;
    }

    uint16_t resonance(uint16_t page) const{
        if(page>=MAX_PAGES) throw std::runtime_error("page outside fixed bank");
        uint32_t acc=0;
        for(size_t j=0;j<CHANNELS;++j)
            acc=(acc + mod_mul(coeff_[j],basis(page,j)))%PRIME;
        return uint16_t(acc);
    }

    bool tune_under_selected_buttons(uint16_t page,const CharacterButton& character){
        const uint16_t target=character.id();
        const uint16_t predicted=resonance(page);

        if(page>=CHANNELS)
            return predicted==target;

        const uint16_t denom=basis(page,page);
        if(!denom) throw std::runtime_error("zero Newton basis");
        coeff_[page]=mod_mul(mod_sub(target,predicted),mod_inv(denom));
        return resonance(page)==target;
    }

    uint16_t effective_resistance(const Probe& p) const{
        if(p.character_button>=CHARACTER_BUTTONS)
            throw std::runtime_error("button outside fixed bank");
        return resonance(p.page)==p.character_button ? R_FLIP : R_LOW;
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

    std::vector<Resolved> observe(uint16_t page,const CharacterButton& button) const{
        if(!on_) throw std::runtime_error("NEITHER: machine OFF");
        Probe p{page,button.id()};
        std::vector<Resolved> out;
        out.reserve(nodes_.size());
        for(const auto& n:nodes_) out.push_back(n.resolve(p));
        return out;
    }
};

template<size_t CHANNELS>
struct ParseResult{
    bool ok=true;
    uint64_t symbols_accepted=0;
    uint16_t page=0;
    uint32_t position=0;
    uint16_t pressed_button=0;
    uint16_t predicted_button=0;
};

template<size_t CHANNELS>
class KeyboardImprinter {
    Field<CHANNELS>& field_;
    const CharacterKeyboard& keyboard_;
    uint32_t positions_;
    uint64_t offset_=0;
public:
    KeyboardImprinter(Field<CHANNELS>& field,
                      const CharacterKeyboard& keyboard,
                      uint32_t positions)
        :field_(field),keyboard_(keyboard),positions_(positions){
        if(!positions_ || field_.node_count()!=positions_)
            throw std::runtime_error("imprinter geometry mismatch");
    }

    // Input is already a machine symbol identity 0..205.
    // The writer can only press a permanently-existing character button.
    ParseResult<CHANNELS> type_symbol(uint16_t symbol){
        const uint16_t page=uint16_t(offset_/positions_);
        const uint32_t position=uint32_t(offset_%positions_);
        if(page>=MAX_PAGES) throw std::runtime_error("source exceeds page bank");

        const auto& key=keyboard_.press(symbol);
        auto& node=field_.node(position);
        const uint16_t before=node.resonance(page);

        if(!node.tune_under_selected_buttons(page,key))
            return {false,offset_,page,position,key.id(),before};

        ++offset_;
        return {true,offset_,page,position,key.id(),node.resonance(page)};
    }

    uint64_t symbols() const { return offset_; }
    bool at_page_boundary() const { return offset_%positions_==0; }
    uint16_t pages_complete() const { return uint16_t(offset_/positions_); }
};

} // namespace trucompute_fixedkbd_v13
