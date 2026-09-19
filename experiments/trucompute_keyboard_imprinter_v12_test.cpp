#include "../trucompute/trucompute_keyboard_imprinter_v12.hpp"
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

using namespace trucompute_keyboard_v12;

static constexpr size_t CHANNELS=8;
static constexpr uint32_t POSITIONS=64;

static void must(bool x,const char* m){ if(!x) throw std::runtime_error(m); }

#pragma pack(push,1)
struct Header{
    char magic[8];
    uint16_t version;
    uint16_t channels;
    uint16_t prime;
    uint16_t physical_character_buttons;
    uint16_t labelled_buttons;
    uint16_t pages;
    uint32_t nodes;
    uint64_t source_length;
    uint8_t button_labels[CHARACTER_BUTTONS];
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

        // All 206 physical character buttons are interrogated, not a lookup map.
        for(uint16_t button=0;button<CHARACTER_BUTTONS;++button){
            auto responses=field.observe(q,button);
            for(size_t i=0;i<responses.size();++i){
                if(responses[i]==Resolved::POS){
                    ++yes[i];
                    winner[i]=int16_t(button);
                }else{
                    must(responses[i]==Resolved::NEG,"nonboolean node result");
                }
            }
        }

        for(size_t i=0;i<field.node_count();++i){
            must(yes[i]==1,"position must have exactly one TRUE button");
            out.push_back(keyboard.read_label(uint16_t(winner[i])));
        }
    }

    return out;
}

