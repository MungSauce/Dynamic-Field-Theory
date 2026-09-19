#include "../trucompute/trucompute_page_conditioned_v9.hpp"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace trucompute_conditioned_v9;

static constexpr uint16_t CHARACTER_COUNT=206;
static constexpr uint16_t PAGE_COUNT=4;
static constexpr uint32_t POSITION_COUNT=64;

static void must(bool x,const char* m){ if(!x) throw std::runtime_error(m); }

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

static Field make_field(){
    Field f;
    for(uint32_t i=0;i<POSITION_COUNT;++i) f.add_data_node(i);
    for(uint16_t q=0;q<PAGE_COUNT;++q) f.add_page_activator(q);
    for(uint16_t c=0;c<CHARACTER_COUNT;++c) f.add_character_activator(c);
    must(f.all_data_nodes_powered(),"all data nodes must be continuously powered");
    return f;
}

static void populate(Field& f){
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
        for(uint16_t q=0;q<PAGE_COUNT;++q)
            f.data_node(pos).imprint(q,uint16_t((q*37+pos*11+3)%CHARACTER_COUNT));
}

static std::vector<uint8_t> reconstruct_page(const Field& f,uint16_t q){
    std::vector<uint16_t> yes(POSITION_COUNT,0);
    std::vector<int16_t> winner(POSITION_COUNT,-1);

    for(uint16_t c=0;c<CHARACTER_COUNT;++c){
        auto frame=f.observe(q,c);
        must(frame.responses.size()==POSITION_COUNT,"every data node must resolve");
        must(frame.selected_page_activators==1,"one page activator");
        must(frame.selected_character_activators==1,"one character activator");

        for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
            const auto r=frame.responses[pos];
            must(r==Resolved::POS || r==Resolved::NEG,"data result must be +/-");
            if(r==Resolved::POS){ ++yes[pos]; winner[pos]=int16_t(c); }
        }
    }

    std::vector<uint8_t> out(POSITION_COUNT);
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
        must(yes[pos]==1,"every page-position must have exactly one TRUE letter");
        out[pos]=uint8_t(winner[pos]);
    }
    return out;
}

static int selftest(){
    Field f=make_field();

    must(f.switch_state()==SwitchState::NEITHER,"only OFF switch is NEITHER");
    for(size_t i=0;i<f.data_node_count();++i){
        must(f.data_node(i).always_powered(),"data node unexpectedly unpowered");
        must(f.data_node(i).unresolved_condition()==Unresolved::BOTH,"native unresolved condition must be BOTH");
    }

    populate(f);

    // Same position, different pages, different true letters.
    const uint16_t p0=f.data_node(7).symbol_for(0);
    const uint16_t p1=f.data_node(7).symbol_for(1);
    must(p0!=p1,"fixture must have page-dependent truth");

    f.power_on();

    // Direct truth table proof for same always-powered node.
    auto a=f.observe(0,p0);
    auto b=f.observe(0,p1);
    auto c=f.observe(1,p0);
    auto d=f.observe(1,p1);
    must(a.responses[7]==Resolved::POS,"page0 true letter must be TRUE");
    must(b.responses[7]==Resolved::NEG,"page0 other page letter must be FALSE");
    must(c.responses[7]==Resolved::NEG,"page1 page0 letter must be FALSE");
    must(d.responses[7]==Resolved::POS,"page1 true letter must be TRUE");

    // Data node never powers down or changes its unresolved BOTH condition.
    must(f.data_node(7).always_powered(),"observation powered down data node");
    must(f.data_node(7).unresolved_condition()==Unresolved::BOTH,"observation destroyed BOTH condition");

    auto q0=reconstruct_page(f,0);
    auto q1=reconstruct_page(f,1);
    auto q2=reconstruct_page(f,2);
    auto q3=reconstruct_page(f,3);

    must(reconstruct_page(f,2)==q2,"page2 revisit");
    must(reconstruct_page(f,0)==q0,"page0 revisit");
    must(reconstruct_page(f,3)==q3,"page3 revisit");
    must(reconstruct_page(f,1)==q1,"page1 revisit");

    must(f.all_data_nodes_powered(),"selector sweep changed node power");

    f.power_off();
    must(f.switch_state()==SwitchState::NEITHER,"final OFF must be NEITHER");
    must(f.all_data_nodes_powered(),"data-node existence/power law must not be selector-gated");

    std::cout<<"TRUCOMPUTE_PAGE_CONDITIONED_V9=PASS\n";
    std::cout<<"data_nodes_continuously_powered=PASS\n";
    std::cout<<"unresolved_field_condition=BOTH\n";
    std::cout<<"resolution_condition=PAGE_PLUS_CHARACTER\n";
    std::cout<<"every_page_has_true_or_false_for_every_character=PASS\n";
    std::cout<<"same_node_changes_output_by_condition=PASS\n";
    std::cout<<"unselected_activators_not_part_of_operation=PASS\n";
    std::cout<<"selector_revisit=PASS\n";
    std::cout<<"exact_one_true_letter_per_page_position=PASS\n";
    std::cout<<"neither_scope=POWER_SWITCH_ONLY\n";
    return 0;
}

