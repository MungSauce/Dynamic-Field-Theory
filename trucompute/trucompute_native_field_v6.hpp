#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace trucompute_field_v6 {

enum class Observation : uint8_t {
    NEITHER,
    NEG,
    STATELESS_ZERO,
    POS
};

struct Selector {
    uint16_t page = 0;
    uint16_t character = 0;
};

enum class NodeRole : uint8_t {
    DATA,
    PAGE_ACTIVATOR,
    CHARACTER_ACTIVATOR
};

class NativeNode {
public:
    virtual ~NativeNode() = default;
    virtual NodeRole role() const = 0;
    virtual const char* native_type() const = 0;
};

class PageActivatorNode final : public NativeNode {
    uint16_t page_;
public:
    explicit PageActivatorNode(uint16_t page):page_(page){}
    NodeRole role() const override { return NodeRole::PAGE_ACTIVATOR; }
    const char* native_type() const override { return "PAGE_ACTIVATOR_NODE"; }
    uint16_t page() const { return page_; }

    // Activators participate only when selected.
    Observation observe(const Selector& s) const {
        return s.page==page_ ? Observation::POS : Observation::NEITHER;
    }
};

class CharacterActivatorNode final : public NativeNode {
    uint16_t character_;
public:
    explicit CharacterActivatorNode(uint16_t character):character_(character){}
    NodeRole role() const override { return NodeRole::CHARACTER_ACTIVATOR; }
    const char* native_type() const override { return "CHARACTER_ACTIVATOR_NODE"; }
    uint16_t character() const { return character_; }

    // Activators participate only when selected.
    Observation observe(const Selector& s) const {
        return s.character==character_ ? Observation::POS : Observation::NEITHER;
    }
};

struct ConditionedOverlay {
    uint16_t page = 0;
    uint16_t symbol = 0;
};

class DataNode final : public NativeNode {
    uint32_t position_;
    std::vector<ConditionedOverlay> overlays_;
public:
    explicit DataNode(uint32_t position):position_(position){}

    NodeRole role() const override { return NodeRole::DATA; }
    const char* native_type() const override { return "TRUCOMPUTE_DATA_NODE"; }
    uint32_t position() const { return position_; }

    // Different page conditions coexist in this one native node.
    // Same condition + same symbol is idempotent.
    // Same condition + different symbol is the only contradiction.
    void imprint(uint16_t page,uint16_t symbol){
        for(const auto& x:overlays_){
            if(x.page!=page) continue;
            if(x.symbol==symbol) return;
            throw std::runtime_error("same-condition overlay contradiction");
        }
        overlays_.push_back({page,symbol});
    }

    const std::vector<ConditionedOverlay>& overlays() const { return overlays_; }

    uint16_t symbol_for(uint16_t page) const {
        for(const auto& x:overlays_) if(x.page==page) return x.symbol;
        throw std::runtime_error("selected page has no overlay in data node");
    }

    // Once page+character activators are validly selected, every data node
    // participates and resolves exactly TRUE(POS) or FALSE(NEG).
    Observation answer(const Selector& s) const {
        const uint16_t symbol=symbol_for(s.page);
        return symbol==s.character ? Observation::POS : Observation::NEG;
    }
};

struct QueryFrame {
    Selector selector{};
    std::vector<Observation> data_responses;
    uint32_t selected_page_activators = 0;
    uint32_t inactive_page_activators = 0;
    uint32_t selected_character_activators = 0;
    uint32_t inactive_character_activators = 0;
};

class Field {
    std::vector<std::unique_ptr<DataNode>> data_;
    std::vector<std::unique_ptr<PageActivatorNode>> pages_;
    std::vector<std::unique_ptr<CharacterActivatorNode>> characters_;

public:
    DataNode& add_data_node(uint32_t position){
        auto p=std::make_unique<DataNode>(position);
        DataNode& r=*p;
        data_.push_back(std::move(p));
        return r;
    }

    PageActivatorNode& add_page_activator(uint16_t page){
        auto p=std::make_unique<PageActivatorNode>(page);
        PageActivatorNode& r=*p;
        pages_.push_back(std::move(p));
        return r;
    }

    CharacterActivatorNode& add_character_activator(uint16_t c){
        auto p=std::make_unique<CharacterActivatorNode>(c);
        CharacterActivatorNode& r=*p;
        characters_.push_back(std::move(p));
        return r;
    }

    size_t data_node_count() const { return data_.size(); }
    size_t page_activator_count() const { return pages_.size(); }
    size_t character_activator_count() const { return characters_.size(); }

    DataNode& data_node(size_t i){ return *data_.at(i); }
    const DataNode& data_node(size_t i) const { return *data_.at(i); }

    QueryFrame query(uint16_t page,uint16_t character) const {
        Selector s{page,character};
        QueryFrame frame;
        frame.selector=s;

        for(const auto& a:pages_){
            const auto o=a->observe(s);
            if(o==Observation::POS) ++frame.selected_page_activators;
            else if(o==Observation::NEITHER) ++frame.inactive_page_activators;
            else throw std::runtime_error("page activator produced invalid state");
        }
        for(const auto& a:characters_){
            const auto o=a->observe(s);
            if(o==Observation::POS) ++frame.selected_character_activators;
            else if(o==Observation::NEITHER) ++frame.inactive_character_activators;
            else throw std::runtime_error("character activator produced invalid state");
        }

        if(frame.selected_page_activators!=1)
            throw std::runtime_error("query requires exactly one selected page activator");
        if(frame.selected_character_activators!=1)
            throw std::runtime_error("query requires exactly one selected character activator");

        frame.data_responses.reserve(data_.size());
        for(const auto& n:data_){
            const auto o=n->answer(s);
            if(o!=Observation::POS && o!=Observation::NEG)
                throw std::runtime_error("data node failed TRUE/FALSE participation law");
            frame.data_responses.push_back(o);
        }
        return frame;
    }
};

} // namespace trucompute_field_v6
