#include "../trucompute/trucompute_native_field_v7.hpp"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace trucompute_field_v7;

static constexpr uint16_t CHARACTER_COUNT=206;
static constexpr uint16_t PAGE_COUNT=4;
static constexpr uint32_t POSITION_COUNT=64;

static void must(bool x,const char* msg){
    if(!x) throw std::runtime_error(msg);
}

#pragma pack(push,1)
struct Header {
    char magic[8];
    uint16_t version;
    uint16_t pages;
    uint16_t characters;
    uint16_t reserved;
    uint32_t positions;
    uint64_t source_length;
};
#pragma pack(pop)

static Field make_empty_field(){
    Field f;
    for(uint32_t i=0;i<POSITION_COUNT;++i) f.add_data_node(i);
    for(uint16_t q=0;q<PAGE_COUNT;++q) f.add_page_activator(q);
    for(uint16_t c=0;c<CHARACTER_COUNT;++c) f.add_character_activator(c);
    return f;
}

static void populate_fixture(Field& field){
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
        for(uint16_t q=0;q<PAGE_COUNT;++q){
            uint16_t symbol=uint16_t((q*37 + pos*11 + 3) % CHARACTER_COUNT);
            field.data_node(pos).imprint(q,symbol);
        }
    }
}

static void validate_query_law(const Field& field,uint16_t q,uint16_t c){
    auto frame=field.query(q,c);
    must(frame.data_responses.size()==POSITION_COUNT,"every data node must answer");
    must(frame.selected_page_activators==1,"exactly one page activator selected");
    must(frame.unselected_page_activators==PAGE_COUNT-1,"other page activators must remain unselected");
    must(frame.selected_character_activators==1,"exactly one character activator selected");
    must(frame.unselected_character_activators==CHARACTER_COUNT-1,"other character activators must remain unselected");

    for(auto o:frame.data_responses)
        must(o==ActiveObservation::POS || o==ActiveObservation::NEG,
             "data node produced non-boolean active result");
}

static std::vector<uint8_t> reconstruct_page(const Field& field,uint16_t q){
    std::vector<int16_t> winner(POSITION_COUNT,-1);
    std::vector<uint16_t> yes(POSITION_COUNT,0);

    for(uint16_t c=0;c<CHARACTER_COUNT;++c){
        auto frame=field.query(q,c);
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
            const auto o=frame.data_responses[pos];
            must(o==ActiveObservation::POS || o==ActiveObservation::NEG,
                 "active data node must be TRUE/FALSE");
            if(o==ActiveObservation::POS){
                ++yes[pos];
                winner[pos]=int16_t(c);
            }
        }
    }

    std::vector<uint8_t> out(POSITION_COUNT);
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
        must(yes[pos]==1,"position must have exactly one TRUE");
        must(winner[pos]>=0,"position missing TRUE");
        out[pos]=uint8_t(winner[pos]);
    }
    return out;
}

