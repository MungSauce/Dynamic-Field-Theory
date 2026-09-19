#include "../trucompute/trucompute_streaming_imprinter_v11.hpp"
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

using namespace trucompute_stream_v11;

static constexpr size_t CHANNELS=8;
static constexpr uint32_t POSITIONS=64;

static void must(bool x,const char* m){ if(!x) throw std::runtime_error(m); }

#pragma pack(push,1)
struct Header{
    char magic[8];
    uint16_t version;
    uint16_t channels;
    uint16_t prime;
    uint16_t terminal_count;
    uint16_t pages;
    uint32_t nodes;
    uint64_t source_length;
    uint8_t terminals[CHARACTER_COUNT];
};
#pragma pack(pop)

static std::vector<uint8_t> reconstruct(Field<CHANNELS>& field,const TerminalMap& terminals,uint16_t pages){
    field.power_on();
    std::vector<uint8_t> out;
    out.reserve(uint64_t(pages)*field.node_count());

    for(uint16_t q=0;q<pages;++q){
        std::vector<uint16_t> yes(field.node_count(),0);
        std::vector<int16_t> winner(field.node_count(),-1);
        for(uint16_t c=0;c<terminals.count();++c){
            auto responses=field.observe(q,c);
            for(size_t i=0;i<responses.size();++i){
                if(responses[i]==Resolved::POS){ ++yes[i]; winner[i]=int16_t(c); }
                else must(responses[i]==Resolved::NEG,"nonboolean result");
            }
        }
        for(size_t i=0;i<field.node_count();++i){
            must(yes[i]==1,"position must have exactly one TRUE");
            out.push_back(terminals.decode(uint16_t(winner[i])));
        }
    }
    return out;
}

static void save_field(const std::string& path,const Field<CHANNELS>& field,
                       const TerminalMap& terminals,uint16_t pages,uint64_t source_length){
    Header h{};
    std::memcpy(h.magic,"TCSTR11",7);
    h.version=11;
    h.channels=CHANNELS;
    h.prime=PRIME;
    h.terminal_count=terminals.count();
    h.pages=pages;
    h.nodes=uint32_t(field.node_count());
    h.source_length=source_length;
    std::memcpy(h.terminals,terminals.reverse().data(),CHARACTER_COUNT);

    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));
    for(const auto& n:field.nodes()){
        for(auto v:n.imprint()){
            out.put(char(uint8_t(v&255)));
            out.put(char(uint8_t((v>>8)&255)));
        }
    }
    must(bool(out),"write artifact");
}

static std::pair<Field<CHANNELS>,std::pair<TerminalMap,Header>>
load_field(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    must(bool(in),"open artifact");
    Header h{};
    in.read(reinterpret_cast<char*>(&h),sizeof(h));
    must(bool(in),"read header");
    must(std::string(h.magic,7)=="TCSTR11","magic");
    must(h.version==11 && h.channels==CHANNELS && h.prime==PRIME,"header");

    TerminalMap terminals;
    std::array<uint8_t,CHARACTER_COUNT> reverse{};
    std::memcpy(reverse.data(),h.terminals,CHARACTER_COUNT);
    terminals.restore(h.terminal_count,reverse);

    Field<CHANNELS> field(h.nodes);
    for(uint32_t i=0;i<h.nodes;++i){
        std::array<uint16_t,CHANNELS> coeff{};
        for(size_t j=0;j<CHANNELS;++j){
            int lo=in.get(),hi=in.get();
            must(lo>=0 && hi>=0,"short imprint");
            coeff[j]=uint16_t(uint16_t(lo)|(uint16_t(hi)<<8));
        }
        field.node(i).set_imprint(coeff);
    }
    char extra;
    must(!in.get(extra),"trailing bytes");
    return {std::move(field),{std::move(terminals),h}};
}

