#include "../trucompute/trucompute_native_collapse_v8.hpp"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace trucompute_collapse_v8;

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

static Field make_field(){
    Field f;
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos) f.add_data_node(pos);
    for(uint16_t q=0;q<PAGE_COUNT;++q) f.add_page_activator(q);
    for(uint16_t c=0;c<CHARACTER_COUNT;++c) f.add_character_activator(c);
    must(f.all_data_nodes_native_both(),"new native field must be BOTH");
    return f;
}

static std::vector<uint8_t> reconstruct_page(const Field& f,uint16_t page){
    std::vector<uint16_t> yes(POSITION_COUNT,0);
    std::vector<int16_t> winner(POSITION_COUNT,-1);

    for(uint16_t c=0;c<CHARACTER_COUNT;++c){
        auto frame=f.observe(page,c);
        must(frame.collapses.size()==POSITION_COUNT,"not every data node collapsed");
        must(frame.selected_page_activators==1,"page activator selection");
        must(frame.selected_character_activators==1,"character activator selection");

        for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
            auto x=frame.collapses[pos];
            must(x==Collapse::POS || x==Collapse::NEG,"collapse was not +/-1");
            if(x==Collapse::POS){
                ++yes[pos];
                winner[pos]=int16_t(c);
            }
        }

        // Observation must not alter the native field.
        must(f.all_data_nodes_native_both(),"observation mutated native BOTH field");
    }

    std::vector<uint8_t> out(POSITION_COUNT);
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
        must(yes[pos]==1,"each position requires exactly one TRUE");
        out[pos]=uint8_t(winner[pos]);
    }
    return out;
}

static void populate(Field& f){
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
        for(uint16_t q=0;q<PAGE_COUNT;++q)
            f.data_node(pos).imprint(q,uint16_t((q*37 + pos*11 + 3)%CHARACTER_COUNT));
}

static int selftest(){
    Field f=make_field();

    // Underlying field is BOTH independent of machine switch.
    must(f.all_data_nodes_native_both(),"field is not intrinsically BOTH");
    must(f.switch_observation()==SwitchObservation::NEITHER,"only OFF switch may be NEITHER");

    bool off_observation_rejected=false;
    try{ (void)f.observe(0,0); }
    catch(const std::runtime_error&){ off_observation_rejected=true; }
    must(off_observation_rejected,"OFF machine allowed collapse");

    populate(f);

    // Same native node contains multiple differently conditioned overlays.
    must(f.data_node(7).symbol_for(0)!=f.data_node(7).symbol_for(1),
         "fixture needs different overlays");
    must(f.data_node(7).native_state()==NativeFieldState::BOTH,
         "imprint must not collapse node");

    f.power_on();
    must(f.switch_observation()==SwitchObservation::ON,"power ON");

    auto p0=reconstruct_page(f,0);
    auto p1=reconstruct_page(f,1);
    auto p2=reconstruct_page(f,2);
    auto p3=reconstruct_page(f,3);

    // Selector order/revisits cannot rewrite the underlying overlays.
    must(reconstruct_page(f,2)==p2,"page2 revisit changed");
    must(reconstruct_page(f,0)==p0,"page0 revisit changed");
    must(reconstruct_page(f,3)==p3,"page3 revisit changed");
    must(reconstruct_page(f,1)==p1,"page1 revisit changed");
    must(f.all_data_nodes_native_both(),"selector revisits mutated BOTH field");

    f.power_off();
    must(f.switch_observation()==SwitchObservation::NEITHER,"OFF boundary");
    must(f.all_data_nodes_native_both(),"power OFF must not destroy native BOTH field");

    std::cout<<"TRUCOMPUTE_NATIVE_COLLAPSE_V8=PASS\n";
    std::cout<<"native_field_rest_state=BOTH\n";
    std::cout<<"neither_scope=POWER_SWITCH_ONLY\n";
    std::cout<<"observation_collapse=POS_OR_NEG_ONLY\n";
    std::cout<<"collapse_mutates_native_field=false\n";
    std::cout<<"conditioned_overlays_coexist=PASS\n";
    std::cout<<"selector_revisit=PASS\n";
    std::cout<<"exact_one_true_per_position=PASS\n";
    return 0;
}

