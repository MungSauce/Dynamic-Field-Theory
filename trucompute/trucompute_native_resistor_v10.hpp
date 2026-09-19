#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace trucompute_resistive_v10 {

constexpr uint16_t PRIME = 65521;
constexpr uint16_t MAX_PAGES = 1000;
constexpr uint16_t CHARACTER_COUNT = 206;

constexpr uint16_t R_LOW = 0;
constexpr uint16_t R_FLIP = 1000;
constexpr uint16_t R_THRESHOLD = 500;

enum class Unresolved : uint8_t { BOTH };
enum class Resolved : int8_t { NEG = -1, POS = +1 };
enum class SwitchState : uint8_t { NEITHER, ON };

struct Probe {
    uint16_t page = 0;
    uint16_t character = 0;
};

class PageActivator {
    uint16_t page_;
public:
    explicit constexpr PageActivator(uint16_t page):page_(page){}
    constexpr uint16_t page() const { return page_; }
    constexpr bool selected(const Probe& p) const { return p.page == page_; }
};

class CharacterActivator {
    uint16_t character_;
public:
    explicit constexpr CharacterActivator(uint16_t c):character_(c){}
    constexpr uint16_t character() const { return character_; }
    constexpr bool selected(const Probe& p) const { return p.character == character_; }
};

inline uint16_t mod_mul(uint32_t a,uint32_t b){
    return uint16_t((uint64_t(a)*uint64_t(b))%PRIME);
}
inline uint16_t mod_pow(uint16_t a,uint32_t e){
    uint32_t base=a;
    uint32_t out=1;
    while(e){
        if(e&1u) out=mod_mul(out,base);
        base=mod_mul(base,base);
        e>>=1u;
    }
    return uint16_t(out);
}
inline uint16_t mod_inv(uint16_t a){
    if(a==0) throw std::runtime_error("zero has no inverse");
    return mod_pow(a,PRIME-2);
}

// Continuously powered native transfer element.
// Source-dependent persistent state is exactly one fixed-width imprint vector.
// There are no per-page/per-character records in the node.
template<size_t CHANNELS>
class ResistorNode {
    static_assert(CHANNELS>0,"CHANNELS must be positive");
    std::array<uint16_t,CHANNELS> imprint_{};

public:
    constexpr bool always_powered() const { return true; }
    constexpr Unresolved unresolved_condition() const { return Unresolved::BOTH; }
    static constexpr size_t channel_count(){ return CHANNELS; }

    const std::array<uint16_t,CHANNELS>& imprint() const { return imprint_; }

    void set_imprint(const std::array<uint16_t,CHANNELS>& v){
        for(auto x:v) if(x>=PRIME) throw std::runtime_error("imprint coefficient outside field");
        imprint_=v;
    }

    // Page activator drives the fixed transfer law; there is no page lookup table.
    uint16_t resonance(uint16_t page) const {
        if(page>=MAX_PAGES) throw std::runtime_error("page outside fixed activator bank");
        uint32_t acc=0;
        uint32_t power=1;
        for(size_t j=0;j<CHANNELS;++j){
            acc=(acc + uint32_t(mod_mul(imprint_[j],power)))%PRIME;
            power=mod_mul(power,page);
        }
        return uint16_t(acc);
    }

    // Character probe measures whether resistance reaches the flip threshold.
    uint16_t effective_resistance(const Probe& p) const {
        if(p.character>=CHARACTER_COUNT)
            throw std::runtime_error("character outside fixed activator bank");
        return resonance(p.page)==p.character ? R_FLIP : R_LOW;
    }

    Resolved resolve(const Probe& p) const {
        return effective_resistance(p)>=R_THRESHOLD ? Resolved::POS : Resolved::NEG;
    }
};

template<size_t CHANNELS>
struct ImprintFailure {
    bool failed=false;
    uint32_t node=0;
    uint16_t page=0;
    uint16_t expected=0;
    uint16_t predicted=0;
};

