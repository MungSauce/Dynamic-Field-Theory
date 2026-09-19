#include "trucompute_balanced_runtime_v14.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace trucompute::balanced_v14;

static void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

static void selftest() {
    BalancedEnvironment env(8);

    for (std::size_t i = 0; i < env.size(); ++i) {
        require(env.natural_state(i) == State::NATURAL_ZERO,
                "fresh cell must be (+1,-1) NATURAL_ZERO");
        require(net(env.natural_state(i)) == 0,
                "natural state must have numeric net zero");
    }

    ObservationFrame frame(1);
    require(frame.state(0) == State::NATURAL_ZERO, "frame starts balanced");

    frame.suppress_negative(0);
    require(frame.state(0) == State::POS,
            "removing -1 from (+1,-1) must expose +1");

    frame.release(0);
    require(frame.state(0) == State::NATURAL_ZERO,
            "release must restore natural balanced zero");

    frame.suppress_positive(0);
    require(frame.state(0) == State::NEG,
            "removing +1 from (+1,-1) must expose -1");

    frame.suppress_negative(0);
    require(frame.state(0) == State::NEITHER,
            "removing both rails must yield NEITHER/OFF");

    frame.release_all();
    require(frame.state(0) == State::NATURAL_ZERO,
            "release_all must restore (+1,-1)");

    env.connect(0, 1, Relation::SAME);
    env.connect(1, 2, Relation::OPPOSITE);
    env.connect(2, 3, Relation::SAME);

    auto plus = env.observe_from(0, State::POS);
    require(plus.consistent, "consistent relation chain rejected");
    require(plus.frame.state(0) == State::POS, "held reference not POS");
    require(plus.frame.state(1) == State::POS, "SAME relation failed");
    require(plus.frame.state(2) == State::NEG, "OPPOSITE relation failed");
    require(plus.frame.state(3) == State::NEG, "propagated SAME relation failed");
    require(plus.frame.state(4) == State::NATURAL_ZERO,
            "unobserved node must remain natural zero");

    auto minus = env.observe_from(0, State::NEG);
    require(minus.consistent, "opposite reference should remain consistent");
    require(minus.frame.state(0) == State::NEG, "held reference not NEG");
    require(minus.frame.state(1) == State::NEG, "SAME relation failed under NEG");
    require(minus.frame.state(2) == State::POS, "OPPOSITE relation failed under NEG");
    require(minus.frame.state(3) == State::POS, "propagated SAME failed under NEG");

    for (std::size_t i = 0; i < env.size(); ++i) {
        require(env.natural_state(i) == State::NATURAL_ZERO,
                "observation mutated the underlying balanced substrate");
    }

    BalancedEnvironment contradiction(3);
    contradiction.connect(0, 1, Relation::SAME);
    contradiction.connect(1, 2, Relation::SAME);
    contradiction.connect(0, 2, Relation::OPPOSITE);
    auto bad = contradiction.observe_from(0, State::POS);
    require(!bad.consistent, "contradictory cycle must be detected");

    std::cout << "TRUCOMPUTE_BALANCED_ZERO_CONFORMANCE=PASS\n";
    std::cout << "natural_state=NATURAL_ZERO(+1,-1)\n";
    std::cout << "compute_primitive=SUPPRESS_RAIL\n";
    std::cout << "observation=REFERENCE_CONDITIONED_NONDESTRUCTIVE_PROJECTION\n";
    std::cout << "release=RESTORE_NATURAL_ZERO\n";
    std::cout << "contradiction_detection=PASS\n";
}

static void demo() {
    BalancedEnvironment env(4);
    env.connect(0, 1, Relation::SAME);
    env.connect(1, 2, Relation::OPPOSITE);
    env.connect(2, 3, Relation::SAME);

    auto p = env.observe_from(0, State::POS);
    std::cout << "hold node0=POS: ";
    for (std::size_t i = 0; i < env.size(); ++i) {
        std::cout << state_name(p.frame.state(i));
        if (i + 1 != env.size()) std::cout << ' ';
    }
    std::cout << "\n";

    auto n = env.observe_from(0, State::NEG);
    std::cout << "hold node0=NEG: ";
    for (std::size_t i = 0; i < env.size(); ++i) {
        std::cout << state_name(n.frame.state(i));
        if (i + 1 != env.size()) std::cout << ' ';
    }
    std::cout << "\n";

    std::cout << "released substrate: ";
    for (std::size_t i = 0; i < env.size(); ++i) {
        std::cout << state_name(env.natural_state(i));
        if (i + 1 != env.size()) std::cout << ' ';
    }
    std::cout << "\n";
}

int main(int argc, char** argv) {
    if (argc == 1 || std::string(argv[1]) == "selftest") {
        selftest();
        return 0;
    }
    if (std::string(argv[1]) == "demo") {
        demo();
        return 0;
    }

    std::cerr << "usage: " << argv[0] << " [selftest|demo]\n";
    return 2;
}
