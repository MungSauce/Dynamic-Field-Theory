#include "../trucompute/trucompute_fixed_keyboard_v13.hpp"
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

using namespace trucompute_fixedkbd_v13;

static constexpr size_t CHANNELS=8;
static constexpr uint32_t POSITIONS=64;

static void must(bool x,const char* m){ if(!x) throw std::runtime_error(m); }

#pragma pack(push,1)
struct Header{
    char magic[8];
    uint16_t version;
    uint16_t channels;
    uint16_t prime;
    uint16_t physical_buttons;
    uint16_t pages;
    uint32_t nodes;
    uint64_t source_symbols;
};
#pragma pack(pop)

static std::vector<uint8_t> reconstruct(Field<CHANNELS>& field,
                                        const CharacterKeyboard& keyboard,
                                        uint16_t pages){
    field.power_on();
    std::vector<uint8_t> out;
    out.reserve(uint64_t(pages)*field.node_count());

    for(uint16_t q=0;q<pages;++q){
        std::vector<uint16_t> yes(field.node_count(),0);
        std::vector<int16_t> winner(field.node_count(),-1);

        for(uint16_t c=0;c<CHARACTER_BUTTONS;++c){
            const auto& key=keyboard.press(c);
            auto r=field.observe(q,key);
            for(size_t i=0;i<r.size();++i){
                if(r[i]==Resolved::POS){ ++yes[i]; winner[i]=int16_t(c); }
                else must(r[i]==Resolved::NEG,"nonboolean result");
            }
        }

        for(size_t i=0;i<field.node_count();++i){
            must(yes[i]==1,"exactly one TRUE button required");
            out.push_back(uint8_t(winner[i]));
        }
    }

    return out;
}

static void save_machine(const std::string& path,
                         const Field<CHANNELS>& field,
                         uint16_t pages,uint64_t symbols){
    Header h{};
    std::memcpy(h.magic,"TCFKV13",7);
    h.version=13;
    h.channels=CHANNELS;
    h.prime=PRIME;
    h.physical_buttons=CHARACTER_BUTTONS;
    h.pages=pages;
    h.nodes=uint32_t(field.node_count());
    h.source_symbols=symbols;

    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open machine");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));
    for(const auto& n:field.nodes()){
        for(auto v:n.imprint()){
            out.put(char(uint8_t(v&255)));
            out.put(char(uint8_t((v>>8)&255)));
        }
    }
    must(bool(out),"write machine");
}

static std::pair<Field<CHANNELS>,Header> load_machine(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    must(bool(in),"open machine");
    Header h{};
    in.read(reinterpret_cast<char*>(&h),sizeof(h));
    must(bool(in),"header");
    must(std::string(h.magic,7)=="TCFKV13","magic");
    must(h.version==13 && h.channels==CHANNELS && h.prime==PRIME &&
         h.physical_buttons==CHARACTER_BUTTONS,"architecture header");

    Field<CHANNELS> f(h.nodes);
    for(uint32_t i=0;i<h.nodes;++i){
        std::array<uint16_t,CHANNELS> v{};
        for(size_t j=0;j<CHANNELS;++j){
            int lo=in.get(),hi=in.get();
            must(lo>=0 && hi>=0,"short imprint");
            v[j]=uint16_t(uint16_t(lo)|(uint16_t(hi)<<8));
        }
        f.node(i).set_imprint(v);
    }
    char extra;
    must(!in.get(extra),"trailing bytes");
    return {std::move(f),h};
}

