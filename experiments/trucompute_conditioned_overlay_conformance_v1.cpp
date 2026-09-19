#include "../trucompute/trucompute_native_field_v5.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace trucompute_field;

static constexpr uint32_t PAGE_SPAN = 64;
static constexpr uint16_t CHARACTER_COUNT = 206;

#pragma pack(push,1)
struct Header {
    char magic[8];
    uint16_t version;
    uint16_t characters;
    uint32_t page_span;
    uint64_t source_length;
    uint64_t overlay_nodes;
};

struct OverlayRecord {
    uint16_t page;
    uint32_t position;
    uint16_t symbol;
};
#pragma pack(pop)

static void must(bool ok,const char* msg){
    if(!ok) throw std::runtime_error(msg);
}

// Native custom node type used by the conformance fixture.
// It is simultaneously a page tint, position carrier, and character runner.
// It does not mutate when selectors change.
class ConditionedSymbolNode final : public Node {
    uint16_t page_;
    uint32_t position_;
    uint16_t symbol_;
public:
    ConditionedSymbolNode(uint16_t page,uint32_t position,uint16_t symbol)
        : page_(page),position_(position),symbol_(symbol){
        if(symbol_>=CHARACTER_COUNT) throw std::runtime_error("symbol outside 206-character field");
    }

    Contribution participate(const Selector& s) const override {
        if(!s.page_active || !s.position_active || !s.character_active)
            return Contribution::none();
        if(s.page!=page_ || s.position!=position_)
            return Contribution::none();
        return s.character==symbol_ ? Contribution::pos() : Contribution::neg();
    }

    const char* native_type() const override {
        return "CONDITIONED_SYMBOL_NODE";
    }

    OverlayRecord record() const {
        return {page_,position_,symbol_};
    }
};

static uint16_t winning_character(const Field& field,uint16_t page,uint32_t position){
    int yes=0;
    uint16_t winner=0;
    int no=0,neither=0,balanced=0;

    for(uint16_t c=0;c<CHARACTER_COUNT;++c){
        Selector s{};
        s.page=page;
        s.position=position;
        s.character=c;
        s.page_active=true;
        s.position_active=true;
        s.character_active=true;

        Observation o=field.observe(s);
        if(o==Observation::POS){ ++yes; winner=c; }
        else if(o==Observation::NEG){ ++no; }
        else if(o==Observation::NEITHER){ ++neither; }
        else if(o==Observation::STATELESS_ZERO){ ++balanced; }
    }

    must(yes==1,"expected exactly one TRUE");
    must(no==CHARACTER_COUNT-1,"expected 205 FALSE responses");
    must(neither==0,"valid position produced NEITHER");
    must(balanced==0,"valid position produced STATELESS_ZERO");
    return winner;
}

static int selftest(){
    Field f;

    // Same physical field, same position, mutually different page overlays.
    f.emplace<ConditionedSymbolNode>(0,7,42);
    f.emplace<ConditionedSymbolNode>(1,7,99);
    f.emplace<ConditionedSymbolNode>(2,7,17);
    f.emplace<ConditionedSymbolNode>(3,7,42);

    const size_t before=f.size();

    must(winning_character(f,0,7)==42,"page 0 overlay");
    must(winning_character(f,1,7)==99,"page 1 overlay");
    must(winning_character(f,2,7)==17,"page 2 overlay");
    must(winning_character(f,3,7)==42,"page 3 overlay");

    // Switch backwards/forwards to prove selector order does not rewrite field.
    must(winning_character(f,1,7)==99,"page 1 revisit");
    must(winning_character(f,0,7)==42,"page 0 revisit");
    must(winning_character(f,3,7)==42,"page 3 revisit");
    must(f.size()==before,"selector mutation changed node population");

    // Wrong position: all overlay nodes remain present but none participates.
    Selector absent{};
    absent.page=0;
    absent.position=8;
    absent.character=42;
    absent.page_active=absent.position_active=absent.character_active=true;
    must(f.observe(absent)==Observation::NEITHER,"non-addressed position must be NEITHER");

    std::cout<<"CONDITIONED_OVERLAY_SELFTEST=PASS\n";
    std::cout<<"same_position_multi_page_coexistence=PASS\n";
    std::cout<<"different_symbols_same_space_no_contradiction=PASS\n";
    std::cout<<"selector_switch_reversible=PASS\n";
    std::cout<<"selector_switch_mutates_field=false\n";
    std::cout<<"exact_one_true_per_valid_position=PASS\n";
    std::cout<<"false_answers_per_valid_position=205\n";
    std::cout<<"field_nodes_before="<<before<<"\n";
    std::cout<<"field_nodes_after="<<f.size()<<"\n";
    return 0;
}

