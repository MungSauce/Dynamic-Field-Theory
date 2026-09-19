#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace trucompute_four_action_v11 {

constexpr uint16_t MAX_PAGES = 1000;
constexpr uint16_t CHARACTER_COUNT = 206;
constexpr uint16_t R_LOW = 0;
constexpr uint16_t R_FLIP = 1000;
constexpr uint16_t R_THRESHOLD = 500;

// Four native actions. The numeric values are only a host serialization code.
// TruCompute semantics are defined by the action law below.
enum class Action : uint8_t {
    NEITHER = 0b00, // no power / no participation
    NEG     = 0b01, // resolved false
    BOTH    = 0b10, // powered, unresolved: +1 and -1 coexist
    POS     = 0b11  // resolved true
};

static_assert(static_cast<uint8_t>(Action::POS) < 4, "four actions must fit two host bits");

inline constexpr uint8_t encode_host_2bit(Action a) {
    return static_cast<uint8_t>(a) & 0b11u;
}

inline constexpr Action decode_host_2bit(uint8_t code) {
    return code == 0 ? Action::NEITHER :
           code == 1 ? Action::NEG :
           code == 2 ? Action::BOTH :
           code == 3 ? Action::POS :
           throw std::runtime_error("invalid 2-bit action code");
}

struct Probe {
    uint16_t page = 0;
    uint16_t character = 0;
};

class PageActivator {
    uint16_t page_;
public:
    explicit constexpr PageActivator(uint16_t p):page_(p){}
    constexpr bool selected(const Probe& p) const { return p.page == page_; }
    constexpr uint16_t page() const { return page_; }
};

class CharacterActivator {
    uint16_t character_;
public:
    explicit constexpr CharacterActivator(uint16_t c):character_(c){}
    constexpr bool selected(const Probe& p) const { return p.character == character_; }
    constexpr uint16_t character() const { return character_; }
};

// Reference fixed-width transfer law carried over from v10.
// The important v11 correction is the four-action node interface:
// power + optional observation condition -> one of four native actions.
template<size_t CHANNELS>
class FourActionResistorNode {
    static_assert(CHANNELS > 0, "CHANNELS must be nonzero");
    std::array<uint16_t, CHANNELS> imprint_{};

public:
    static constexpr size_t channel_count() { return CHANNELS; }

    const std::array<uint16_t,CHANNELS>& imprint() const { return imprint_; }

    void set_imprint(const std::array<uint16_t,CHANNELS>& v) {
        imprint_ = v;
    }

    // Simple bounded reference transfer: coefficients are mixed with page.
    // This is not a page table; it is a fixed-width node characteristic.
    uint16_t resonance(uint16_t page) const {
        if(page >= MAX_PAGES) throw std::runtime_error("page outside fixed bank");
        uint32_t acc = 0;
        uint32_t x = uint32_t(page) + 1u;
        uint32_t power = 1u;
        for(size_t i=0;i<CHANNELS;++i) {
            acc = (acc + uint32_t(imprint_[i]) * power) % 65521u;
            power = (power * x) % 65521u;
        }
        return uint16_t(acc);
    }

    uint16_t effective_resistance(const Probe& p) const {
        if(p.character >= CHARACTER_COUNT)
            throw std::runtime_error("character outside fixed bank");
        return resonance(p.page) == p.character ? R_FLIP : R_LOW;
    }

    // This is the native four-action law.
    // Nothing here writes back into the node.
    Action act(bool powered, const std::optional<Probe>& observation = std::nullopt) const {
        if(!powered) return Action::NEITHER;
        if(!observation.has_value()) return Action::BOTH;
        return effective_resistance(*observation) >= R_THRESHOLD
            ? Action::POS
            : Action::NEG;
    }
};

template<size_t CHANNELS>
class Field {
    bool powered_ = false;
    std::vector<FourActionResistorNode<CHANNELS>> nodes_;
    std::array<PageActivator,MAX_PAGES> pages_;
    std::array<CharacterActivator,CHARACTER_COUNT> characters_;

    static std::array<PageActivator,MAX_PAGES> make_pages() {
        std::array<PageActivator,MAX_PAGES> a = []{
            std::array<PageActivator,MAX_PAGES> tmp{
                PageActivator(0)
            };
            return tmp;
        }();
        // std::array cannot default-construct this type portably; overwrite via placement-like assignment
        for(uint16_t i=0;i<MAX_PAGES;++i) a[i]=PageActivator(i);
        return a;
    }

    static std::array<CharacterActivator,CHARACTER_COUNT> make_chars() {
        std::array<CharacterActivator,CHARACTER_COUNT> a = []{
            std::array<CharacterActivator,CHARACTER_COUNT> tmp{
                CharacterActivator(0)
            };
            return tmp;
        }();
        for(uint16_t i=0;i<CHARACTER_COUNT;++i) a[i]=CharacterActivator(i);
        return a;
    }

public:
    explicit Field(uint32_t node_count)
        : nodes_(node_count), pages_(make_pages()), characters_(make_chars()) {}

    void power_on() { powered_ = true; }
    void power_off() { powered_ = false; }
    bool powered() const { return powered_; }

    size_t node_count() const { return nodes_.size(); }
    FourActionResistorNode<CHANNELS>& node(size_t i) { return nodes_.at(i); }
    const FourActionResistorNode<CHANNELS>& node(size_t i) const { return nodes_.at(i); }

    // No selector: every powered data node is BOTH.
    std::vector<Action> unresolved_actions() const {
        std::vector<Action> out;
        out.reserve(nodes_.size());
        for(const auto& n:nodes_) out.push_back(n.act(powered_));
        return out;
    }

    // Selected page+character: every powered data node resolves POS or NEG.
    std::vector<Action> observe(uint16_t page,uint16_t character) const {
        if(page>=MAX_PAGES || character>=CHARACTER_COUNT)
            throw std::runtime_error("selector outside fixed bank");

        Probe p{page,character};
        if(!pages_[page].selected(p) || !characters_[character].selected(p))
            throw std::runtime_error("selector activation failure");

        std::vector<Action> out;
        out.reserve(nodes_.size());
        for(const auto& n:nodes_) out.push_back(n.act(powered_,p));
        return out;
    }
};

} // namespace trucompute_four_action_v11
