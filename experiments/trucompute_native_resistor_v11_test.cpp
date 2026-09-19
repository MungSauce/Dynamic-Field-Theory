#include "../trucompute/trucompute_native_resistor_v11.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace trucompute_resistor_v11;

static constexpr size_t CHANNELS=8;
static constexpr uint32_t POSITIONS=64;

static void must(bool x,const char* m){ if(!x) throw std::runtime_error(m); }

#pragma pack(push,1)
struct Header{
    char magic[8];
    uint16_t version;
    uint16_t channels;
    uint16_t pages;
    uint16_t characters;
    uint32_t nodes;
    uint64_t source_length;
};
#pragma pack(pop)

static std::vector<uint8_t> reconstruct(Machine<CHANNELS>& m,uint16_t pages){
    std::vector<uint8_t> out;
    out.reserve(uint64_t(pages)*m.node_count());

    for(uint16_t q=0;q<pages;++q){
        std::vector<uint16_t> yes(m.node_count(),0);
        std::vector<int16_t> winner(m.node_count(),-1);

        for(uint16_t c=0;c<CHARACTER_COUNT;++c){
            auto frame=m.set_probe(q,c);
            must(frame.responses.size()==m.node_count(),"response width");
            must(frame.selected_page_activators==1,"page activator");
            must(frame.selected_character_activators==1,"character activator");

            for(size_t i=0;i<frame.responses.size();++i){
                auto r=frame.responses[i];
                must(r==NativeCondition::POS || r==NativeCondition::NEG,
                     "resolved response must be +/-");
                if(r==NativeCondition::POS){
                    ++yes[i];
                    winner[i]=int16_t(c);
                }
            }
            must(m.all_substrates_both(),"observation mutated native substrate");
        }

        for(size_t i=0;i<m.node_count();++i){
            must(yes[i]==1,"exactly one true character per position");
            out.push_back(uint8_t(winner[i]));
        }
    }
    return out;
}

static void save_machine(const std::string& path,const Machine<CHANNELS>& m,
                         uint16_t pages,uint64_t source_length){
    Header h{};
    std::memcpy(h.magic,"TCRV11",6);
    h.version=11;
    h.channels=CHANNELS;
    h.pages=pages;
    h.characters=CHARACTER_COUNT;
    h.nodes=uint32_t(m.node_count());
    h.source_length=source_length;

    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));

    for(const auto& n:m.nodes()){
        for(uint16_t x:n.imprint()){
            out.put(char(uint8_t(x&255u)));
            out.put(char(uint8_t((x>>8)&255u)));
        }
    }
    must(bool(out),"write artifact");
}

static std::pair<Machine<CHANNELS>,Header> load_machine(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    must(bool(in),"open artifact");

    Header h{};
    in.read(reinterpret_cast<char*>(&h),sizeof(h));
    must(bool(in),"header");
    must(std::string(h.magic,6)=="TCRV11","magic");
    must(h.version==11 && h.channels==CHANNELS &&
         h.characters==CHARACTER_COUNT,"architecture");

    Machine<CHANNELS> m(h.nodes);
    for(uint32_t i=0;i<h.nodes;++i){
        std::array<uint16_t,CHANNELS> coeff{};
        for(size_t j=0;j<CHANNELS;++j){
            int lo=in.get(),hi=in.get();
            must(lo>=0 && hi>=0,"short imprint");
            coeff[j]=uint16_t(uint16_t(lo)|(uint16_t(hi)<<8));
            must(coeff[j]<PRIME,"coefficient outside field");
        }
        m.node(i).set_imprint(coeff);
    }
    char extra;
    must(!in.get(extra),"trailing bytes");
    return {std::move(m),h};
}