static int selftest(){
    // NEITHER exists only at the power switch.
    Field field=make_empty_field();
    must(field.switch_observation()==SwitchObservation::NEITHER,"OFF must be NEITHER");

    bool off_query_rejected=false;
    try{ (void)field.query(0,0); }
    catch(const std::runtime_error&){ off_query_rejected=true; }
    must(off_query_rejected,"OFF machine accepted active query");

    field.power_on();
    must(field.switch_observation()==SwitchObservation::ON,"power ON state");

    // Every active zero is BOTH, never NEITHER.
    must(settle_active({Contribution::neg(),Contribution::pos()})==ActiveObservation::BOTH,
         "active zero must be BOTH");
    must(settle_active({Contribution::neg(),Contribution::pos(),
                        Contribution::neg(),Contribution::pos()})==ActiveObservation::BOTH,
         "all active zero sums must be BOTH");

    bool empty_active_rejected=false;
    try{ (void)settle_active({}); }
    catch(const std::runtime_error&){ empty_active_rejected=true; }
    must(empty_active_rejected,"empty active operation must not become zero");

    populate_fixture(field);

    // Explicit incompatible-looking overlays coexist in the same data node.
    field.data_node(7).imprint(0,field.data_node(7).symbol_for(0)); // idempotence
    must(field.data_node(7).symbol_for(0)!=field.data_node(7).symbol_for(1),
         "fixture should demonstrate different conditioned symbols");

    bool same_condition_conflict=false;
    try{
        const uint16_t existing=field.data_node(7).symbol_for(1);
        field.data_node(7).imprint(1,uint16_t((existing+1)%CHARACTER_COUNT));
    }catch(const std::runtime_error&){
        same_condition_conflict=true;
    }
    must(same_condition_conflict,"same-condition contradiction not detected");

    for(uint16_t q=0;q<PAGE_COUNT;++q)
        for(uint16_t c: {uint16_t(0),uint16_t(17),uint16_t(42),uint16_t(99),uint16_t(205)})
            validate_query_law(field,q,c);

    const auto p0=reconstruct_page(field,0);
    const auto p1=reconstruct_page(field,1);
    const auto p2=reconstruct_page(field,2);
    const auto p3=reconstruct_page(field,3);

    // Revisit selectors out of order. Same field; no mutation.
    must(reconstruct_page(field,1)==p1,"page1 selector revisit");
    must(reconstruct_page(field,0)==p0,"page0 selector revisit");
    must(reconstruct_page(field,3)==p3,"page3 selector revisit");
    must(reconstruct_page(field,2)==p2,"page2 selector revisit");

    field.power_off();
    must(field.switch_observation()==SwitchObservation::NEITHER,"OFF must return to NEITHER");

    std::cout<<"TRUCOMPUTE_OVERLAY_V3_SELFTEST=PASS\n";
    std::cout<<"neither_only_at_power_switch=PASS\n";
    std::cout<<"active_zero_is_both=PASS\n";
    std::cout<<"empty_active_operation_rejected=PASS\n";
    std::cout<<"data_nodes_always_true_or_false=PASS\n";
    std::cout<<"unselected_activators_emit_no_state=PASS\n";
    std::cout<<"same_node_multi_condition_coexistence=PASS\n";
    std::cout<<"same_condition_conflict_detected=PASS\n";
    std::cout<<"selector_switch_reversible=PASS\n";
    std::cout<<"exact_one_true_per_position=PASS\n";
    return 0;
}