static int freeze_cmd(const std::string& source,const std::string& artifact){
    std::ifstream in(source,std::ios::binary);
    must(bool(in),"open source");
    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    must(src.size()==uint64_t(PAGE_COUNT)*POSITION_COUNT,"source fixture length");
    for(uint8_t b:src) must(b<CHARACTER_COUNT,"symbol outside 206");

    Field f=make_field();
    for(uint16_t q=0;q<PAGE_COUNT;++q)
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            f.data_node(pos).imprint(q,src[uint64_t(q)*POSITION_COUNT+pos]);

    must(f.all_data_nodes_native_both(),"imprint mutated native field");
    f.power_on();

    for(uint16_t q=0;q<PAGE_COUNT;++q){
        auto page=reconstruct_page(f,q);
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            must(page[pos]==src[uint64_t(q)*POSITION_COUNT+pos],
                 "prefreeze replay mismatch");
    }
    f.power_off();

    Header h{};
    std::memcpy(h.magic,"TCNCV8",6);
    h.version=8;
    h.pages=PAGE_COUNT;
    h.characters=CHARACTER_COUNT;
    h.positions=POSITION_COUNT;
    h.source_length=src.size();

    std::ofstream out(artifact,std::ios::binary|std::ios::trunc);
    must(bool(out),"open artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));

    // Conformance-only serialization of overlay conditions. It is not a
    // compression result; it exists solely to test reload + exact replay.
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
        for(uint16_t q=0;q<PAGE_COUNT;++q)
            out.put(char(uint8_t(f.data_node(pos).symbol_for(q))));

    must(bool(out),"write artifact");
    out.close();

    std::cout<<"status=NATIVE_COLLAPSE_FIELD_FROZEN\n";
    std::cout<<"source_bytes="<<src.size()<<"\n";
    std::cout<<"native_data_nodes="<<POSITION_COUNT<<"\n";
    std::cout<<"native_rest_state=BOTH\n";
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
    must(std::string(h.magic,6)=="TCNCV8","magic");
    must(h.version==8,"version");
    must(h.pages==PAGE_COUNT && h.characters==CHARACTER_COUNT &&
         h.positions==POSITION_COUNT,"geometry");

    Field f=make_field();
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
        for(uint16_t q=0;q<PAGE_COUNT;++q){
            int x=in.get();
            must(x>=0 && x<CHARACTER_COUNT,"overlay symbol");
            f.data_node(pos).imprint(q,uint16_t(x));
        }
    }
    char trailing;
    must(!in.get(trailing),"trailing bytes");

    must(f.switch_observation()==SwitchObservation::NEITHER,"fresh field begins OFF");
    must(f.all_data_nodes_native_both(),"freshly loaded field must be BOTH");

    f.power_on();
    std::ofstream out(recovered_path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open recovered");

    uint64_t frames=0,true_count=0,false_count=0;
    for(uint16_t q=0;q<PAGE_COUNT;++q){
        std::vector<uint16_t> yes(POSITION_COUNT,0);
        std::vector<int16_t> winner(POSITION_COUNT,-1);

        for(uint16_t c=0;c<CHARACTER_COUNT;++c){
            auto frame=f.observe(q,c);
            ++frames;
            for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
                auto x=frame.collapses[pos];
                if(x==Collapse::POS){
                    ++true_count;
                    ++yes[pos];
                    winner[pos]=int16_t(c);
                }else if(x==Collapse::NEG){
                    ++false_count;
                }else{
                    throw std::runtime_error("invalid collapse");
                }
            }
            must(f.all_data_nodes_native_both(),"query changed native field");
        }

        for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
            must(yes[pos]==1,"exact one TRUE");
            out.put(char(uint8_t(winner[pos])));
        }
    }
    out.close();

    // Out-of-order selector revisit after full reconstruction.
    for(uint16_t q: {uint16_t(3),uint16_t(0),uint16_t(2),uint16_t(1),uint16_t(3)}){
        auto page=reconstruct_page(f,q);
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            must(page[pos]==uint8_t(f.data_node(pos).symbol_for(q)),
                 "selector revisit mismatch");
    }

    f.power_off();
    must(f.switch_observation()==SwitchObservation::NEITHER,"final OFF");
    must(f.all_data_nodes_native_both(),"OFF destroyed native field");

    std::cout<<"status=NATIVE_COLLAPSE_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<h.source_length<<"\n";
    std::cout<<"query_frames="<<frames<<"\n";
    std::cout<<"true_collapses="<<true_count<<"\n";
    std::cout<<"false_collapses="<<false_count<<"\n";
    std::cout<<"nonboolean_collapses=0\n";
    std::cout<<"native_field_after_queries=BOTH\n";
    std::cout<<"selector_revisit=PASS\n";
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
