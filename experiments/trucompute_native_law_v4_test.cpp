#include "../trucompute/trucompute_native_law_v4.hpp"
#include <iostream>
#include <type_traits>
using namespace trucompute_native;

struct DemoEnvironment final : Environment {
    Contribution contribution(Node n,const Selector&s,uint32_t slot) const override {
        if(!s.page_active) return Contribution::absent();
        if(slot==0) return ((n.id + s.page)&1u) ? Contribution::pos() : Contribution::neg();
        if(slot==1) return ((n.id + s.page)&1u) ? Contribution::neg() : Contribution::pos();
        return Contribution::absent();
    }
};

int main(){
    static_assert(std::is_trivially_copyable<Node>::value);
    static_assert(sizeof(Node)==sizeof(uint32_t));

    DemoEnvironment env;
    Node n{7};
    Selector off{};
    if(env.observe(n,off,{0})!=Observation::NEITHER) return 1;

    Selector p0{0,0,true,false};
    auto a=env.observe(n,p0,{0});
    if(!Law::resolved(a)) return 2;

    auto z=env.observe(n,p0,{0,1});
    if(z!=Observation::STATELESS_ZERO) return 3;

    Selector p1{1,0,true,false};
    auto b=env.observe(n,p1,{0});
    if(!Law::resolved(b) || b==a) return 4;

    // Node identity did not change when selector changed.
    if(n.id!=7) return 5;

    std::cout<<"TRUCOMPUTE_NATIVE_LAW_V4=PASS\n";
    std::cout<<"node_retained_state_bytes=0\n";
    std::cout<<"node_identity_bytes="<<sizeof(Node)<<"\n";
    std::cout<<"observation_is_law_result=true\n";
    std::cout<<"selector_changes_observation_without_node_mutation=true\n";
    std::cout<<"stateless_zero_is_participating_result=true\n";
    std::cout<<"neither_is_no_participation=true\n";
    return 0;
}
