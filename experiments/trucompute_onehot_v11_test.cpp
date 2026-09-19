#include "../trucompute/trucompute_onehot_v11.hpp"
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

using namespace trucompute_onehot_v11;

static constexpr size_t CHANNELS=8;
static constexpr uint32_t POSITIONS=64;

static void must(bool x,const char* m){ if(!x) throw std::runtime_error(m); }

#pragma pack(push,1)
struct Header {
    char magic[8];
    uint16_t version;
    uint16_t channels;
    uint16_t page_buttons;
    uint16_t character_buttons;
    uint16_t pages;
    uint16_t reserved;
    uint32_t positions;
    uint64_t source_length;
};
#pragma pack(pop)

static std::vector<uint8_t> reconstruct(Machine<CHANNELS>& m,uint16_t pages){
    m.power_on();
    std::vector<uint8_t> out;
    out.reserve(uint64_t(pages)*m.data_node_count());

    for(uint16_t q=0;q<pages;++q){
        m.press_page(q);
        must(m.page_buttons().true_count()==1,"page bank not one-hot");
        must(m.page_buttons().false_count()==PAGE_BUTTONS-1,"page false count");

        std::vector<uint16_t> yes(m.data_node_count(),0);
        std::vector<int16_t> winner(m.data_node_count(),-1);

        for(uint16_t c=0;c<CHARACTER_BUTTONS;++c){
            m.press_character(c);
            must(m.character_buttons().true_count()==1,"character bank not one-hot");
            must(m.character_buttons().false_count()==CHARACTER_BUTTONS-1,"character false count");

            const auto responses=m.observe();
            must(responses.size()==m.data_node_count(),"response width");

            for(size_t i=0;i<responses.size();++i){
                if(responses[i]==Truth::TRUE_VALUE){
                    ++yes[i];
                    winner[i]=int16_t(c);
                }else{
                    must(responses[i]==Truth::FALSE_VALUE,"data node emitted non-boolean");
                }
            }
        }

        for(size_t i=0;i<m.data_node_count();++i){
            must(yes[i]==1,"each position must have one true character");
            out.push_back(uint8_t(winner[i]));
        }
    }
    return out;
}

static void save_machine(const std::string& path,const Machine<CHANNELS>& m,uint16_t pages,uint64_t source_length){
    Header h{};
    std::memcpy(h.magic,"TC1HOT11",8);
    h.version=11;
    h.channels=CHANNELS;
    h.page_buttons=PAGE_BUTTONS;
    h.character_buttons=CHARACTER_BUTTONS;
    h.pages=pages;
    h.positions=uint32_t(m.data_node_count());
    h.source_length=source_length;

    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));

    for(const auto& n:m.data_nodes()){
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
    must(bool(in),"read header");
    must(std::string(h.magic,8)=="TC1HOT11","magic");
    must(h.version==11 && h.channels==CHANNELS &&
         h.page_buttons==PAGE_BUTTONS &&
         h.character_buttons==CHARACTER_BUTTONS,
         "architecture header");

    Machine<CHANNELS> m(h.positions);
    for(uint32_t i=0;i<h.positions;++i){
        std::array<uint16_t,CHANNELS> coeff{};
        for(size_t k=0;k<CHANNELS;++k){
            int lo=in.get(),hi=in.get();
            must(lo>=0 && hi>=0,"short imprint");
            coeff[k]=uint16_t(uint16_t(lo)|(uint16_t(hi)<<8));
            must(coeff[k]<PRIME,"invalid coefficient");
        }
        m.data_nodes()[i].set_imprint(coeff);
    }

    char extra;
    must(!in.get(extra),"trailing bytes");
    return {std::move(m),h};
}

