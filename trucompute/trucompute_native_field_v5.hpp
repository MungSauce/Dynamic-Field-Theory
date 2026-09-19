#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace trucompute_field {

// These are observations of a field interaction, not encoded node values.
enum class Observation : uint8_t {
    NEITHER,
    NEG,
    STATELESS_ZERO,
    POS
};

struct Selector {
    uint16_t page = 0;
    uint16_t character = 0;
    bool page_active = false;
    bool character_active = false;
};

struct Contribution {
    int value = 0;
    bool participates = false;

    static constexpr Contribution none() { return {0,false}; }
    static constexpr Contribution neg()  { return {-1,true}; }
    static constexpr Contribution pos()  { return {+1,true}; }
};

inline Observation settle(const std::vector<Contribution>& xs) {
    bool any=false;
    int sum=0;
    for(const auto& x:xs){
        if(!x.participates) continue;
        any=true;
        if(x.value!=+1 && x.value!=-1)
            throw std::runtime_error("TruCompute native node emitted non-native contribution");
        sum+=x.value;
    }
    if(!any) return Observation::NEITHER;
    if(sum<0) return Observation::NEG;
    if(sum>0) return Observation::POS;
    return Observation::STATELESS_ZERO;
}

// Native node interface. A field is literally made of these.
// The node decides whether/how it participates under the current selector.
class Node {
public:
    virtual ~Node() = default;
    virtual Contribution participate(const Selector&) const = 0;
    virtual const char* native_type() const = 0;
};

class PositiveNode final : public Node {
public:
    Contribution participate(const Selector&) const override {
        return Contribution::pos();
    }
    const char* native_type() const override { return "POSITIVE_NODE"; }
};

class NegativeNode final : public Node {
public:
    Contribution participate(const Selector&) const override {
        return Contribution::neg();
    }
    const char* native_type() const override { return "NEGATIVE_NODE"; }
};

// Both polarities are genuinely present when this node participates.
class BalancedNode final : public Node {
public:
    Contribution participate(const Selector&) const override {
        // A balanced node is handled through its native pair expansion.
        // Direct scalar participation is intentionally invalid so callers
        // cannot collapse it into a fake numeric/binary node.
        throw std::runtime_error("BalancedNode must expand as two native contributions");
    }
    std::vector<Contribution> expand(const Selector&) const {
        return {Contribution::neg(),Contribution::pos()};
    }
    const char* native_type() const override { return "BALANCED_NODE"; }
};

class NeitherNode final : public Node {
public:
    Contribution participate(const Selector&) const override {
        return Contribution::none();
    }
    const char* native_type() const override { return "NEITHER_NODE"; }
};

// Selector-sensitive carrier. Its type is native; it is not a generic
// state-holding cell. Page/character selectors determine participation,
// analogous to filtering a colour already present in the field.
class FilteredPositiveNode final : public Node {
    uint16_t page_;
    uint16_t character_;
    bool use_character_;
public:
    explicit FilteredPositiveNode(uint16_t page)
        : page_(page),character_(0),use_character_(false){}
    FilteredPositiveNode(uint16_t page,uint16_t character)
        : page_(page),character_(character),use_character_(true){}

    Contribution participate(const Selector& s) const override {
        if(!s.page_active || s.page!=page_) return Contribution::none();
        if(use_character_ && (!s.character_active || s.character!=character_))
            return Contribution::none();
        return Contribution::pos();
    }
    const char* native_type() const override { return "FILTERED_POSITIVE_NODE"; }
};

class FilteredNegativeNode final : public Node {
    uint16_t page_;
    uint16_t character_;
    bool use_character_;
public:
    explicit FilteredNegativeNode(uint16_t page)
        : page_(page),character_(0),use_character_(false){}
    FilteredNegativeNode(uint16_t page,uint16_t character)
        : page_(page),character_(character),use_character_(true){}

    Contribution participate(const Selector& s) const override {
        if(!s.page_active || s.page!=page_) return Contribution::none();
        if(use_character_ && (!s.character_active || s.character!=character_))
            return Contribution::none();
        return Contribution::neg();
    }
    const char* native_type() const override { return "FILTERED_NEGATIVE_NODE"; }
};

// A native field is exactly a population of native nodes.
// There is no separate environment object holding node state.
class Field {
    std::vector<std::unique_ptr<Node>> nodes_;
public:
    template<class T,class...Args>
    T& emplace(Args&&...args){
        auto p=std::make_unique<T>(std::forward<Args>(args)...);
        T& ref=*p;
        nodes_.push_back(std::move(p));
        return ref;
    }

    size_t size() const { return nodes_.size(); }

    Observation observe(const Selector& s) const {
        std::vector<Contribution> xs;
        xs.reserve(nodes_.size()*2);
        for(const auto& n:nodes_){
            if(auto* b=dynamic_cast<const BalancedNode*>(n.get())){
                auto pair=b->expand(s);
                xs.insert(xs.end(),pair.begin(),pair.end());
            }else{
                xs.push_back(n->participate(s));
            }
        }
        return settle(xs);
    }

    const Node& at(size_t i) const { return *nodes_.at(i); }
};

} // namespace trucompute_field
