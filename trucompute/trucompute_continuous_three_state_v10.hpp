#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace trucompute_continuous_v10 {

// The native three-condition TruCompute domain.
// BOTH is not numeric zero and is never an OFF state.
enum class NativeCondition : int8_t {
    NEG  = -1,
    BOTH = 0,
    POS  = +1
};

struct Probe {
    uint16_t page = 0;
    uint16_t character = 0;
};

struct PageCondition {
    uint16_t page = 0;
    uint16_t symbol = 0;
};

class PageActivator {
    uint16_t page_;
public:
    explicit PageActivator(uint16_t page):page_(page){}
    uint16_t page() const { return page_; }
    bool selected(const Probe& p) const { return p.page==page_; }
};

class CharacterActivator {
    uint16_t character_;
public:
    explicit CharacterActivator(uint16_t c):character_(c){}
    uint16_t character() const { return character_; }
    bool selected(const Probe& p) const { return p.character==character_; }
};

// A TruCompute data node is continuously present.
// Its substrate is intrinsically BOTH.
// Probe changes resolve that same node to POS or NEG without an intervening
// OFF/zero/NEITHER state and without rewriting the substrate.
class DataNode {
    uint32_t position_;
    std::vector<PageCondition> conditions_;

public:
    explicit DataNode(uint32_t position):position_(position){}

    uint32_t position() const { return position_; }

    constexpr NativeCondition substrate_condition() const {
        return NativeCondition::BOTH;
    }

    void imprint(uint16_t page,uint16_t symbol){
        for(const auto& c:conditions_){
            if(c.page!=page) continue;
            if(c.symbol==symbol) return;
            throw std::runtime_error("same page has conflicting symbol");
        }
        conditions_.push_back({page,symbol});
    }

    uint16_t symbol_for(uint16_t page) const {
        for(const auto& c:conditions_)
            if(c.page==page) return c.symbol;
        throw std::runtime_error("page condition missing");
    }

    NativeCondition resolve(const Probe& p) const {
        return symbol_for(p.page)==p.character
            ? NativeCondition::POS
            : NativeCondition::NEG;
    }

    const std::vector<PageCondition>& conditions() const { return conditions_; }
};

struct Frame {
    Probe probe{};
    std::vector<NativeCondition> responses;
    uint32_t selected_page_activators=0;
    uint32_t unselected_page_activators=0;
    uint32_t selected_character_activators=0;
    uint32_t unselected_character_activators=0;
    uint64_t transition_index=0;
};

// The machine has no OFF state.
// Construction creates an already-present continuous field.
// set_probe() changes the condition applied to the field; it does not reset it.
class Field {
    std::vector<std::unique_ptr<DataNode>> data_;
    std::vector<PageActivator> pages_;
    std::vector<CharacterActivator> characters_;

    bool has_probe_=false;
    Probe current_probe_{};
    uint64_t transitions_=0;

public:
    DataNode& add_data_node(uint32_t position){
        auto p=std::make_unique<DataNode>(position);
        DataNode& r=*p;
        data_.push_back(std::move(p));
        return r;
    }

    void add_page_activator(uint16_t page){ pages_.emplace_back(page); }
    void add_character_activator(uint16_t c){ characters_.emplace_back(c); }

    size_t data_node_count() const { return data_.size(); }
    size_t page_activator_count() const { return pages_.size(); }
    size_t character_activator_count() const { return characters_.size(); }

    DataNode& data_node(size_t i){ return *data_.at(i); }
    const DataNode& data_node(size_t i) const { return *data_.at(i); }

    bool has_probe() const { return has_probe_; }
    Probe current_probe() const {
        if(!has_probe_) throw std::runtime_error("no probe selected yet");
        return current_probe_;
    }
    uint64_t transition_count() const { return transitions_; }

    bool all_substrates_both() const {
        for(const auto& n:data_)
            if(n->substrate_condition()!=NativeCondition::BOTH) return false;
        return true;
    }

    Frame set_probe(uint16_t page,uint16_t character){
        Probe p{page,character};

        Frame f;
        f.probe=p;

        for(const auto& a:pages_){
            if(a.selected(p)) ++f.selected_page_activators;
            else ++f.unselected_page_activators;
        }
        for(const auto& a:characters_){
            if(a.selected(p)) ++f.selected_character_activators;
            else ++f.unselected_character_activators;
        }

        if(f.selected_page_activators!=1)
            throw std::runtime_error("exactly one page activator must be selected");
        if(f.selected_character_activators!=1)
            throw std::runtime_error("exactly one character activator must be selected");

        // Direct transition: old probe -> new probe. No zero/OFF/reset phase.
        current_probe_=p;
        has_probe_=true;
        ++transitions_;
        f.transition_index=transitions_;

        f.responses.reserve(data_.size());
        for(const auto& n:data_){
            const auto r=n->resolve(p);
            if(r==NativeCondition::BOTH)
                throw std::runtime_error("active page-letter probe resolved to BOTH");
            f.responses.push_back(r);
        }

        return f;
    }
};

} // namespace trucompute_continuous_v10