static int selftest(){
    ResistorNode<CHANNELS> n;
    must(n.substrate_condition()==NativeCondition::BOTH,"native substrate");
    std::array<uint16_t,CHANNELS> coeff{};
    coeff[0]=42;
    n.set_imprint(coeff);

    const auto original=n.imprint();

    must(n.effective_resistance({0,42})==R_POS,"true probe high resistance");
    must(n.resolve({0,42})==NativeCondition::POS,"true probe positive");
    must(n.effective_resistance({0,41})==R_NEG,"false probe low resistance");
    must(n.resolve({0,41})==NativeCondition::NEG,"false probe negative");

    // Direct probe changes: POS -> NEG -> POS without any OFF/reset.
    must(n.resolve({7,42})==NativeCondition::POS,"direct transition POS");
    must(n.resolve({7,17})==NativeCondition::NEG,"direct transition NEG");
    must(n.resolve({999,42})==NativeCondition::POS,"direct transition POS again");
    must(n.imprint()==original,"probing mutated imprint");

    Machine<CHANNELS> m(1);
    m.node(0).set_imprint(coeff);
    auto a=m.set_probe(0,42);
    auto b=m.set_probe(0,41);
    auto c=m.set_probe(999,42);
    must(a.responses[0]==NativeCondition::POS,"machine probe1");
    must(b.responses[0]==NativeCondition::NEG,"machine probe2");
    must(c.responses[0]==NativeCondition::POS,"machine probe3");
    must(m.transition_count()==3,"continuous transition count");
    must(m.all_substrates_both(),"machine mutated substrate");

    std::cout<<"TRUCOMPUTE_NATIVE_RESISTOR_V11=PASS\n";
    std::cout<<"native_domain=NEG_BOTH_POS\n";
    std::cout<<"machine_off_state=false\n";
    std::cout<<"reset_between_probes=false\n";
    std::cout<<"node_native_primitive=CONDITION_SENSITIVE_RESISTOR\n";
    std::cout<<"native_substrate=BOTH\n";
    std::cout<<"negative_resistance="<<R_NEG<<"\n";
    std::cout<<"positive_resistance="<<R_POS<<"\n";
    std::cout<<"flip_threshold="<<R_THRESHOLD<<"\n";
    std::cout<<"page_response_table=false\n";
    std::cout<<"character_response_table=false\n";
    std::cout<<"fixed_imprint_channels="<<CHANNELS<<"\n";
    std::cout<<"probe_mutates_imprint=false\n";
    return 0;
}

static int freeze_cmd(const std::string& source_path,const std::string& artifact){
    std::ifstream in(source_path,std::ios::binary);
    must(bool(in),"open source");
    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    must(!src.empty() && src.size()%POSITIONS==0,"whole pages required");
    uint16_t pages=uint16_t(src.size()/POSITIONS);

    Machine<CHANNELS> m(POSITIONS);
    auto failure=FixedImprinter<CHANNELS>::imprint_exact(m.nodes(),src,POSITIONS);

    if(failure.failed){
        std::cout<<"status=CAPACITY_EXCEEDED"
                 <<" node="<<failure.node
                 <<" page="<<failure.page
                 <<" expected="<<failure.expected
                 <<" predicted="<<failure.predicted
                 <<" channels="<<CHANNELS
                 <<" artifact_created=false\n";
        return 2;
    }

    auto recovered=reconstruct(m,pages);
    must(recovered==src,"prefreeze exact replay");
    save_machine(artifact,m,pages,src.size());

    std::cout<<"status=NATIVE_RESISTOR_FIELD_FROZEN\n";
    std::cout<<"source_bytes="<<src.size()<<"\n";
    std::cout<<"pages="<<pages<<"\n";
    std::cout<<"node_count="<<POSITIONS<<"\n";
    std::cout<<"fixed_imprint_channels="<<CHANNELS<<"\n";
    std::cout<<"coefficient_bytes_per_node="<<(2*CHANNELS)<<"\n";
    std::cout<<"page_response_table=false\n";
    std::cout<<"character_response_table=false\n";
    std::cout<<"residual_bytes=0\n";
    std::cout<<"prefreeze_exact_replay=PASS\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int replay_cmd(const std::string& artifact,const std::string& outpath){
    auto loaded=load_machine(artifact);
    auto& m=loaded.first;
    const auto& h=loaded.second;

    auto recovered=reconstruct(m,h.pages);
    must(recovered.size()==h.source_length,"recovered size");

    std::ofstream out(outpath,std::ios::binary|std::ios::trunc);
    must(bool(out),"open output");
    out.write(reinterpret_cast<const char*>(recovered.data()),std::streamsize(recovered.size()));
    must(bool(out),"write output");

    std::cout<<"status=NATIVE_RESISTOR_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<recovered.size()<<"\n";
    std::cout<<"pages="<<h.pages<<"\n";
    std::cout<<"node_count="<<h.nodes<<"\n";
    std::cout<<"fixed_imprint_channels="<<h.channels<<"\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    std::cout<<"machine_off_state=false\n";
    std::cout<<"reset_between_probes=false\n";
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
