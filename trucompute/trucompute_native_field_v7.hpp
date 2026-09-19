#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace trucompute_field_v7 {

// NEITHER is intentionally not an active-field result.
// While powered ON, an active TruCompute resolution is always NEG, BOTH, or POS.
enum class ActiveObservation : uint8_t {
    NEG,
    BOTH, // +1 and -1 both participate; net zero
    POS
};

// NEITHER exists only at the machine power boundary.
enum class SwitchObservation : uint8_t {
    NEITHER,
    ON
};

struct Selector {
    uint16_t page = 0;
    uint16_t character = 0;
};

struct Contribution {
    int8_t polarity = 0; // must be -1 or +1

    static constexpr Contribution neg() { return {-1}; }
    static constexpr Contribution pos() { return {+1}; }
};

// Active operations cannot settle to NEITHER.
// An empty active operation is invalid rather than silently becoming zero.
inline ActiveObservation settle_active(const std::vector<Contribution>& xs) {
    if(xs.empty())
        throw std::runtime_error("active TruCompute operation requires participation");

    int sum=0;
    for(const auto& x:xs){
        if(x.polarity!=+1 && x.polarity!=-1)
            throw std::runtime_error("native contribution must be -1 or +1");
        sum+=x.polarity;
    }

    if(sum<0) return ActiveObservation::NEG;
    if(sum>0) return ActiveObservation::POS;
    return ActiveObservation::BOTH;
}

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

// Activators do not emit NEITHER when unselected.
// They either participate in the current selector operation or are not invoked.
class PageActivatorNode final : public NativeNode {
    uint16_t page_;
public:
    explicit PageActivatorNode(uint16_t page):page_(page){}
    NodeRole role() const override { return NodeRole::PAGE_ACTIVATOR; }
    const char* native_type() const override { return "PAGE_ACTIVATOR_NODE"; }
    uint16_t page() const { return page_; }
    bool selected(const Selector& s) const { return s.page==page_; }
};

class CharacterActivatorNode final : public NativeNode {
    uint16_t character_;
public:
    explicit CharacterActivatorNode(uint16_t character):character_(character){}
    NodeRole role() const override { return NodeRole::CHARACTER_ACTIVATOR; }
    const char* native_type() const override { return "CHARACTER_ACTIVATOR_NODE"; }
    uint16_t character() const { return character_; }
    bool selected(const Selector& s) const { return s.character==character_; }
};

struct ConditionedOverlay {
    uint16_t page = 0;
    uint16_t symbol = 0;
};

// A data node is always part of every valid ON query.
// Its conditioned overlays coexist inside this one native field node.
class DataNode final : public NativeNode {
    uint32_t position_;
    std::vector<ConditionedOverlay> overlays_;

public:
    explicit DataNode(uint32_t position):position_(position){}

    NodeRole role() const override { return NodeRole::DATA; }
    const char* native_type() const override { return "TRUCOMPUTE_DATA_NODE"; }
    uint32_t position() const { return position_; }

    void imprint(uint16_t page,uint16_t symbol){
        for(const auto& x:overlays_){
            if(x.page!=page) continue;
            if(x.symbol==symbol) return; // idempotent same conditioned write
            throw std::runtime_error("same-condition overlay contradiction");
        }
        overlays_.push_back({page,symbol});
    }

    const std::vector<ConditionedOverlay>& overlays() const { return overlays_; }

    uint16_t symbol_for(uint16_t page) const {
        for(const auto& x:overlays_)
            if(x.page==page) return x.symbol;
        throw std::runtime_error("selected page has no overlay in data node");
    }

    // Every data node participates in every valid selected query.
    // There is no NEITHER result here.
    ActiveObservation answer(const Selector& s) const {
        return symbol_for(s.page)==s.character
            ? ActiveObservation::POS
            : ActiveObservation::NEG;
    }
};

struct QueryFrame {
    Selector selector{};
    std::vector<ActiveObservation> data_responses;
    uint32_t selected_page_activators = 0;
    uint32_t unselected_page_activators = 0;
    uint32_t selected_character_activators = 0;
    uint32_t unselected_character_activators = 0;
};

class Field {
    bool powered_ = false;
    std::vector<std::unique_ptr<DataNode>> data_;
    std::vector<std::unique_ptr<PageActivatorNode>> pages_;
    std::vector<std::unique_ptr<CharacterActivatorNode>> characters_;

public:
    SwitchObservation switch_observation() const {
        return powered_ ? SwitchObservation::ON : SwitchObservation::NEITHER;
    }

    void power_on(){ powered_=true; }
    void power_off(){ powered_=false; }
    bool powered() const { return powered_; }

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

    CharacterActivatorNode& add_character_activator(uint16_t character){
        auto p=std::make_unique<CharacterActivatorNode>(character);
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
        if(!powered_)
            throw std::runtime_error("NEITHER: machine is OFF");

        Selector s{page,character};
        QueryFrame frame;
        frame.selector=s;

        for(const auto& a:pages_){
            if(a->selected(s)) ++frame.selected_page_activators;
            else ++frame.unselected_page_activators;
        }
        for(const auto& a:characters_){
            if(a->selected(s)) ++frame.selected_character_activators;
            else ++frame.unselected_character_activators;
        }

        if(frame.selected_page_activators!=1)
            throw std::runtime_error("query requires exactly one selected page activator");
        if(frame.selected_character_activators!=1)
            throw std::runtime_error("query requires exactly one selected character activator");

        frame.data_responses.reserve(data_.size());
        for(const auto& n:data_){
            // Every data node participates and must return only TRUE/FALSE.
            frame.data_responses.push_back(n->answer(s));
        }

        return frame;
    }
};

} // namespace trucompute_field_v7
