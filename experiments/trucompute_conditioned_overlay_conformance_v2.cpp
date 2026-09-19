#include "../trucompute/trucompute_native_field_v6.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace trucompute_field_v6;

static constexpr uint16_t CHARACTER_COUNT=206;
static constexpr uint16_t PAGE_COUNT=4;
static constexpr uint32_t POSITION_COUNT=64;

static void must(bool x,const char* msg){
    if(!x) throw std::runtime_error(msg);
}

#pragma pack(push,1)
struct Header{
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

static void validate_query_law(const Field& field,uint16_t page,uint16_t character){
    auto frame=field.query(page,character);
    must(frame.data_responses.size()==POSITION_COUNT,"not every data node responded");
    must(frame.selected_page_activators==1,"page selected activator count");
    must(frame.inactive_page_activators==PAGE_COUNT-1,"page inactive activator count");
    must(frame.selected_character_activators==1,"character selected activator count");
    must(frame.inactive_character_activators==CHARACTER_COUNT-1,"character inactive activator count");

    for(auto o:frame.data_responses)
        must(o==Observation::POS || o==Observation::NEG,
             "data node did not resolve TRUE/FALSE");
}

static std::vector<uint8_t> reconstruct_page(const Field& field,uint16_t page){
    std::vector<int16_t> winner(POSITION_COUNT,-1);
    std::vector<uint16_t> yes(POSITION_COUNT,0);

    for(uint16_t c=0;c<CHARACTER_COUNT;++c){
        auto frame=field.query(page,c);
        for(uint32_t i=0;i<POSITION_COUNT;++i){
            auto o=frame.data_responses[i];
            must(o==Observation::POS || o==Observation::NEG,
                 "all data nodes must participate TRUE/FALSE");
            if(o==Observation::POS){
                ++yes[i];
                winner[i]=int16_t(c);
            }
        }
    }

    std::vector<uint8_t> out(POSITION_COUNT);
    for(uint32_t i=0;i<POSITION_COUNT;++i){
        must(yes[i]==1,"position did not have exactly one TRUE among 206");
        must(winner[i]>=0,"missing winning character");
        out[i]=uint8_t(winner[i]);
    }
    return out;
}

static int selftest(){
    Field field=make_empty_field();

    // Same native data node, same position, four different conditioned overlays.
    field.data_node(7).imprint(0,42);
    field.data_node(7).imprint(1,99);
    field.data_node(7).imprint(2,17);
    field.data_node(7).imprint(3,42);

    // Populate all other positions so every node can answer every valid query.
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
        if(pos==7) continue;
        for(uint16_t q=0;q<PAGE_COUNT;++q)
            field.data_node(pos).imprint(q,uint16_t((q*37+pos*11+3)%CHARACTER_COUNT));
    }

    // Different conditions can carry incompatible-looking symbols without conflict.
    must(field.data_node(7).symbol_for(0)==42,"page0 overlay");
    must(field.data_node(7).symbol_for(1)==99,"page1 overlay");
    must(field.data_node(7).symbol_for(2)==17,"page2 overlay");
    must(field.data_node(7).symbol_for(3)==42,"page3 overlay");

    // Same condition with same value is idempotent; conflicting value is invalid.
    field.data_node(7).imprint(1,99);
    bool same_condition_conflict=false;
    try{ field.data_node(7).imprint(1,100); }
    catch(const std::runtime_error&){ same_condition_conflict=true; }
    must(same_condition_conflict,"same-condition conflict was not detected");

    // Every data node participates TRUE/FALSE on every selected query.
    for(uint16_t q=0;q<PAGE_COUNT;++q){
        for(uint16_t c: {uint16_t(0),uint16_t(17),uint16_t(42),uint16_t(99),uint16_t(205)})
            validate_query_law(field,q,c);
    }

    // Selector changes expose different overlays without mutating the field.
    const auto p0=reconstruct_page(field,0);
    const auto p1=reconstruct_page(field,1);
    const auto p2=reconstruct_page(field,2);
    const auto p3=reconstruct_page(field,3);
    const auto p1_again=reconstruct_page(field,1);
    const auto p0_again=reconstruct_page(field,0);

    must(p1==p1_again,"page1 changed after selector switching");
    must(p0==p0_again,"page0 changed after selector switching");
    must(p0[7]==42 && p1[7]==99 && p2[7]==17 && p3[7]==42,
         "conditioned overlay replay mismatch");

    std::cout<<"CONDITIONED_OVERLAY_V2_SELFTEST=PASS\n";
    std::cout<<"data_nodes_always_true_or_false=PASS\n";
    std::cout<<"activators_only_participate_when_selected=PASS\n";
    std::cout<<"same_node_multi_condition_coexistence=PASS\n";
    std::cout<<"same_condition_conflict_detected=PASS\n";
    std::cout<<"selector_switch_reversible=PASS\n";
    std::cout<<"exact_one_true_per_position=PASS\n";
    std::cout<<"page_activators="<<field.page_activator_count()<<"\n";
    std::cout<<"character_activators="<<field.character_activator_count()<<"\n";
    std::cout<<"data_nodes="<<field.data_node_count()<<"\n";
    return 0;
}

