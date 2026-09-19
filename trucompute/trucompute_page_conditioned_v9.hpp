#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace trucompute_conditioned_v9 {

enum class Resolved : int8_t {
    NEG = -1,
    POS = +1
};

enum class Unresolved : uint8_t {
    BOTH
};

enum class SwitchState : uint8_t {
    NEITHER,
    ON
};

struct Selector {
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
    explicit PageActivator(uint16_t p):page_(p){}
    bool selected(const Selector& s) const { return s.page==page_; }
    uint16_t page() const { return page_; }
};

class CharacterActivator {
    uint16_t character_;
public:
    explicit CharacterActivator(uint16_t c):character_(c){}
    bool selected(const Selector& s) const { return s.character==character_; }
    uint16_t character() const { return character_; }
};

// The native data node is continuously powered.
// Its unresolved field condition is BOTH.
// Supplying (page, character) changes its resolved output by design.
class DataNode {
    uint32_t position_;
    std::vector<PageCondition> conditions_;

public:
    explicit DataNode(uint32_t position):position_(position){}

    uint32_t position() const { return position_; }
    constexpr bool always_powered() const { return true; }
    constexpr Unresolved unresolved_condition() const { return Unresolved::BOTH; }

    void imprint(uint16_t page,uint16_t symbol){
        for(const auto& c:conditions_){
            if(c.page!=page) continue;
            if(c.symbol==symbol) return; // idempotent same condition
            throw std::runtime_error("same page has conflicting symbol");
        }
        conditions_.push_back({page,symbol});
    }

    uint16_t symbol_for(uint16_t page) const {
        for(const auto& c:conditions_)
            if(c.page==page) return c.symbol;
        throw std::runtime_error("page condition missing from powered node");
    }

    Resolved resolve(const Selector& s) const {
        return symbol_for(s.page)==s.character ? Resolved::POS : Resolved::NEG;
    }

    const std::vector<PageCondition>& conditions() const { return conditions_; }
};

struct QueryFrame {
    Selector selector{};
    std::vector<Resolved> responses;
    uint32_t selected_page_activators=0;
    uint32_t unselected_page_activators=0;
    uint32_t selected_character_activators=0;
    uint32_t unselected_character_activators=0;
};

class Field {
    bool on_=false;
    std::vector<std::unique_ptr<DataNode>> data_;
    std::vector<PageActivator> pages_;
    std::vector<CharacterActivator> characters_;

public:
    void power_on(){ on_=true; }
    void power_off(){ on_=false; }
    bool powered() const { return on_; }

    SwitchState switch_state() const {
        return on_ ? SwitchState::ON : SwitchState::NEITHER;
    }

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

    bool all_data_nodes_powered() const {
        for(const auto& n:data_) if(!n->always_powered()) return false;
        return true;
    }

    QueryFrame observe(uint16_t page,uint16_t character) const {
        if(!on_) throw std::runtime_error("NEITHER: machine OFF");

        Selector s{page,character};
        QueryFrame frame;
        frame.selector=s;

        for(const auto& a:pages_){
            if(a.selected(s)) ++frame.selected_page_activators;
            else ++frame.unselected_page_activators;
        }
        for(const auto& a:characters_){
            if(a.selected(s)) ++frame.selected_character_activators;
            else ++frame.unselected_character_activators;
        }

        if(frame.selected_page_activators!=1)
            throw std::runtime_error("exactly one page activator must be selected");
        if(frame.selected_character_activators!=1)
            throw std::runtime_error("exactly one character activator must be selected");

        frame.responses.reserve(data_.size());
        for(const auto& n:data_)
            frame.responses.push_back(n->resolve(s));

        return frame;
    }
};

} // namespace trucompute_conditioned_v9