static int freeze_cmd(const std::string& source,const std::string& artifact){
    std::ifstream in(source,std::ios::binary);
    must(bool(in),"open source");
    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    must(src.size()==uint64_t(PAGE_COUNT)*POSITION_COUNT,"fixture source length");
    for(uint8_t b:src) must(b<CHARACTER_COUNT,"fixture symbol outside 206");

    Field field=make_empty_field();
    field.power_on();

    for(uint16_t q=0;q<PAGE_COUNT;++q)
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            field.data_node(pos).imprint(q,src[uint64_t(q)*POSITION_COUNT+pos]);

    for(uint16_t q=0;q<PAGE_COUNT;++q){
        const auto page=reconstruct_page(field,q);
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            must(page[pos]==src[uint64_t(q)*POSITION_COUNT+pos],
                 "prefreeze exact replay mismatch");
    }

    Header h{};
    std::memcpy(h.magic,"TCOVL3A",7);
    h.version=3;
    h.pages=PAGE_COUNT;
    h.characters=CHARACTER_COUNT;
    h.positions=POSITION_COUNT;
    h.source_length=src.size();

    std::ofstream out(artifact,std::ios::binary|std::ios::trunc);
    must(bool(out),"open artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));

    // Conformance-only frozen description. This intentionally records each
    // conditioned symbol so a fresh process can reconstruct the same native
    // field. It is NOT a compression artifact or size claim.
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
        for(uint16_t q=0;q<PAGE_COUNT;++q)
            out.put(char(uint8_t(field.data_node(pos).symbol_for(q))));

    must(bool(out),"write artifact");
    out.close();
    field.power_off();

    std::cout<<"status=CONDITIONED_FIELD_FROZEN\n";
    std::cout<<"source_bytes="<<src.size()<<"\n";
    std::cout<<"data_nodes="<<POSITION_COUNT<<"\n";
    std::cout<<"page_activators="<<PAGE_COUNT<<"\n";
    std::cout<<"character_activators="<<CHARACTER_COUNT<<"\n";
    std::cout<<"conditioned_overlays="<<(uint64_t(PAGE_COUNT)*POSITION_COUNT)<<"\n";
    std::cout<<"prefreeze_exact_replay=PASS\n";
    std::cout<<"artifact_purpose=CONFORMANCE_ONLY_NOT_COMPRESSION\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int replay_cmd(const std::string& artifact,const std::string& recovered_path){
    std::ifstream in(artifact,std::ios::binary);
    must(bool(in),"open artifact");

    Header h{};
    in.read(reinterpret_cast<char*>(&h),sizeof(h));
    must(bool(in),"read header");
    must(std::string(h.magic,7)=="TCOVL3A","magic");
    must(h.version==3,"version");
    must(h.pages==PAGE_COUNT && h.characters==CHARACTER_COUNT &&
         h.positions==POSITION_COUNT,"geometry");
    must(h.source_length==uint64_t(PAGE_COUNT)*POSITION_COUNT,"source length");

    Field field=make_empty_field();
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
        for(uint16_t q=0;q<PAGE_COUNT;++q){
            int x=in.get();
            must(x>=0 && x<CHARACTER_COUNT,"overlay symbol");
            field.data_node(pos).imprint(q,uint16_t(x));
        }
    }
    char trailing;
    must(!in.get(trailing),"trailing bytes");

    must(field.switch_observation()==SwitchObservation::NEITHER,
         "reloaded field must begin OFF");
    field.power_on();

    std::ofstream out(recovered_path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open recovered");

    uint64_t frames=0;
    uint64_t data_true=0,data_false=0;
    uint64_t selected_pages=0,unselected_pages=0;
    uint64_t selected_characters=0,unselected_characters=0;

    for(uint16_t q=0;q<PAGE_COUNT;++q){
        std::vector<int16_t> winner(POSITION_COUNT,-1);
        std::vector<uint16_t> yes(POSITION_COUNT,0);

        for(uint16_t c=0;c<CHARACTER_COUNT;++c){
            auto frame=field.query(q,c);
            ++frames;
            selected_pages+=frame.selected_page_activators;
            unselected_pages+=frame.unselected_page_activators;
            selected_characters+=frame.selected_character_activators;
            unselected_characters+=frame.unselected_character_activators;

            must(frame.data_responses.size()==POSITION_COUNT,"response width");
            for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
                auto o=frame.data_responses[pos];
                if(o==ActiveObservation::POS){
                    ++data_true;
                    ++yes[pos];
                    winner[pos]=int16_t(c);
                }else if(o==ActiveObservation::NEG){
                    ++data_false;
                }else{
                    throw std::runtime_error("data node emitted non-boolean active result");
                }
            }
        }

        for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
            must(yes[pos]==1,"exactly one TRUE per position");
            out.put(char(uint8_t(winner[pos])));
        }
    }
    out.close();

    for(uint16_t q: {uint16_t(3),uint16_t(0),uint16_t(2),uint16_t(1),uint16_t(3)}){
        const auto page=reconstruct_page(field,q);
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            must(page[pos]==uint8_t(field.data_node(pos).symbol_for(q)),
                 "selector revisit mismatch");
    }

    field.power_off();
    must(field.switch_observation()==SwitchObservation::NEITHER,
         "NEITHER must exist at OFF switch");

    std::cout<<"status=CONDITIONED_OVERLAY_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<h.source_length<<"\n";
    std::cout<<"query_frames="<<frames<<"\n";
    std::cout<<"data_true_responses="<<data_true<<"\n";
    std::cout<<"data_false_responses="<<data_false<<"\n";
    std::cout<<"data_nonboolean_responses=0\n";
    std::cout<<"selected_page_activator_events="<<selected_pages<<"\n";
    std::cout<<"unselected_page_activator_events="<<unselected_pages<<"\n";
    std::cout<<"selected_character_activator_events="<<selected_characters<<"\n";
    std::cout<<"unselected_character_activator_events="<<unselected_characters<<"\n";
    std::cout<<"selector_revisit=PASS\n";
    std::cout<<"neither_seen_during_active_queries=0\n";
    std::cout<<"final_switch_state=NEITHER\n";
    return 0;
}

int main(int argc,char**argv){
    try{
        if(argc==2 && std::string(argv[1])=="selftest") return selftest();
        if(argc==4 && std::string(argv[1])=="freeze") return freeze_cmd(argv[2],argv[3]);
        if(argc==4 && std::string(argv[1])=="replay") return replay_cmd(argv[2],argv[3]);
        std::cerr<<"usage: selftest | freeze SOURCE ARTIFACT | replay ARTIFACT OUTPUT\n";
        return 64;
    }catch(const std::exception& e){
        std::cerr<<"error="<<e.what()<<"\n";
        return 70;
    }
}
