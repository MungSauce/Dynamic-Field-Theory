#include "../trucompute/trucompute_four_action_v11.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace trucompute_four_action_v11;

static void must(bool x,const char* m){ if(!x) throw std::runtime_error(m); }

int main() {
    // Host encoding proof: four native actions, two host bits.
    must(encode_host_2bit(Action::NEITHER)==0b00,"NEITHER code");
    must(encode_host_2bit(Action::NEG)==0b01,"NEG code");
    must(encode_host_2bit(Action::BOTH)==0b10,"BOTH code");
    must(encode_host_2bit(Action::POS)==0b11,"POS code");
    for(uint8_t i=0;i<4;++i)
        must(encode_host_2bit(decode_host_2bit(i))==i,"2-bit roundtrip");

    constexpr size_t CHANNELS=4;
    FourActionResistorNode<CHANNELS> node;
    std::array<uint16_t,CHANNELS> imprint{};
    imprint[0]=42; // page 0 resonance = 42 under reference transfer law
    node.set_imprint(imprint);

    const auto before=node.imprint();

    // Same node, all four native actions.
    must(node.act(false)==Action::NEITHER,"OFF node must act NEITHER");
    must(node.act(true)==Action::BOTH,"powered unobserved node must act BOTH");
    must(node.act(true,Probe{0,41})==Action::NEG,"nonmatching observation must act NEG");
    must(node.act(true,Probe{0,42})==Action::POS,"matching observation must act POS");

    // Observation changes action, not imprint.
    must(node.imprint()==before,"four-action transitions mutated imprint");

    Field<CHANNELS> field(8);
    for(size_t i=0;i<field.node_count();++i){
        std::array<uint16_t,CHANNELS> v{};
        v[0]=uint16_t(40+i);
        field.node(i).set_imprint(v);
    }

    // OFF -> all NEITHER.
    auto off=field.unresolved_actions();
    for(auto a:off) must(a==Action::NEITHER,"field OFF action");

    field.power_on();

    // ON with no observation -> all BOTH.
    auto unresolved=field.unresolved_actions();
    for(auto a:unresolved) must(a==Action::BOTH,"powered unresolved action");

    // ON with observation -> every node exactly NEG or POS.
    auto responses=field.observe(0,42);
    size_t pos=0,neg=0;
    for(auto a:responses){
        if(a==Action::POS) ++pos;
        else if(a==Action::NEG) ++neg;
        else throw std::runtime_error("observed node emitted NEITHER/BOTH");
    }
    must(pos==1 && neg==7,"expected one matching resistor at character 42");

    // Release the probe -> same nodes return to BOTH, no mutation.
    auto released=field.unresolved_actions();
    for(auto a:released) must(a==Action::BOTH,"probe release must return BOTH");

    field.power_off();
    auto final_off=field.unresolved_actions();
    for(auto a:final_off) must(a==Action::NEITHER,"final OFF must be NEITHER");

    std::cout<<"TRUCOMPUTE_FOUR_ACTION_NODE_V11=PASS\n";
    std::cout<<"native_action_count=4\n";
    std::cout<<"host_encoding_bits=2\n";
    std::cout<<"action_00=NEITHER\n";
    std::cout<<"action_01=NEG\n";
    std::cout<<"action_10=BOTH\n";
    std::cout<<"action_11=POS\n";
    std::cout<<"off_action=NEITHER\n";
    std::cout<<"powered_unobserved_action=BOTH\n";
    std::cout<<"observed_actions=NEG_OR_POS\n";
    std::cout<<"observation_mutates_imprint=false\n";
    std::cout<<"page_response_table=false\n";
    std::cout<<"character_response_table=false\n";
    return 0;
}
