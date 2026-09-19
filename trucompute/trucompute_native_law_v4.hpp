#pragma once
#include <cstdint>
#include <initializer_list>
#include <stdexcept>

namespace trucompute_native {

// Observation is a RESULT of the environmental law.
// It is never retained inside a Node.
enum class Observation : uint8_t {
    NEITHER,
    NEG,
    STATELESS_ZERO,
    POS
};

struct Node {
    uint32_t id = 0;
    constexpr explicit Node(uint32_t id_=0) : id(id_) {}
};

// A selector changes the conditions under which the same substrate is observed.
// It is control state, not payload stored in a node.
struct Selector {
    uint16_t page = 0;
    uint16_t character = 0;
    bool page_active = false;
    bool character_active = false;
};

// A contribution is an event presented to the law for the current observation.
// Host encoding is irrelevant to the abstract machine; only these semantics matter.
struct Contribution {
    int8_t polarity = 0;   // -1 or +1
    bool participates = false;

    static constexpr Contribution absent() { return {0,false}; }
    static constexpr Contribution neg()    { return {-1,true}; }
    static constexpr Contribution pos()    { return {+1,true}; }
};

// TruCompute exists as an environmental resolution law.
// Nodes do not "have" NEG/POS/etc. The law produces those observations from
// participating contributions under the current selector condition.
struct Law {
    static Observation resolve(std::initializer_list<Contribution> xs) {
        bool any = false;
        int sum = 0;
        for (auto x : xs) {
            if (!x.participates) continue;
            any = true;
            if (x.polarity != -1 && x.polarity != +1)
                throw std::runtime_error("native contribution polarity must be -1 or +1");
            sum += x.polarity;
        }
        if (!any) return Observation::NEITHER;
        if (sum < 0) return Observation::NEG;
        if (sum > 0) return Observation::POS;
        return Observation::STATELESS_ZERO;
    }

    static constexpr bool participated(Observation x) {
        return x != Observation::NEITHER;
    }
    static constexpr bool resolved(Observation x) {
        return x == Observation::NEG || x == Observation::POS;
    }
    static constexpr int net(Observation x) {
        return x == Observation::NEG ? -1 :
               x == Observation::POS ? +1 :
               x == Observation::STATELESS_ZERO ? 0 :
               2; // sentinel: NEITHER has no numeric value
    }
};

// Generic selector-conditioned participation interface.
// Concrete CSS environments implement this without putting state inside Node.
class Environment {
public:
    virtual ~Environment() = default;

    virtual Contribution contribution(
        Node node,
        const Selector& selector,
        uint32_t relation_slot
    ) const = 0;

    Observation observe(
        Node node,
        const Selector& selector,
        std::initializer_list<uint32_t> relation_slots
    ) const {
        bool any = false;
        int sum = 0;
        for (auto slot : relation_slots) {
            Contribution c = contribution(node, selector, slot);
            if (!c.participates) continue;
            any = true;
            if (c.polarity != -1 && c.polarity != +1)
                throw std::runtime_error("invalid environment contribution");
            sum += c.polarity;
        }
        if (!any) return Observation::NEITHER;
        if (sum < 0) return Observation::NEG;
        if (sum > 0) return Observation::POS;
        return Observation::STATELESS_ZERO;
    }
};

} // namespace trucompute_native