static void save_machine(const std::string& path,
                         const Field<CHANNELS>& field,
                         const CharacterKeyboard& keyboard,
                         uint16_t pages,
                         uint64_t source_length){
    Header h{};
    std::memcpy(h.magic,"TCKBD12",7);
    h.version=12;
    h.channels=CHANNELS;
    h.prime=PRIME;
    h.physical_character_buttons=CHARACTER_BUTTONS;
    h.labelled_buttons=keyboard.labelled_count();
    h.pages=pages;
    h.nodes=uint32_t(field.node_count());
    h.source_length=source_length;

    for(uint16_t i=0;i<CHARACTER_BUTTONS;++i)
        h.button_labels[i]=keyboard.press(i).labelled() ? keyboard.press(i).label() : 0;

    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    must(bool(out),"open machine artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));
    for(const auto& n:field.nodes()){
        for(auto v:n.imprint()){
            out.put(char(uint8_t(v&255)));
            out.put(char(uint8_t((v>>8)&255)));
        }
    }
    must(bool(out),"write machine artifact");
}

static std::pair<Field<CHANNELS>,std::pair<CharacterKeyboard,Header>>
load_machine(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    must(bool(in),"open machine artifact");

    Header h{};
    in.read(reinterpret_cast<char*>(&h),sizeof(h));
    must(bool(in),"read header");
    must(std::string(h.magic,7)=="TCKBD12","magic");
    must(h.version==12 && h.channels==CHANNELS && h.prime==PRIME &&
         h.physical_character_buttons==CHARACTER_BUTTONS,"architecture header");

    CharacterKeyboard keyboard;
    std::array<uint8_t,CHARACTER_BUTTONS> labels{};
    std::memcpy(labels.data(),h.button_labels,CHARACTER_BUTTONS);
    keyboard.restore(h.labelled_buttons,labels);

    Field<CHANNELS> field(h.nodes);
    for(uint32_t i=0;i<h.nodes;++i){
        std::array<uint16_t,CHANNELS> coeff{};
        for(size_t j=0;j<CHANNELS;++j){
            int lo=in.get(),hi=in.get();
            must(lo>=0 && hi>=0,"short node imprint");
            coeff[j]=uint16_t(uint16_t(lo)|(uint16_t(hi)<<8));
        }
        field.node(i).set_imprint(coeff);
    }
    char extra;
    must(!in.get(extra),"trailing bytes");
    return {std::move(field),{std::move(keyboard),h}};
}

static int selftest(){
    Field<CHANNELS> field(1);
    CharacterKeyboard keyboard;
    KeyboardImprinter<CHANNELS> writer(field,keyboard,1);

    const std::vector<uint8_t> text={'A','B','C','D','E','F','G','H'};
    for(size_t i=0;i<text.size();++i){
        auto r=writer.type(text[i]);
        must(r.ok,"typing failed");
        must(r.page==i,"wrong page");
        must(r.position==0,"wrong position");
    }

    must(keyboard.labelled_count()==8,"expected 8 tuned keys");
    must(CharacterKeyboard::physical_button_count()==206,"must have 206 physical buttons");

    const auto before=field.node(0).imprint();
    const auto recovered=reconstruct(field,keyboard,8);
    must(recovered==text,"keyboard replay mismatch");
    must(field.node(0).imprint()==before,"readout changed hand tuning");
    field.power_off();

    std::cout<<"TRUCOMPUTE_KEYBOARD_IMPRINTER_V12=PASS\n";
    std::cout<<"writer_interface=SAME_206_CHARACTER_BUTTONS_AS_READER\n";
    std::cout<<"physical_character_buttons=206\n";
    std::cout<<"separate_terminal_map=false\n";
    std::cout<<"parser_mode=READ_ONE_CHARACTER_THEN_PRESS_KEY\n";
    std::cout<<"parser_stores_source_copy=false\n";
    std::cout<<"parser_stores_page_buffer=false\n";
    std::cout<<"node_page_records=false\n";
    std::cout<<"node_character_records=false\n";
    std::cout<<"finished_product=HAND_TUNED_MACHINE\n";
    return 0;
}

static int freeze_cmd(const std::string& source_path,const std::string& artifact){
    std::ifstream in(source_path,std::ios::binary);
    must(bool(in),"open source");

    Field<CHANNELS> field(POSITIONS);
    CharacterKeyboard keyboard;
    KeyboardImprinter<CHANNELS> writer(field,keyboard,POSITIONS);

    char ch;
    while(in.get(ch)){
        auto r=writer.type(uint8_t(ch));
        if(!r.ok){
            std::cout<<"status=CAPACITY_EXCEEDED"
                     <<" bytes_imprinted="<<r.bytes_accepted
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
    const uint64_t bytes=writer.bytes();

    // Prefreeze verification only; source is not retained in the machine.
    in.clear(); in.seekg(0);
    std::vector<uint8_t> source((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    const auto recovered=reconstruct(field,keyboard,pages);
    must(recovered==source,"prefreeze exact replay mismatch");
    field.power_off();

    save_machine(artifact,field,keyboard,pages,bytes);

    std::cout<<"status=HAND_TUNED_MACHINE_FROZEN\n";
    std::cout<<"source_bytes="<<bytes<<"\n";
    std::cout<<"pages="<<pages<<"\n";
    std::cout<<"node_count="<<POSITIONS<<"\n";
    std::cout<<"physical_character_buttons=206\n";
    std::cout<<"labelled_character_buttons="<<keyboard.labelled_count()<<"\n";
    std::cout<<"separate_terminal_map=false\n";
    std::cout<<"writer_interface=KEYBOARD\n";
    std::cout<<"reader_interface=KEYBOARD\n";
    std::cout<<"page_buffer_bytes=0\n";
    std::cout<<"source_copy_bytes_during_imprint=0\n";
    std::cout<<"page_response_table=false\n";
    std::cout<<"character_response_table=false\n";
    std::cout<<"residual_bytes=0\n";
    std::cout<<"finished_product=HAND_TUNED_MACHINE\n";
    std::cout<<"prefreeze_exact_replay=PASS\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int replay_cmd(const std::string& artifact,const std::string& output){
    auto loaded=load_machine(artifact);
    auto& field=loaded.first;
    auto& keyboard=loaded.second.first;
    const auto& h=loaded.second.second;

    const auto recovered=reconstruct(field,keyboard,h.pages);
    field.power_off();
    must(recovered.size()==h.source_length,"recovered length");

    std::ofstream out(output,std::ios::binary|std::ios::trunc);
    must(bool(out),"open output");
    out.write(reinterpret_cast<const char*>(recovered.data()),std::streamsize(recovered.size()));
    must(bool(out),"write output");

    std::cout<<"status=HAND_TUNED_MACHINE_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<recovered.size()<<"\n";
    std::cout<<"physical_character_buttons=206\n";
    std::cout<<"labelled_character_buttons="<<keyboard.labelled_count()<<"\n";
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
