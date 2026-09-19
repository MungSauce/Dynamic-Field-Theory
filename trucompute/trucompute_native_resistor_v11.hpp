#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace trucompute_resistor_v11 {

constexpr uint16_t PRIME=65521;
constexpr uint16_t MAX_PAGES=1000;
constexpr uint16_t CHARACTER_COUNT=206;

constexpr uint16_t R_NEG=0;
constexpr uint16_t R_POS=1024;
constexpr uint16_t R_THRESHOLD=512;

enum class NativeCondition : int8_t {
    NEG=-1,
    BOTH=0,
    POS=+1
};

struct Probe {
    uint16_t page=0;
    uint16_t character=0;
};

class PageActivator {
    uint16_t page_;
public:
    explicit constexpr PageActivator(uint16_t page):page_(page){}
    constexpr uint16_t page() const { return page_; }
    constexpr bool selected(const Probe& p) const { return p.page==page_; }
};

class CharacterActivator {
    uint16_t character_;
public:
    explicit constexpr CharacterActivator(uint16_t c):character_(c){}
    constexpr uint16_t character() const { return character_; }
    constexpr bool selected(const Probe& p) const { return p.character==character_; }
};

inline uint16_t mod_mul(uint32_t a,uint32_t b){
    return uint16_t((uint64_t(a)*uint64_t(b))%PRIME);
}
inline uint16_t mod_pow(uint16_t a,uint32_t e){
    uint16_t out=1,base=a;
    while(e){
        if(e&1u) out=mod_mul(out,base);
        base=mod_mul(base,base);
        e>>=1u;
    }
    return out;
}
inline uint16_t mod_inv(uint16_t a){
    if(a==0) throw std::runtime_error("zero inverse");
    return mod_pow(a,PRIME-2);
}

// Continuously present native resistor node.
// BOTH is the substrate condition. The probe changes its effective resistance,
// causing direct POS/NEG resolution with no reset/OFF/NEITHER phase.
template<size_t CHANNELS>
class ResistorNode {
    static_assert(CHANNELS>0,"fixed channel count required");
    std::array<uint16_t,CHANNELS> imprint_{};

public:
    constexpr NativeCondition substrate_condition() const {
        return NativeCondition::BOTH;
    }

    static constexpr size_t channel_count(){ return CHANNELS; }

    const std::array<uint16_t,CHANNELS>& imprint() const { return imprint_; }

    void set_imprint(const std::array<uint16_t,CHANNELS>& v){
        for(auto x:v) if(x>=PRIME) throw std::runtime_error("coefficient outside field");
        imprint_=v;
    }

    // Fixed transfer law: page activator excites the imprinted node.
    // This is one bounded transfer function, not a page response table.
    uint16_t page_resonance(uint16_t page) const {
        if(page>=MAX_PAGES) throw std::runtime_error("page outside fixed activator bank");
        uint16_t power=1;
        uint32_t acc=0;
        for(size_t j=0;j<CHANNELS;++j){
            acc=(acc+mod_mul(imprint_[j],power))%PRIME;
            power=mod_mul(power,page);
        }
        return uint16_t(acc);
    }

    uint16_t effective_resistance(const Probe& p) const {
        if(p.character>=CHARACTER_COUNT)
            throw std::runtime_error("character outside fixed activator bank");
        return page_resonance(p.page)==p.character ? R_POS : R_NEG;
    }

    NativeCondition resolve(const Probe& p) const {
        return effective_resistance(p)>=R_THRESHOLD
            ? NativeCondition::POS
            : NativeCondition::NEG;
    }
};

template<size_t CHANNELS>
struct CapacityFailure {
    bool failed=false;
    uint32_t node=0;
    uint16_t page=0;
    uint16_t expected=0;
    uint16_t predicted=0;
};

