#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace trucompute_onehot_v11 {

constexpr uint16_t PRIME = 65521;
constexpr uint16_t PAGE_BUTTONS = 1000;
constexpr uint16_t CHARACTER_BUTTONS = 206;

enum class Truth : uint8_t {
    FALSE_VALUE = 0,
    TRUE_VALUE = 1
};

enum class Power : uint8_t {
    OFF = 0,
    ON = 1
};

template<size_t N>
class OneHotButtonBank {
    std::array<Truth,N> labels_{};
    bool has_selection_ = false;
    uint16_t selected_ = 0;

public:
    OneHotButtonBank() {
        labels_.fill(Truth::FALSE_VALUE);
    }

    void press(uint16_t index) {
        if(index>=N) throw std::runtime_error("button index outside fixed bank");
        labels_.fill(Truth::FALSE_VALUE);
        labels_[index]=Truth::TRUE_VALUE;
        selected_=index;
        has_selection_=true;
    }

    Truth label(uint16_t index) const {
        if(index>=N) throw std::runtime_error("button index outside fixed bank");
        return labels_[index];
    }

    bool has_selection() const { return has_selection_; }

    uint16_t selected() const {
        if(!has_selection_) throw std::runtime_error("no button selected");
        return selected_;
    }

    size_t true_count() const {
        size_t n=0;
        for(auto x:labels_) if(x==Truth::TRUE_VALUE) ++n;
        return n;
    }

    size_t false_count() const {
        return N-true_count();
    }

    static constexpr size_t size(){ return N; }
};

inline uint16_t mod_mul(uint32_t a,uint32_t b){
    return uint16_t((uint64_t(a)*uint64_t(b))%PRIME);
}
inline uint16_t mod_pow(uint16_t a,uint32_t e){
    uint16_t base=a;
    uint16_t out=1;
    while(e){
        if(e&1u) out=mod_mul(out,base);
        base=mod_mul(base,base);
        e>>=1u;
    }
    return out;
}
inline uint16_t mod_inv(uint16_t a){
    if(a==0) throw std::runtime_error("zero has no inverse");
    return mod_pow(a,PRIME-2);
}

// Continuously powered native data element.
//
// Source-dependent state is a fixed-width transfer imprint only.
// No page table, character table, response cache, or residual stream exists.
//
// Page button selects the operating condition.
// Character button selects the probe.
// The node returns exactly TRUE or FALSE.
template<size_t CHANNELS>
class NativeResistorNode {
    static_assert(CHANNELS>0,"fixed imprint must have at least one channel");
    std::array<uint16_t,CHANNELS> imprint_{};

public:
    static constexpr size_t channel_count(){ return CHANNELS; }
    constexpr bool continuously_powered() const { return true; }

    void set_imprint(const std::array<uint16_t,CHANNELS>& x){
        for(auto v:x) if(v>=PRIME) throw std::runtime_error("imprint coefficient outside native field");
        imprint_=x;
    }

    const std::array<uint16_t,CHANNELS>& imprint() const { return imprint_; }

    uint16_t page_resonance(uint16_t page) const {
        if(page>=PAGE_BUTTONS) throw std::runtime_error("page outside fixed button bank");
        uint16_t acc=0;
        uint16_t power=1;
        for(size_t k=0;k<CHANNELS;++k){
            acc=uint16_t((uint32_t(acc)+mod_mul(imprint_[k],power))%PRIME);
            power=mod_mul(power,page);
        }
        return acc;
    }

