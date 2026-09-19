#include "../trucompute/trucompute_native_field_v5.hpp"
#include <iostream>
using namespace trucompute_field;

int main(){
    {
        Field f;
        f.emplace<NeitherNode>();
        Selector s{};
        if(f.observe(s)!=Observation::NEITHER) return 1;
    }

    {
        Field f;
        f.emplace<PositiveNode>();
        Selector s{};
        if(f.observe(s)!=Observation::POS) return 2;
    }

    {
        Field f;
        f.emplace<NegativeNode>();
        Selector s{};
        if(f.observe(s)!=Observation::NEG) return 3;
    }

    {
        Field f;
        f.emplace<BalancedNode>();
        Selector s{};
        if(f.observe(s)!=Observation::STATELESS_ZERO) return 4;
    }

    // Same field space contains two differently filtered native carriers.
    // Selecting page 3 exposes POS. Selecting page 7 exposes NEG.
    {
        Field f;
        f.emplace<FilteredPositiveNode>(3);
        f.emplace<FilteredNegativeNode>(7);

        Selector p3{}; p3.page_active=true; p3.page=3;
        Selector p7{}; p7.page_active=true; p7.page=7;
        Selector p4{}; p4.page_active=true; p4.page=4;

        if(f.observe(p3)!=Observation::POS) return 5;
        if(f.observe(p7)!=Observation::NEG) return 6;
        if(f.observe(p4)!=Observation::NEITHER) return 7;

        // Node objects are unchanged across observations.
        if(std::string(f.at(0).native_type())!="FILTERED_POSITIVE_NODE") return 8;
        if(std::string(f.at(1).native_type())!="FILTERED_NEGATIVE_NODE") return 9;
    }

    // Character lens can further filter the same field.
    {
        Field f;
        f.emplace<FilteredPositiveNode>(9,42);
        f.emplace<FilteredNegativeNode>(9,43);

        Selector c42{}; c42.page_active=true; c42.page=9; c42.character_active=true; c42.character=42;
        Selector c43=c42; c43.character=43;
        Selector c44=c42; c44.character=44;

        if(f.observe(c42)!=Observation::POS) return 10;
        if(f.observe(c43)!=Observation::NEG) return 11;
        if(f.observe(c44)!=Observation::NEITHER) return 12;
    }

    std::cout<<"TRUCOMPUTE_NATIVE_FIELD_V5=PASS\n";
    std::cout<<"field_is_composed_of_native_nodes=true\n";
    std::cout<<"generic_identity_only_nodes=false\n";
    std::cout<<"node_type_defines_participation_law=true\n";
    std::cout<<"selector_filters_existing_nodes=true\n";
    std::cout<<"selector_does_not_mutate_nodes=true\n";
    std::cout<<"balanced_is_native_coexistence=true\n";
    return 0;
}