static int selftest(){
    OneHotButtonBank<8> bank;
    bank.press(2);
    must(bank.label(2)==Truth::TRUE_VALUE,"pressed button not true");
    must(bank.true_count()==1 && bank.false_count()==7,"one-hot first press");
    bank.press(6);
    must(bank.label(6)==Truth::TRUE_VALUE,"new button not true");
    must(bank.label(2)==Truth::FALSE_VALUE,"old true button not forced false");
    must(bank.true_count()==1 && bank.false_count()==7,"one-hot second press");

    Machine<CHANNELS> m(3);
    m.power_on();
    m.press_page(12);
    m.press_character(42);
    must(m.page_buttons().true_count()==1,"page bank one-hot");
    must(m.page_buttons().false_count()==999,"page false labels");
    must(m.character_buttons().true_count()==1,"character bank one-hot");
    must(m.character_buttons().false_count()==205,"character false labels");

    std::array<uint16_t,CHANNELS> constant{};
    constant[0]=42;
    for(auto& n:m.data_nodes()) n.set_imprint(constant);

    auto before=m.data_nodes()[0].imprint();
    auto r=m.observe();
    for(auto x:r) must(x==Truth::TRUE_VALUE,"matching probe should be true");

    m.press_character(41);
    r=m.observe();
    for(auto x:r) must(x==Truth::FALSE_VALUE,"new button must turn prior truth false");
    must(m.character_buttons().label(42)==Truth::FALSE_VALUE,"previous character button not false");
    must(m.character_buttons().label(41)==Truth::TRUE_VALUE,"new character button not true");
    must(m.data_nodes()[0].imprint()==before,"button press mutated source imprint");

    std::cout<<"TRUCOMPUTE_ONEHOT_FIELD_V11=PASS\n";
    std::cout<<"page_button_true_count=1\n";
    std::cout<<"page_button_false_count=999\n";
    std::cout<<"character_button_true_count=1\n";
    std::cout<<"character_button_false_count=205\n";
    std::cout<<"data_node_outputs=TRUE_OR_FALSE_ONLY\n";
    std::cout<<"button_press_forces_peer_labels_false=PASS\n";
    std::cout<<"buttons_are_runtime_control_not_source_artifact=PASS\n";
    std::cout<<"data_imprint_mutated_by_button_press=false\n";
    std::cout<<"page_response_table=false\n";
    std::cout<<"character_response_table=false\n";
    return 0;
}

static int freeze_cmd(const std::string& source_path,const std::string& artifact){
    std::ifstream in(source_path,std::ios::binary);
    must(bool(in),"open source");
    std::vector<uint8_t> source((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    must(!source.empty() && source.size()%POSITIONS==0,"source page geometry");
    const uint64_t pages64=source.size()/POSITIONS;
    must(pages64<=PAGE_BUTTONS,"too many pages");
    const uint16_t pages=uint16_t(pages64);

    Machine<CHANNELS> m(POSITIONS);
    const auto fail=FixedWidthImprinter<CHANNELS>::imprint_exact(m.data_nodes(),source,POSITIONS);
    if(fail.failed){
        std::cout<<"status=CAPACITY_EXCEEDED"
                 <<" node="<<fail.node
                 <<" page="<<fail.page
                 <<" expected="<<fail.expected
                 <<" predicted="<<fail.predicted
                 <<" fixed_channels="<<CHANNELS
                 <<" artifact_created=false\n";
        return 2;
    }

    const auto recovered=reconstruct(m,pages);
    must(recovered==source,"prefreeze exact replay");
    m.power_off();

    save_machine(artifact,m,pages,source.size());

    std::cout<<"status=ONEHOT_NATIVE_FIELD_FROZEN\n";
    std::cout<<"source_bytes="<<source.size()<<"\n";
    std::cout<<"pages="<<pages<<"\n";
    std::cout<<"positions="<<POSITIONS<<"\n";
    std::cout<<"fixed_imprint_channels="<<CHANNELS<<"\n";
    std::cout<<"page_buttons_persisted_in_artifact=false\n";
    std::cout<<"character_buttons_persisted_in_artifact=false\n";
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

    const auto recovered=reconstruct(m,h.pages);
    m.power_off();
    must(recovered.size()==h.source_length,"recovered size");

    std::ofstream out(outpath,std::ios::binary|std::ios::trunc);
    must(bool(out),"open recovered");
    out.write(reinterpret_cast<const char*>(recovered.data()),std::streamsize(recovered.size()));
    must(bool(out),"write recovered");

    std::cout<<"status=ONEHOT_NATIVE_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<recovered.size()<<"\n";
    std::cout<<"page_buttons_true="<<m.page_buttons().true_count()<<"\n";
    std::cout<<"page_buttons_false="<<m.page_buttons().false_count()<<"\n";
    std::cout<<"character_buttons_true="<<m.character_buttons().true_count()<<"\n";
    std::cout<<"character_buttons_false="<<m.character_buttons().false_count()<<"\n";
    std::cout<<"data_outputs_nonboolean=0\n";
    std::cout<<"final_machine_power=OFF\n";
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