static int freeze_cmd(const std::string& source,const std::string& artifact){
    std::ifstream in(source,std::ios::binary);
    must(bool(in),"open source");

    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)),{});
    must(src.size()==uint64_t(PAGE_COUNT)*POSITION_COUNT,"fixture source length");
    for(uint8_t b:src) must(b<CHARACTER_COUNT,"fixture outside 206 symbols");

    Field field=make_empty_field();
    for(uint16_t q=0;q<PAGE_COUNT;++q)
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            field.data_node(pos).imprint(q,src[uint64_t(q)*POSITION_COUNT+pos]);

    // Prove all conditioned overlays coexist before freeze.
    for(uint16_t q=0;q<PAGE_COUNT;++q){
        auto recovered=reconstruct_page(field,q);
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            must(recovered[pos]==src[uint64_t(q)*POSITION_COUNT+pos],
                 "prefreeze page reconstruction");
    }

    Header h{};
    std::memcpy(h.magic,"TCOVL206",8);
    h.version=2;
    h.pages=PAGE_COUNT;
    h.characters=CHARACTER_COUNT;
    h.positions=POSITION_COUNT;
    h.source_length=src.size();

    std::ofstream out(artifact,std::ios::binary|std::ios::trunc);
    must(bool(out),"open artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));

    // Conformance serialization, not a compression representation:
    // one symbol for every conditioned overlay so a fresh process can rebuild
    // the same native field and prove exact selector replay.
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
        for(uint16_t q=0;q<PAGE_COUNT;++q){
            uint8_t b=uint8_t(field.data_node(pos).symbol_for(q));
            out.put(char(b));
        }
    }
    must(bool(out),"write artifact");
    out.close();

    std::cout<<"status=CONDITIONED_FIELD_FROZEN\n";
    std::cout<<"source_bytes="<<src.size()<<"\n";
    std::cout<<"data_nodes="<<POSITION_COUNT<<"\n";
    std::cout<<"page_activators="<<PAGE_COUNT<<"\n";
    std::cout<<"character_activators="<<CHARACTER_COUNT<<"\n";
    std::cout<<"conditioned_overlays="<<(uint64_t(PAGE_COUNT)*POSITION_COUNT)<<"\n";
    std::cout<<"artifact_purpose=CONFORMANCE_ONLY_NOT_COMPRESSION\n";
    std::cout<<"prefreeze_exact_replay=PASS\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int replay_cmd(const std::string& artifact,const std::string& recovered_path){
    std::ifstream in(artifact,std::ios::binary);
    must(bool(in),"open artifact");

    Header h{};
    in.read(reinterpret_cast<char*>(&h),sizeof(h));
    must(bool(in),"header");
    must(std::string(h.magic,8)=="TCOVL206","magic");
    must(h.version==2,"version");
    must(h.pages==PAGE_COUNT && h.characters==CHARACTER_COUNT &&
         h.positions==POSITION_COUNT,"geometry");
    must(h.source_length==uint64_t(PAGE_COUNT)*POSITION_COUNT,"source length");

    Field field=make_empty_field();
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
        for(uint16_t q=0;q<PAGE_COUNT;++q){
            int x=in.get();
            must(x>=0,"short overlays");
            must(x<CHARACTER_COUNT,"overlay symbol");
            field.data_node(pos).imprint(q,uint16_t(x));
        }
    }
    char trailing;
    must(!in.get(trailing),"trailing bytes");

    std::ofstream out(recovered_path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open recovered");

    uint64_t query_frames=0;
    uint64_t data_true=0,data_false=0;
    uint64_t active_page_activators=0,inactive_page_activators=0;
    uint64_t active_char_activators=0,inactive_char_activators=0;

    for(uint16_t q=0;q<PAGE_COUNT;++q){
        std::vector<int16_t> winner(POSITION_COUNT,-1);
        std::vector<uint16_t> yes(POSITION_COUNT,0);

        for(uint16_t c=0;c<CHARACTER_COUNT;++c){
            auto frame=field.query(q,c);
            ++query_frames;
            active_page_activators+=frame.selected_page_activators;
            inactive_page_activators+=frame.inactive_page_activators;
            active_char_activators+=frame.selected_character_activators;
            inactive_char_activators+=frame.inactive_character_activators;

            must(frame.data_responses.size()==POSITION_COUNT,"response width");
            for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
                auto o=frame.data_responses[pos];
                if(o==Observation::POS){
                    ++data_true;
                    ++yes[pos];
                    winner[pos]=int16_t(c);
                }else if(o==Observation::NEG){
                    ++data_false;
                }else{
                    throw std::runtime_error("data node produced non-boolean response");
                }
            }
        }

        for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
            must(yes[pos]==1,"exactly one TRUE per position");
            out.put(char(uint8_t(winner[pos])));
        }
    }
    out.close();

    // Selector revisit after full reconstruction.
    for(uint16_t q: {uint16_t(3),uint16_t(0),uint16_t(2),uint16_t(1),uint16_t(3)}){
        auto page=reconstruct_page(field,q);
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            must(page[pos]==uint8_t(field.data_node(pos).symbol_for(q)),
                 "selector revisit mismatch");
    }

    std::cout<<"status=CONDITIONED_OVERLAY_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<h.source_length<<"\n";
    std::cout<<"query_frames="<<query_frames<<"\n";
    std::cout<<"data_true_responses="<<data_true<<"\n";
    std::cout<<"data_false_responses="<<data_false<<"\n";
    std::cout<<"data_nonboolean_responses=0\n";
    std::cout<<"selected_page_activator_events="<<active_page_activators<<"\n";
    std::cout<<"inactive_page_activator_events="<<inactive_page_activators<<"\n";
    std::cout<<"selected_character_activator_events="<<active_char_activators<<"\n";
    std::cout<<"inactive_character_activator_events="<<inactive_char_activators<<"\n";
    std::cout<<"selector_revisit=PASS\n";
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
