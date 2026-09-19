#include "trucompute_temporal_freeze_v15.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace trucompute::temporal_v15;

static void require(bool ok, const std::string& msg) {
    if (!ok) {
        std::cerr << "FAIL: " << msg << '\n';
        std::exit(1);
    }
}

int main() {
    Environment env(5);
    Frame frame = env.new_frame();

    for (std::size_t i = 0; i < frame.size(); ++i) {
        require(frame.state(i) == State::ACTIVE_ZERO,
                "fresh cell is not live balanced zero");
        require(active_net(frame.state(i)) == 0,
                "live balanced zero is not numeric zero");
    }

    frame.resolve(0, State::POS);
    frame.freeze(0);
    require(frame.state(0) == State::FROZEN_ZERO,
            "freeze did not emit 00");
    require(frame.retained_active_state(0) == State::POS,
            "00 did not preserve prior POS");
    frame.thaw(0);
    require(frame.state(0) == State::POS,
            "thaw did not restore frozen POS");
    frame.rebalance(0);
    require(frame.state(0) == State::ACTIVE_ZERO,
            "rebalance did not restore live zero");

    env.connect(0, 1, Relation::SAME);
    env.connect(1, 2, Relation::SAME);
    env.connect(2, 3, Relation::OPPOSITE);

    auto first = env.propagate(frame, 0, State::POS);
    require(first.consistent, "initial propagation inconsistent");
    require(frame.state(0) == State::POS &&
            frame.state(1) == State::POS &&
            frame.state(2) == State::POS &&
            frame.state(3) == State::NEG,
            "initial relation projection wrong");

    // Freeze node 1 while POS. Visible machine code becomes 00 while its
    // previously active condition remains retained in time.
    frame.freeze(1);
    require(frame.state(1) == State::FROZEN_ZERO,
            "frozen node not encoded 00");
    require(frame.retained_active_state(1) == State::POS,
            "frozen condition changed");

    // Rebalance still-powered nodes and drive from the far side.
    // The frozen node is an insulation boundary, so collapse cannot cross it.
    frame.rebalance(0);
    frame.rebalance(2);
    frame.rebalance(3);
    auto second = env.propagate(frame, 3, State::POS);
    require(second.consistent, "post-freeze propagation inconsistent");
    require(frame.state(3) == State::POS, "new reference not POS");
    require(frame.state(2) == State::NEG, "opposite relation from node3 failed");
    require(frame.state(1) == State::FROZEN_ZERO, "00 barrier was modified");
    require(frame.state(0) == State::ACTIVE_ZERO,
            "collapse crossed 00 insulation boundary");

    frame.thaw(1);
    require(frame.state(1) == State::POS,
            "thaw did not resume frozen-in-time POS");

    bool blocked = false;
    frame.freeze(4);
    try {
        frame.resolve(4, State::NEG);
    } catch (const std::runtime_error&) {
        blocked = true;
    }
    require(blocked, "frozen cell evolved while unpowered");

    std::cout << "TRUCOMPUTE_TEMPORAL_FREEZE_CONFORMANCE=PASS\n";
    std::cout << "11=ACTIVE_ZERO(+1,-1)_POWERED\n";
    std::cout << "10=POS_POWERED\n";
    std::cout << "01=NEG_POWERED\n";
    std::cout << "00=FROZEN_IN_TIME_UNPOWERED\n";
    std::cout << "freeze_preserves_previous_active_condition=PASS\n";
    std::cout << "frozen_cell_cannot_evolve=PASS\n";
    std::cout << "00_insulates_relation_propagation=PASS\n";
    return 0;
}