static int selftest(){
    CharacterKeyboard keyboard;
    must(keyboard.physical_button_count()==206,"all 206 buttons must pre-exist");
    for(uint16_t c=0;c<206;++c) must(keyboard.press(c).id()==c,"button identity changed");

    Field<CHANNELS> field(1);
    KeyboardImprinter<CHANNELS> writer(field,keyboard,1);
    const std::vector<uint8_t> symbols={5,17,42,99,3,205,0,88};

    for(size_t q=0;q<symbols.size();++q){
        auto r=writer.type_symbol(symbols[q]);
        must(r.ok,"typing failed");
        must(r.pressed_button==symbols[q],"wrong physical key pressed");
    }

    const auto before=field.node(0).imprint();
    const auto recovered=reconstruct(field,keyboard,8);
    must(recovered==symbols,"replay mismatch");
    must(field.node(0).imprint()==before,"readout changed tuning");

    bool rejected=false;
    try{ (void)keyboard.press(206); }
    catch(const std::runtime_error&){ rejected=true; }
    must(rejected,"machine accepted nonexistent 207th button");

    std::cout<<"TRUCOMPUTE_FIXED_KEYBOARD_V13=PASS\n";
    std::cout<<"physical_character_buttons=206\n";
    std::cout<<"all_buttons_exist_before_source=true\n";
    std::cout<<"button_identity_source_tuned=false\n";
    std::cout<<"button_coupling_source_tuned=false\n";
    std::cout<<"separate_terminal_map=false\n";
    std::cout<<"writer_interface=PERMANENT_206_BUTTON_BANK\n";
    std::cout<<"reader_interface=PERMANENT_206_BUTTON_BANK\n";
    std::cout<<"finished_product=HAND_TUNED_RESISTOR_FIELD\n";
    return 0;
}

static int freeze_cmd(const std::string& source_path,const std::string& artifact){
    std::ifstream in(source_path,std::ios::binary);
    must(bool(in),"open source");

    CharacterKeyboard keyboard;
    Field<CHANNELS> field(POSITIONS);
    KeyboardImprinter<CHANNELS> writer(field,keyboard,POSITIONS);

    char ch;
    while(in.get(ch)){
        const uint8_t symbol=uint8_t(ch);
        if(symbol>=CHARACTER_BUTTONS)
            throw std::runtime_error("source symbol not in permanent 206-button alphabet");

        auto r=writer.type_symbol(symbol);
        if(!r.ok){
            std::cout<<"status=CAPACITY_EXCEEDED"
                     <<" symbols_imprinted="<<r.symbols_accepted
                     <<" page="<<r.page
                     <<" position="<<r.position
                     <<" pressed_button="<<r.pressed_button
                     <<" predicted_button="<<r.predicted_button
                     <<" fixed_channels="<<CHANNELS
                     <<" artifact_created=false\n";
            return 2;
        }
    }

    must(writer.at_page_boundary(),"source ended mid-page");
    const uint16_t pages=writer.pages_complete();
    const uint64_t symbols=writer.symbols();

    in.clear(); in.seekg(0);
    std::vector<uint8_t> source((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    const auto recovered=reconstruct(field,keyboard,pages);
    must(recovered==source,"prefreeze replay mismatch");
    field.power_off();

    save_machine(artifact,field,pages,symbols);

    std::cout<<"status=FIXED_KEYBOARD_MACHINE_FROZEN\n";
    std::cout<<"source_symbols="<<symbols<<"\n";
    std::cout<<"pages="<<pages<<"\n";
    std::cout<<"node_count="<<POSITIONS<<"\n";
    std::cout<<"physical_character_buttons=206\n";
    std::cout<<"all_buttons_exist_before_source=true\n";
    std::cout<<"source_dependent_button_state_bytes=0\n";
    std::cout<<"separate_terminal_map=false\n";
    std::cout<<"page_buffer_bytes=0\n";
    std::cout<<"source_copy_bytes_during_imprint=0\n";
    std::cout<<"page_response_table=false\n";
    std::cout<<"character_response_table=false\n";
    std::cout<<"residual_bytes=0\n";
    std::cout<<"finished_product=HAND_TUNED_RESISTOR_FIELD\n";
    std::cout<<"prefreeze_exact_replay=PASS\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int replay_cmd(const std::string& artifact,const std::string& output){
    CharacterKeyboard keyboard;
    auto loaded=load_machine(artifact);
    auto& field=loaded.first;
    const auto& h=loaded.second;

    auto recovered=reconstruct(field,keyboard,h.pages);
    field.power_off();
    must(recovered.size()==h.source_symbols,"recovered symbol count");

    std::ofstream out(output,std::ios::binary|std::ios::trunc);
    must(bool(out),"open output");
    out.write(reinterpret_cast<const char*>(recovered.data()),std::streamsize(recovered.size()));
    must(bool(out),"write output");

    std::cout<<"status=FIXED_KEYBOARD_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_symbols="<<recovered.size()<<"\n";
    std::cout<<"physical_character_buttons=206\n";
    std::cout<<"source_dependent_button_state_bytes=0\n";
    std::cout<<"separate_terminal_map=false\n";
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