// Fixed-capacity reference imprint law.
// CHANNELS is fixed before source access. It is never widened.
template<size_t CHANNELS>
class FixedImprinter {
    static std::array<uint16_t,CHANNELS>
    interpolate_prefix(const std::vector<uint16_t>& y,size_t m){
        if(m==0 || m>CHANNELS) throw std::runtime_error("invalid interpolation width");

        std::vector<std::vector<uint16_t>> a(m,std::vector<uint16_t>(m+1,0));
        for(size_t r=0;r<m;++r){
            uint16_t x=uint16_t(r);
            uint16_t power=1;
            for(size_t c=0;c<m;++c){
                a[r][c]=power;
                power=mod_mul(power,x);
            }
            a[r][m]=y[r];
        }

        for(size_t c=0;c<m;++c){
            size_t pivot=c;
            while(pivot<m && a[pivot][c]==0) ++pivot;
            if(pivot==m) throw std::runtime_error("singular page basis");
            if(pivot!=c) std::swap(a[pivot],a[c]);

            uint16_t inv=mod_inv(a[c][c]);
            for(size_t k=c;k<=m;++k) a[c][k]=mod_mul(a[c][k],inv);

            for(size_t r=0;r<m;++r){
                if(r==c) continue;
                uint16_t factor=a[r][c];
                if(factor==0) continue;
                for(size_t k=c;k<=m;++k){
                    uint16_t sub=mod_mul(factor,a[c][k]);
                    a[r][k]=uint16_t((uint32_t(a[r][k])+PRIME-sub)%PRIME);
                }
            }
        }

        std::array<uint16_t,CHANNELS> out{};
        for(size_t i=0;i<m;++i) out[i]=a[i][m];
        return out;
    }

public:
    static CapacityFailure<CHANNELS>
    imprint_exact(std::vector<ResistorNode<CHANNELS>>& nodes,
                  const std::vector<uint8_t>& source,
                  uint32_t positions){
        if(positions==0 || nodes.size()!=positions)
            throw std::runtime_error("node geometry mismatch");
        if(source.empty() || source.size()%positions!=0)
            throw std::runtime_error("source must contain whole pages");

        uint64_t pages64=source.size()/positions;
        if(pages64>MAX_PAGES) throw std::runtime_error("source exceeds fixed page bank");
        uint16_t pages=uint16_t(pages64);

        for(uint8_t b:source)
            if(b>=CHARACTER_COUNT) throw std::runtime_error("source symbol outside 206 alphabet");

        // Candidate imprint is temporary solver state only.
        std::vector<std::array<uint16_t,CHANNELS>> candidate(nodes.size());

        const size_t fitted_pages=pages<CHANNELS ? pages : CHANNELS;
        for(uint32_t pos=0;pos<positions;++pos){
            std::vector<uint16_t> y;
            y.reserve(fitted_pages);
            for(size_t q=0;q<fitted_pages;++q)
                y.push_back(source[q*positions+pos]);

            candidate[pos]=interpolate_prefix(y,fitted_pages);

            ResistorNode<CHANNELS> test;
            test.set_imprint(candidate[pos]);

            for(uint16_t q=0;q<pages;++q){
                uint16_t predicted=test.page_resonance(q);
                uint16_t expected=source[uint64_t(q)*positions+pos];
                if(predicted!=expected)
                    return {true,pos,q,expected,predicted};
            }
        }

        // Commit only after the entire field proves representable.
        for(size_t i=0;i<nodes.size();++i)
            nodes[i].set_imprint(candidate[i]);

        return {};
    }
};

template<size_t CHANNELS>
struct ObservationFrame {
    Probe probe{};
    std::vector<NativeCondition> responses;
    uint64_t transition_index=0;
    uint32_t selected_page_activators=0;
    uint32_t selected_character_activators=0;
};

// No power switch exists. Field construction means the continuous field exists.
// set_probe transitions directly from the previous selector to the next.
template<size_t CHANNELS>
class Machine {
    std::vector<ResistorNode<CHANNELS>> nodes_;
    std::vector<PageActivator> pages_;
    std::vector<CharacterActivator> characters_;

    bool has_probe_=false;
    Probe current_probe_{};
    uint64_t transitions_=0;

public:
    explicit Machine(uint32_t node_count):nodes_(node_count){
        pages_.reserve(MAX_PAGES);
        characters_.reserve(CHARACTER_COUNT);
        for(uint16_t q=0;q<MAX_PAGES;++q) pages_.emplace_back(q);
        for(uint16_t c=0;c<CHARACTER_COUNT;++c) characters_.emplace_back(c);
    }

    size_t node_count() const { return nodes_.size(); }
    std::vector<ResistorNode<CHANNELS>>& nodes(){ return nodes_; }
    const std::vector<ResistorNode<CHANNELS>>& nodes() const { return nodes_; }
    ResistorNode<CHANNELS>& node(size_t i){ return nodes_.at(i); }
    const ResistorNode<CHANNELS>& node(size_t i) const { return nodes_.at(i); }

    bool has_probe() const { return has_probe_; }
    uint64_t transition_count() const { return transitions_; }

    bool all_substrates_both() const {
        for(const auto& n:nodes_)
            if(n.substrate_condition()!=NativeCondition::BOTH) return false;
        return true;
    }

    ObservationFrame<CHANNELS> set_probe(uint16_t page,uint16_t character){
        Probe p{page,character};
        ObservationFrame<CHANNELS> f;
        f.probe=p;

        for(const auto& a:pages_) if(a.selected(p)) ++f.selected_page_activators;
        for(const auto& a:characters_) if(a.selected(p)) ++f.selected_character_activators;

        if(f.selected_page_activators!=1)
            throw std::runtime_error("page selector is not one-hot");
        if(f.selected_character_activators!=1)
            throw std::runtime_error("character selector is not one-hot");

        // Direct transition; no reset or neutral phase.
        current_probe_=p;
        has_probe_=true;
        f.transition_index=++transitions_;

        f.responses.reserve(nodes_.size());
        for(const auto& n:nodes_){
            NativeCondition r=n.resolve(p);
            if(r==NativeCondition::BOTH)
                throw std::runtime_error("probe may resolve only NEG/POS");
            f.responses.push_back(r);
        }
        return f;
    }
};

} // namespace trucompute_resistor_v11
