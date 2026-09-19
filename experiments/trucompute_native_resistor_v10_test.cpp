#include "../trucompute/trucompute_native_resistor_v10.hpp"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace trucompute_resistive_v10;

static constexpr size_t CHANNELS=8;
static constexpr uint32_t POSITIONS=64;

static void must(bool x,const char* m){ if(!x) throw std::runtime_error(m); }

#pragma pack(push,1)
struct Header {
    char magic[8];
    uint16_t version;
    uint16_t channels;
    uint16_t prime;
    uint16_t characters;
    uint16_t max_pages;
    uint16_t pages;
    uint32_t nodes;
    uint64_t source_length;
};
#pragma pack(pop)

static std::vector<uint8_t> reconstruct(Field<CHANNELS>& field,uint16_t pages){
    field.power_on();
    std::vector<uint8_t> out;
    out.reserve(uint64_t(pages)*field.node_count());

    for(uint16_t q=0;q<pages;++q){
        std::vector<uint16_t> yes(field.node_count(),0);
        std::vector<int16_t> winner(field.node_count(),-1);

        for(uint16_t c=0;c<CHARACTER_COUNT;++c){
            auto responses=field.observe(q,c);
            must(responses.size()==field.node_count(),"response width");
            for(size_t i=0;i<responses.size();++i){
                if(responses[i]==Resolved::POS){
                    ++yes[i];
                    winner[i]=int16_t(c);
                }else{
                    must(responses[i]==Resolved::NEG,"nonboolean result");
                }
            }
        }

        for(size_t i=0;i<field.node_count();++i){
            must(yes[i]==1,"position must have exactly one TRUE");
            out.push_back(uint8_t(winner[i]));
        }
    }

    return out;
}

static void save_field(const std::string& path,const Field<CHANNELS>& field,uint16_t pages,uint64_t source_length){
    Header h{};
    std::memcpy(h.magic,"TCRV10",6);
    h.version=10;
    h.channels=CHANNELS;
    h.prime=PRIME;
    h.characters=CHARACTER_COUNT;
    h.max_pages=MAX_PAGES;
    h.pages=pages;
    h.nodes=uint32_t(field.node_count());
    h.source_length=source_length;

    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));
    for(const auto& n:field.nodes()){
        for(auto x:n.imprint()){
            out.put(char(uint8_t(x&255u)));
            out.put(char(uint8_t((x>>8)&255u)));
        }
    }
    must(bool(out),"write artifact");
}

static std::pair<Field<CHANNELS>,Header> load_field(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    must(bool(in),"open artifact");

    Header h{};
    in.read(reinterpret_cast<char*>(&h),sizeof(h));
    must(bool(in),"read header");
    must(std::string(h.magic,6)=="TCRV10","magic");
    must(h.version==10 && h.channels==CHANNELS && h.prime==PRIME &&
         h.characters==CHARACTER_COUNT && h.max_pages==MAX_PAGES,"architecture header");

    Field<CHANNELS> field(h.nodes);
    for(uint32_t i=0;i<h.nodes;++i){
        std::array<uint16_t,CHANNELS> v{};
        for(size_t j=0;j<CHANNELS;++j){
            int lo=in.get(),hi=in.get();
            must(lo>=0 && hi>=0,"short imprint");
            v[j]=uint16_t(uint16_t(lo)|(uint16_t(hi)<<8));
            must(v[j]<PRIME,"coefficient outside field");
        }
        field.node(i).set_imprint(v);
    }
    char extra;
    must(!in.get(extra),"trailing bytes");
    return {std::move(field),h};
}

static int selftest(){
    static_assert(sizeof(std::array<uint16_t,CHANNELS>)==CHANNELS*sizeof(uint16_t),
                  "fixed imprint must be dense fixed width");

    ResistorNode<CHANNELS> n;
    must(n.always_powered(),"node must be continuously powered");
    must(n.unresolved_condition()==Unresolved::BOTH,"unresolved condition must be BOTH");

    std::array<uint16_t,CHANNELS> v{};
    v[0]=42;
    n.set_imprint(v);

    for(uint16_t q: {uint16_t(0),uint16_t(1),uint16_t(999)}){
        must(n.effective_resistance({q,42})==R_FLIP,"matching probe must resist");
        must(n.resolve({q,42})==Resolved::POS,"matching probe must flip positive");
        must(n.effective_resistance({q,41})==R_LOW,"nonmatch must have low resistance");
        must(n.resolve({q,41})==Resolved::NEG,"nonmatch must stay negative");
    }

    const auto before=n.imprint();
    for(uint16_t c=0;c<CHARACTER_COUNT;++c) (void)n.resolve({7,c});
    must(n.imprint()==before,"observation mutated imprint");

    std::cout<<"TRUCOMPUTE_NATIVE_RESISTOR_V10=PASS\n";
    std::cout<<"node_native_primitive=CONDITION_SENSITIVE_RESISTOR\n";
    std::cout<<"node_unresolved_condition=BOTH\n";
    std::cout<<"negative_resistance_units="<<R_LOW<<"\n";
    std::cout<<"positive_resistance_units="<<R_FLIP<<"\n";
    std::cout<<"flip_threshold_units="<<R_THRESHOLD<<"\n";
    std::cout<<"page_response_table=false\n";
    std::cout<<"character_response_table=false\n";
    std::cout<<"fixed_imprint_channels="<<CHANNELS<<"\n";
    std::cout<<"observation_mutates_imprint=false\n";
    return 0;
}


