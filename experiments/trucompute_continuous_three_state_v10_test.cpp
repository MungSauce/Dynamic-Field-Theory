#include "../trucompute/trucompute_continuous_three_state_v10.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace trucompute_continuous_v10;

static constexpr uint16_t PAGES=4;
static constexpr uint16_t CHARS=206;
static constexpr uint32_t POSITIONS=64;

static void must(bool x,const char* m){
    if(!x) throw std::runtime_error(m);
}

static Field make_field(){
    Field f;
    for(uint32_t i=0;i<POSITIONS;++i) f.add_data_node(i);
    for(uint16_t q=0;q<PAGES;++q) f.add_page_activator(q);
    for(uint16_t c=0;c<CHARS;++c) f.add_character_activator(c);

    for(uint32_t pos=0;pos<POSITIONS;++pos)
        for(uint16_t q=0;q<PAGES;++q)
            f.data_node(pos).imprint(q,uint16_t((q*37+pos*11+3)%CHARS));

    return f;
}

static std::vector<uint8_t> reconstruct_page(Field& f,uint16_t q){
    std::vector<uint16_t> yes(POSITIONS,0);
    std::vector<int16_t> winner(POSITIONS,-1);

    for(uint16_t c=0;c<CHARS;++c){
        auto frame=f.set_probe(q,c);
        must(frame.responses.size()==POSITIONS,"response width");
        must(frame.selected_page_activators==1,"page selector");
        must(frame.selected_character_activators==1,"character selector");

        for(uint32_t pos=0;pos<POSITIONS;++pos){
            auto r=frame.responses[pos];
            must(r==NativeCondition::POS || r==NativeCondition::NEG,
                 "active response must be +/- only");
            if(r==NativeCondition::POS){
                ++yes[pos];
                winner[pos]=int16_t(c);
            }
        }

        must(f.all_substrates_both(),"probe mutated BOTH substrate");
    }

    std::vector<uint8_t> out(POSITIONS);
    for(uint32_t pos=0;pos<POSITIONS;++pos){
        must(yes[pos]==1,"exactly one TRUE letter required");
        out[pos]=uint8_t(winner[pos]);
    }
    return out;
}

int main(){
    Field f=make_field();

    // There is no OFF/NEITHER machine lifecycle.
    must(!f.has_probe(),"field may begin unprobed but it is not OFF");
    must(f.transition_count()==0,"initial transition count");
    must(f.all_substrates_both(),"native field substrate must exist as BOTH");

    const auto symbol00=f.data_node(0).symbol_for(0);
    const auto symbol10=f.data_node(0).symbol_for(1);
    must(symbol00!=symbol10,"fixture should have different page conditions");

    // First observation.
    auto a=f.set_probe(0,symbol00);
    must(a.responses[0]==NativeCondition::POS,"first probe true");

    // Direct selector-to-selector transition. No reset step.
    auto b=f.set_probe(0,uint16_t((symbol00+1)%CHARS));
    must(b.responses[0]==NativeCondition::NEG,"same page different letter false");

    auto c=f.set_probe(1,symbol10);
    must(c.responses[0]==NativeCondition::POS,"different page true");

    auto d=f.set_probe(1,symbol00);
    must(d.responses[0]==NativeCondition::NEG,"different page old letter false");

    must(f.transition_count()==4,"selector transitions should be continuous");
    must(f.all_substrates_both(),"direct transitions mutated substrate");

    // Exhaustive page/character sweep with no reset between any probes.
    auto p0=reconstruct_page(f,0);
    auto p1=reconstruct_page(f,1);
    auto p2=reconstruct_page(f,2);
    auto p3=reconstruct_page(f,3);

    must(reconstruct_page(f,2)==p2,"page2 revisit");
    must(reconstruct_page(f,0)==p0,"page0 revisit");
    must(reconstruct_page(f,3)==p3,"page3 revisit");
    must(reconstruct_page(f,1)==p1,"page1 revisit");

    must(f.all_substrates_both(),"full sweep mutated substrate");

    std::cout<<"TRUCOMPUTE_CONTINUOUS_THREE_STATE_V10=PASS\n";
    std::cout<<"machine_off_state=false\n";
    std::cout<<"neither_state=false\n";
    std::cout<<"native_domain=NEG_BOTH_POS\n";
    std::cout<<"native_substrate=BOTH\n";
    std::cout<<"active_resolution=NEG_OR_POS\n";
    std::cout<<"selector_transition_requires_reset=false\n";
    std::cout<<"selector_transition_passes_through_zero=false\n";
    std::cout<<"data_nodes_continuously_present=PASS\n";
    std::cout<<"substrate_mutated_by_observation=false\n";
    std::cout<<"selector_revisit=PASS\n";
    std::cout<<"exact_one_true_letter_per_page_position=PASS\n";
    std::cout<<"transition_count="<<f.transition_count()<<"\n";
    return 0;
}