    // "No resistance" = false. "Enough resistance to flip" = true.
    Truth probe(uint16_t page,uint16_t character) const {
        if(character>=CHARACTER_BUTTONS)
            throw std::runtime_error("character outside fixed button bank");
        return page_resonance(page)==character
            ? Truth::TRUE_VALUE
            : Truth::FALSE_VALUE;
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

template<size_t CHANNELS>
class FixedWidthImprinter {
    static std::array<uint16_t,CHANNELS>
    solve_prefix(const std::vector<uint16_t>& y,size_t m){
        if(m==0 || m>CHANNELS) throw std::runtime_error("invalid interpolation width");

        std::vector<std::vector<uint16_t>> a(m,std::vector<uint16_t>(m+1,0));
        for(size_t r=0;r<m;++r){
            uint16_t power=1;
            for(size_t c=0;c<m;++c){
                a[r][c]=power;
                power=mod_mul(power,uint16_t(r));
            }
            a[r][m]=y[r];
        }

        for(size_t col=0;col<m;++col){
            size_t pivot=col;
            while(pivot<m && a[pivot][col]==0) ++pivot;
            if(pivot==m) throw std::runtime_error("singular page basis");
            if(pivot!=col) std::swap(a[pivot],a[col]);

            const uint16_t inv=mod_inv(a[col][col]);
            for(size_t k=col;k<=m;++k)
                a[col][k]=mod_mul(a[col][k],inv);

            for(size_t row=0;row<m;++row){
                if(row==col) continue;
                const uint16_t factor=a[row][col];
                if(factor==0) continue;
                for(size_t k=col;k<=m;++k){
                    const uint16_t sub=mod_mul(factor,a[col][k]);
                    a[row][k]=uint16_t((uint32_t(a[row][k])+PRIME-sub)%PRIME);
                }
            }
        }

        std::array<uint16_t,CHANNELS> coeff{};
        for(size_t i=0;i<m;++i) coeff[i]=a[i][m];
        return coeff;
    }

public:
    static ImprintFailure<CHANNELS>
    imprint_exact(std::vector<NativeResistorNode<CHANNELS>>& nodes,
                  const std::vector<uint8_t>& source,
                  uint32_t positions){
        if(positions==0 || nodes.size()!=positions)
            throw std::runtime_error("node geometry mismatch");
        if(source.empty() || source.size()%positions!=0)
            throw std::runtime_error("source must contain complete pages");

        const uint64_t pages64=source.size()/positions;
        if(pages64>PAGE_BUTTONS)
            throw std::runtime_error("source exceeds fixed page bank");
        const uint16_t pages=uint16_t(pages64);

        for(auto b:source)
            if(b>=CHARACTER_BUTTONS)
                throw std::runtime_error("source symbol outside 206-button alphabet");

        std::vector<std::array<uint16_t,CHANNELS>> candidate(nodes.size());
        const size_t fitted=pages<CHANNELS?pages:CHANNELS;

        for(uint32_t pos=0;pos<positions;++pos){
            std::vector<uint16_t> y;
            y.reserve(fitted);
            for(size_t q=0;q<fitted;++q)
                y.push_back(source[q*positions+pos]);

            candidate[pos]=solve_prefix(y,fitted);

            NativeResistorNode<CHANNELS> probe;
            probe.set_imprint(candidate[pos]);

            for(uint16_t q=0;q<pages;++q){
                const uint16_t expected=source[uint64_t(q)*positions+pos];
                const uint16_t predicted=probe.page_resonance(q);
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
class Machine {
    Power power_ = Power::OFF;
    OneHotButtonBank<PAGE_BUTTONS> page_buttons_;
    OneHotButtonBank<CHARACTER_BUTTONS> character_buttons_;
    std::vector<NativeResistorNode<CHANNELS>> data_nodes_;

public:
    explicit Machine(uint32_t data_nodes):data_nodes_(data_nodes){}

    void power_on(){ power_=Power::ON; }
    void power_off(){ power_=Power::OFF; }
    Power power() const { return power_; }

    void press_page(uint16_t page){ page_buttons_.press(page); }
    void press_character(uint16_t c){ character_buttons_.press(c); }

    const auto& page_buttons() const { return page_buttons_; }
    const auto& character_buttons() const { return character_buttons_; }

    size_t data_node_count() const { return data_nodes_.size(); }
    std::vector<NativeResistorNode<CHANNELS>>& data_nodes(){ return data_nodes_; }
    const std::vector<NativeResistorNode<CHANNELS>>& data_nodes() const { return data_nodes_; }

    std::vector<Truth> observe() const {
        if(power_!=Power::ON) throw std::runtime_error("machine OFF");
        if(!page_buttons_.has_selection()) throw std::runtime_error("no page button TRUE");
        if(!character_buttons_.has_selection()) throw std::runtime_error("no character button TRUE");

        const uint16_t page=page_buttons_.selected();
        const uint16_t character=character_buttons_.selected();

        std::vector<Truth> out;
        out.reserve(data_nodes_.size());
        for(const auto& n:data_nodes_)
            out.push_back(n.probe(page,character));
        return out;
    }
};

} // namespace trucompute_onehot_v11