static int blank_cmd(const std::string& artifact){
    Field<CHANNELS> field(POSITIONS);
    // The machine already exists structurally in its unresolved BOTH condition.
    save_field(artifact,field,0,0);
    std::cout<<"status=NATIVE_RESISTOR_BLANK_FIELD_FROZEN\n";
    std::cout<<"node_count="<<POSITIONS<<"\n";
    std::cout<<"fixed_imprint_channels="<<CHANNELS<<"\n";
    std::cout<<"native_unresolved_condition=BOTH\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int freeze_cmd(const std::string& source_path,const std::string& artifact){
    std::ifstream in(source_path,std::ios::binary);
    must(bool(in),"open source");
    std::vector<uint8_t> source((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    must(!source.empty() && source.size()%POSITIONS==0,"source page geometry");
    const uint64_t pages64=source.size()/POSITIONS;
    must(pages64<=MAX_PAGES,"too many pages");
    const uint16_t pages=uint16_t(pages64);

    Field<CHANNELS> field(POSITIONS);
    const auto failure=Imprinter<CHANNELS>::imprint_exact(field.nodes(),source,POSITIONS);
    if(failure.failed){
        std::cout<<"status=CAPACITY_EXCEEDED"
                 <<" node="<<failure.node
                 <<" page="<<failure.page
                 <<" expected="<<failure.expected
                 <<" predicted="<<failure.predicted
                 <<" channels="<<CHANNELS
                 <<" node_count="<<POSITIONS
                 <<" artifact_created=false\n";
        return 2;
    }

    const auto recovered=reconstruct(field,pages);
    must(recovered==source,"prefreeze exact replay");
    field.power_off();

    save_field(artifact,field,pages,source.size());

    std::cout<<"status=NATIVE_RESISTOR_FIELD_FROZEN\n";
    std::cout<<"source_bytes="<<source.size()<<"\n";
    std::cout<<"pages="<<pages<<"\n";
    std::cout<<"node_count="<<POSITIONS<<"\n";
    std::cout<<"fixed_imprint_channels="<<CHANNELS<<"\n";
    std::cout<<"coefficient_bytes_per_node="<<(CHANNELS*sizeof(uint16_t))<<"\n";
    std::cout<<"page_response_table=false\n";
    std::cout<<"character_response_table=false\n";
    std::cout<<"residual_bytes=0\n";
    std::cout<<"prefreeze_exact_replay=PASS\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int replay_cmd(const std::string& artifact,const std::string& recovered_path){
    auto loaded=load_field(artifact);
    auto& field=loaded.first;
    const auto& h=loaded.second;

    const auto recovered=reconstruct(field,h.pages);
    field.power_off();

    must(recovered.size()==h.source_length,"recovered size");
    std::ofstream out(recovered_path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open recovered");
    out.write(reinterpret_cast<const char*>(recovered.data()),std::streamsize(recovered.size()));
    must(bool(out),"write recovered");

    std::cout<<"status=NATIVE_RESISTOR_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<recovered.size()<<"\n";
    std::cout<<"pages="<<h.pages<<"\n";
    std::cout<<"node_count="<<h.nodes<<"\n";
    std::cout<<"fixed_imprint_channels="<<h.channels<<"\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    std::cout<<"final_switch_state=NEITHER\n";
    return 0;
}

int main(int argc,char**argv){
    try{
        if(argc==2 && std::string(argv[1])=="selftest") return selftest();
        if(argc==3 && std::string(argv[1])=="blank") return blank_cmd(argv[2]);
        if(argc==4 && std::string(argv[1])=="freeze") return freeze_cmd(argv[2],argv[3]);
        if(argc==4 && std::string(argv[1])=="replay") return replay_cmd(argv[2],argv[3]);
        return 64;
    }catch(const std::exception& e){
        std::cerr<<"error="<<e.what()<<"\n";
        return 70;
    }
}