// Exact bounded imprinter. Channel width is fixed before source access.
// Failure is reported rather than adding storage.
template<size_t CHANNELS>
class Imprinter {
    static std::array<uint16_t,CHANNELS>
    solve_prefix(const std::vector<uint16_t>& y,size_t m){
        if(m==0 || m>CHANNELS) throw std::runtime_error("invalid interpolation width");

        std::vector<std::vector<uint16_t>> a(m,std::vector<uint16_t>(m+1,0));
        for(size_t row=0;row<m;++row){
            uint16_t power=1;
            const uint16_t x=uint16_t(row);
            for(size_t col=0;col<m;++col){
                a[row][col]=power;
                power=mod_mul(power,x);
            }
            a[row][m]=y[row];
        }

        for(size_t col=0;col<m;++col){
            size_t pivot=col;
            while(pivot<m && a[pivot][col]==0) ++pivot;
            if(pivot==m) throw std::runtime_error("singular fixed page basis");
            if(pivot!=col) std::swap(a[pivot],a[col]);

            const uint16_t inv=mod_inv(a[col][col]);
            for(size_t k=col;k<=m;++k)
                a[col][k]=mod_mul(a[col][k],inv);

            for(size_t row=0;row<m;++row){
                if(row==col) continue;
                const uint16_t factor=a[row][col];
                if(factor==0) continue;
                for(size_t k=col;k<=m;++k){
                    const uint32_t sub=mod_mul(factor,a[col][k]);
                    a[row][k]=uint16_t((uint32_t(a[row][k])+PRIME-sub)%PRIME);
                }
            }
        }

        std::array<uint16_t,CHANNELS> out{};
        for(size_t i=0;i<m;++i) out[i]=a[i][m];
        return out;
    }

public:
    static ImprintFailure<CHANNELS>
    imprint_exact(std::vector<ResistorNode<CHANNELS>>& nodes,
                  const std::vector<uint8_t>& source,
                  uint32_t positions){
        if(positions==0 || nodes.size()!=positions)
            throw std::runtime_error("node geometry mismatch");
        if(source.empty() || source.size()%positions!=0)
            throw std::runtime_error("source must contain whole pages");

        const uint64_t pages64=source.size()/positions;
        if(pages64>MAX_PAGES) throw std::runtime_error("source exceeds fixed page activator bank");
        const uint16_t pages=uint16_t(pages64);

        for(uint8_t b:source)
            if(b>=CHARACTER_COUNT) throw std::runtime_error("source symbol outside 206-character alphabet");

        std::vector<std::array<uint16_t,CHANNELS>> candidate(nodes.size());
        const size_t fit_pages=pages<CHANNELS ? pages : CHANNELS;

        for(uint32_t pos=0;pos<positions;++pos){
            std::vector<uint16_t> y;
            y.reserve(fit_pages);
            for(size_t q=0;q<fit_pages;++q)
                y.push_back(source[q*positions+pos]);

            candidate[pos]=solve_prefix(y,fit_pages);

            ResistorNode<CHANNELS> probe;
            probe.set_imprint(candidate[pos]);
            for(uint16_t q=0;q<pages;++q){
                const uint16_t predicted=probe.resonance(q);
                const uint16_t expected=source[uint64_t(q)*positions+pos];
                if(predicted!=expected)
                    return {true,pos,q,expected,predicted};
            }
        }

        for(size_t i=0;i<nodes.size();++i)
            nodes[i].set_imprint(candidate[i]);

        return {};
    }
};

template<size_t CHANNELS>
class Field {
    bool on_=false;
    std::vector<ResistorNode<CHANNELS>> nodes_;

public:
    explicit Field(uint32_t node_count):nodes_(node_count){}

    void power_on(){ on_=true; }
    void power_off(){ on_=false; }
    bool powered() const { return on_; }
    SwitchState switch_state() const { return on_ ? SwitchState::ON : SwitchState::NEITHER; }

    size_t node_count() const { return nodes_.size(); }
    ResistorNode<CHANNELS>& node(size_t i){ return nodes_.at(i); }
    const ResistorNode<CHANNELS>& node(size_t i) const { return nodes_.at(i); }
    std::vector<ResistorNode<CHANNELS>>& nodes(){ return nodes_; }
    const std::vector<ResistorNode<CHANNELS>>& nodes() const { return nodes_; }

    std::vector<Resolved> observe(uint16_t page,uint16_t character) const {
        if(!on_) throw std::runtime_error("NEITHER: machine OFF");
        if(page>=MAX_PAGES) throw std::runtime_error("page selector outside fixed bank");
        if(character>=CHARACTER_COUNT) throw std::runtime_error("character selector outside fixed bank");

        Probe p{page,character};
        std::vector<Resolved> out;
        out.reserve(nodes_.size());
        for(const auto& n:nodes_)
            out.push_back(n.resolve(p));
        return out;
    }
};

} // namespace trucompute_resistive_v10