static int freeze_cmd(const std::string& srcpath,const std::string& artifact){
    std::ifstream in(srcpath,std::ios::binary);
    must(bool(in),"open source");
    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    must(src.size()==uint64_t(PAGE_COUNT)*POSITION_COUNT,"fixture source size");
    for(auto b:src) must(b<CHARACTER_COUNT,"fixture symbol");

    Field f=make_field();
    for(uint16_t q=0;q<PAGE_COUNT;++q)
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            f.data_node(pos).imprint(q,src[uint64_t(q)*POSITION_COUNT+pos]);

    f.power_on();
    for(uint16_t q=0;q<PAGE_COUNT;++q){
        auto page=reconstruct_page(f,q);
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            must(page[pos]==src[uint64_t(q)*POSITION_COUNT+pos],"prefreeze replay");
    }
    f.power_off();

    Header h{};
    std::memcpy(h.magic,"TCPCV9",6);
    h.version=9; h.pages=PAGE_COUNT; h.characters=CHARACTER_COUNT;
    h.positions=POSITION_COUNT; h.source_length=src.size();

    std::ofstream out(artifact,std::ios::binary|std::ios::trunc);
    must(bool(out),"open artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));

    // Conformance-only persistence of page-conditioned truth.
    // Not a compression representation.
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
        for(uint16_t q=0;q<PAGE_COUNT;++q)
            out.put(char(uint8_t(f.data_node(pos).symbol_for(q))));

    must(bool(out),"write artifact");
    out.close();

    std::cout<<"status=PAGE_CONDITIONED_FIELD_FROZEN\n";
    std::cout<<"source_bytes="<<src.size()<<"\n";
    std::cout<<"data_nodes="<<POSITION_COUNT<<"\n";
    std::cout<<"page_conditions_per_node="<<PAGE_COUNT<<"\n";
    std::cout<<"character_activators="<<CHARACTER_COUNT<<"\n";
    std::cout<<"prefreeze_exact_replay=PASS\n";
    std::cout<<"artifact_purpose=CONFORMANCE_ONLY_NOT_COMPRESSION\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int replay_cmd(const std::string& artifact,const std::string& outpath){
    std::ifstream in(artifact,std::ios::binary);
    must(bool(in),"open artifact");

    Header h{};
    in.read(reinterpret_cast<char*>(&h),sizeof(h));
    must(bool(in),"read header");
    must(std::string(h.magic,6)=="TCPCV9","magic");
    must(h.version==9 && h.pages==PAGE_COUNT && h.characters==CHARACTER_COUNT &&
         h.positions==POSITION_COUNT,"geometry");

    Field f=make_field();
    for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
        for(uint16_t q=0;q<PAGE_COUNT;++q){
            int x=in.get();
            must(x>=0 && x<CHARACTER_COUNT,"overlay symbol");
            f.data_node(pos).imprint(q,uint16_t(x));
        }

    char extra;
    must(!in.get(extra),"trailing bytes");

    f.power_on();
    std::ofstream out(outpath,std::ios::binary|std::ios::trunc);
    must(bool(out),"open output");

    uint64_t frames=0,positives=0,negatives=0;
    for(uint16_t q=0;q<PAGE_COUNT;++q){
        std::vector<uint16_t> yes(POSITION_COUNT,0);
        std::vector<int16_t> winner(POSITION_COUNT,-1);

        for(uint16_t ch=0;ch<CHARACTER_COUNT;++ch){
            auto frame=f.observe(q,ch);
            ++frames;
            for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
                if(frame.responses[pos]==Resolved::POS){
                    ++positives; ++yes[pos]; winner[pos]=int16_t(ch);
                }else{
                    ++negatives;
                }
            }
        }

        for(uint32_t pos=0;pos<POSITION_COUNT;++pos){
            must(yes[pos]==1,"exactly one TRUE");
            out.put(char(uint8_t(winner[pos])));
        }
    }
    out.close();

    must(f.all_data_nodes_powered(),"replay changed node power law");

    for(uint16_t q: {uint16_t(3),uint16_t(0),uint16_t(2),uint16_t(1),uint16_t(3)}){
        auto page=reconstruct_page(f,q);
        for(uint32_t pos=0;pos<POSITION_COUNT;++pos)
            must(page[pos]==uint8_t(f.data_node(pos).symbol_for(q)),"selector revisit mismatch");
    }

    f.power_off();

    std::cout<<"status=PAGE_CONDITIONED_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<h.source_length<<"\n";
    std::cout<<"query_frames="<<frames<<"\n";
    std::cout<<"positive_responses="<<positives<<"\n";
    std::cout<<"negative_responses="<<negatives<<"\n";
    std::cout<<"nonboolean_responses=0\n";
    std::cout<<"data_nodes_continuously_powered=PASS\n";
    std::cout<<"selector_revisit=PASS\n";
    std::cout<<"final_switch_state=NEITHER\n";
    return 0;
}

int main(int argc,char**argv){
    try{
        if(argc==2 && std::string(argv[1])=="selftest") return selftest();
        if(argc==4 && std::string(argv[1])=="freeze") return freeze_cmd(argv[2],argv[3]);
        if(argc==4 && std::string(argv[1])=="replay") return replay_cmd(argv[2],argv[3]);
        return 64;
    }catch(const std::exception& e){
        std::cerr<<"error="<<e.what()<<"\n";
        return 70;
    }
}