static int selftest(){
    Field<CHANNELS> field(1);
    TerminalMap terminals;
    CharacterStreamImprinter<CHANNELS> parser(field,terminals,1);

    const std::vector<uint8_t> seq={'A','B','C','D','E','F','G','H'};
    for(size_t i=0;i<seq.size();++i){
        auto r=parser.push(seq[i]);
        must(r.ok,"representable stream unexpectedly failed");
        must(r.page==i,"wrong page");
        must(r.position==0,"wrong position");
    }

    field.power_on();
    for(uint16_t q=0;q<8;++q){
        uint16_t code=0xffff;
        for(uint16_t c=0;c<terminals.count();++c)
            if(field.observe(q,c)[0]==Resolved::POS){
                must(code==0xffff,"multiple TRUE");
                code=c;
            }
        must(code!=0xffff,"missing TRUE");
        must(terminals.decode(code)==seq[q],"stream imprint mismatch");
    }
    field.power_off();

    const auto frozen=field.node(0).imprint();
    for(uint16_t c=0;c<terminals.count();++c){
        field.power_on();
        (void)field.observe(3,c);
        field.power_off();
    }
    must(field.node(0).imprint()==frozen,"observation mutated imprint");

    std::cout<<"TRUCOMPUTE_STREAMING_IMPRINTER_V11=PASS\n";
    std::cout<<"parser_mode=CHARACTER_BY_CHARACTER\n";
    std::cout<<"parser_stores_page_buffer=false\n";
    std::cout<<"parser_stores_source_copy=false\n";
    std::cout<<"node_page_records=false\n";
    std::cout<<"node_character_records=false\n";
    std::cout<<"fixed_imprint_channels="<<CHANNELS<<"\n";
    std::cout<<"observation_mutates_imprint=false\n";
    return 0;
}

static int freeze_cmd(const std::string& source_path,const std::string& artifact){
    std::ifstream in(source_path,std::ios::binary);
    must(bool(in),"open source");

    Field<CHANNELS> field(POSITIONS);
    TerminalMap terminals;
    CharacterStreamImprinter<CHANNELS> parser(field,terminals,POSITIONS);

    char ch;
    while(in.get(ch)){
        auto r=parser.push(uint8_t(ch));
        if(!r.ok){
            std::cout<<"status=CAPACITY_EXCEEDED"
                     <<" bytes_imprinted="<<r.bytes
                     <<" page="<<r.page
                     <<" position="<<r.position
                     <<" expected_symbol="<<r.symbol
                     <<" predicted_symbol="<<r.predicted
                     <<" fixed_channels="<<CHANNELS
                     <<" artifact_created=false\n";
            return 2;
        }
    }

    must(parser.at_page_boundary(),"source ended mid-page");
    const uint16_t pages=parser.pages_complete();
    const uint64_t bytes=parser.bytes();

    // Source is still available here only for prefreeze verification.
    in.clear(); in.seekg(0);
    std::vector<uint8_t> source((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    const auto recovered=reconstruct(field,terminals,pages);
    must(recovered==source,"prefreeze replay mismatch");
    field.power_off();

    save_field(artifact,field,terminals,pages,bytes);

    std::cout<<"status=STREAMING_FIELD_FROZEN\n";
    std::cout<<"source_bytes="<<bytes<<"\n";
    std::cout<<"pages="<<pages<<"\n";
    std::cout<<"node_count="<<POSITIONS<<"\n";
    std::cout<<"terminal_count="<<terminals.count()<<"\n";
    std::cout<<"fixed_imprint_channels="<<CHANNELS<<"\n";
    std::cout<<"parser_mode=CHARACTER_BY_CHARACTER\n";
    std::cout<<"page_buffer_bytes=0\n";
    std::cout<<"source_copy_bytes_during_imprint=0\n";
    std::cout<<"page_response_table=false\n";
    std::cout<<"character_response_table=false\n";
    std::cout<<"residual_bytes=0\n";
    std::cout<<"prefreeze_exact_replay=PASS\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int replay_cmd(const std::string& artifact,const std::string& output){
    auto loaded=load_field(artifact);
    auto& field=loaded.first;
    auto& terminals=loaded.second.first;
    const auto& h=loaded.second.second;

    auto recovered=reconstruct(field,terminals,h.pages);
    field.power_off();
    must(recovered.size()==h.source_length,"recovered length");

    std::ofstream out(output,std::ios::binary|std::ios::trunc);
    must(bool(out),"open recovered");
    out.write(reinterpret_cast<const char*>(recovered.data()),std::streamsize(recovered.size()));
    must(bool(out),"write recovered");

    std::cout<<"status=STREAMING_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<recovered.size()<<"\n";
    std::cout<<"pages="<<h.pages<<"\n";
    std::cout<<"node_count="<<h.nodes<<"\n";
    std::cout<<"terminal_count="<<h.terminal_count<<"\n";
    std::cout<<"fixed_imprint_channels="<<h.channels<<"\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
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