static int freeze_field(const std::string& source,const std::string& artifact){
    std::ifstream in(source,std::ios::binary);
    must(bool(in),"open source");

    std::vector<OverlayRecord> records;
    char ch;
    uint64_t t=0;
    while(in.get(ch)){
        uint16_t symbol=uint8_t(ch);
        must(symbol<CHARACTER_COUNT,"fixture byte outside 0..205");
        OverlayRecord r{
            uint16_t(t/PAGE_SPAN),
            uint32_t(t%PAGE_SPAN),
            symbol
        };
        records.push_back(r);
        ++t;
    }

    Field field;
    for(const auto& r:records)
        field.emplace<ConditionedSymbolNode>(r.page,r.position,r.symbol);

    // Pre-freeze selector proof over all positions.
    for(uint64_t i=0;i<records.size();++i){
        const auto& r=records[size_t(i)];
        must(winning_character(field,r.page,r.position)==r.symbol,"prefreeze exact replay");
    }
    must(field.size()==records.size(),"field node count changed during prefreeze replay");

    Header h{};
    std::memcpy(h.magic,"TCOVLY1",7);
    h.version=1;
    h.characters=CHARACTER_COUNT;
    h.page_span=PAGE_SPAN;
    h.source_length=records.size();
    h.overlay_nodes=records.size();

    std::ofstream out(artifact,std::ios::binary|std::ios::trunc);
    must(bool(out),"open artifact");
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));
    out.write(reinterpret_cast<const char*>(records.data()),
              std::streamsize(records.size()*sizeof(OverlayRecord)));
    must(bool(out),"write artifact");
    out.close();

    std::cout<<"status=OVERLAY_FIELD_FROZEN\n";
    std::cout<<"source_bytes="<<records.size()<<"\n";
    std::cout<<"overlay_nodes="<<records.size()<<"\n";
    std::cout<<"single_field=true\n";
    std::cout<<"page_bank=false\n";
    std::cout<<"selector_tables=false\n";
    std::cout<<"prefreeze_exact_replay=PASS\n";
    std::cout<<"artifact_bytes="<<std::filesystem::file_size(artifact)<<"\n";
    return 0;
}

static int replay_field(const std::string& artifact,const std::string& recovered){
    std::ifstream in(artifact,std::ios::binary);
    must(bool(in),"open artifact");

    Header h{};
    in.read(reinterpret_cast<char*>(&h),sizeof(h));
    must(bool(in),"read header");
    must(std::string(h.magic,7)=="TCOVLY1","magic");
    must(h.version==1,"version");
    must(h.characters==CHARACTER_COUNT,"character count");
    must(h.page_span==PAGE_SPAN,"page span");
    must(h.overlay_nodes==h.source_length,"overlay node count");

    std::vector<OverlayRecord> records(size_t(h.overlay_nodes));
    in.read(reinterpret_cast<char*>(records.data()),
            std::streamsize(records.size()*sizeof(OverlayRecord)));
    must(bool(in),"read records");
    char trailing;
    must(!in.read(&trailing,1),"trailing bytes");

    Field field;
    for(const auto& r:records){
        must(r.symbol<CHARACTER_COUNT,"record symbol");
        must(r.position<PAGE_SPAN,"record position");
        field.emplace<ConditionedSymbolNode>(r.page,r.position,r.symbol);
    }

    const size_t population_before=field.size();

    std::ofstream out(recovered,std::ios::binary|std::ios::trunc);
    must(bool(out),"open recovered");

    uint64_t positives=0,negatives=0;
    for(uint64_t t=0;t<h.source_length;++t){
        uint16_t page=uint16_t(t/PAGE_SPAN);
        uint32_t pos=uint32_t(t%PAGE_SPAN);

        int yes=0;
        uint16_t winner=0;
        for(uint16_t c=0;c<CHARACTER_COUNT;++c){
            Selector s{};
            s.page=page;
            s.position=pos;
            s.character=c;
            s.page_active=s.position_active=s.character_active=true;
            Observation o=field.observe(s);
            if(o==Observation::POS){++yes;++positives;winner=c;}
            else if(o==Observation::NEG){++negatives;}
            else throw std::runtime_error("valid replay query was not POS/NEG");
        }
        must(yes==1,"replay position did not have exactly one TRUE");
        out.put(char(uint8_t(winner)));
    }
    out.close();

    must(field.size()==population_before,"selector sweeps mutated field population");

    // Revisit pages out of order after full replay.
    if(h.source_length>=PAGE_SPAN*3){
        for(uint16_t page: {uint16_t(2),uint16_t(0),uint16_t(1),uint16_t(2)}){
            const uint64_t t=uint64_t(page)*PAGE_SPAN+7;
            if(t<h.source_length){
                const uint16_t got=winning_character(field,page,7);
                must(got==records[size_t(t)].symbol,"selector revisit changed overlay");
            }
        }
    }

    std::cout<<"status=OVERLAY_EXACT_REPLAY_PASS\n";
    std::cout<<"recovered_bytes="<<h.source_length<<"\n";
    std::cout<<"overlay_nodes="<<population_before<<"\n";
    std::cout<<"selector_sweeps="<<(h.source_length*CHARACTER_COUNT)<<"\n";
    std::cout<<"yes_responses="<<positives<<"\n";
    std::cout<<"no_responses="<<negatives<<"\n";
    std::cout<<"selector_revisit=PASS\n";
    std::cout<<"field_population_unchanged=PASS\n";
    return 0;
}

int main(int argc,char**argv){
    try{
        if(argc==2 && std::string(argv[1])=="selftest") return selftest();
        if(argc==4 && std::string(argv[1])=="freeze") return freeze_field(argv[2],argv[3]);
        if(argc==4 && std::string(argv[1])=="replay") return replay_field(argv[2],argv[3]);
        std::cerr<<"usage: selftest | freeze SOURCE ARTIFACT | replay ARTIFACT OUTPUT\n";
        return 64;
    }catch(const std::exception& e){
        std::cerr<<"error="<<e.what()<<"\n";
        return 70;
    }
}
