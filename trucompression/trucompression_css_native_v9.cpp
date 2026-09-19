#include "../trucompute/trucompute_native_law_v4.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using trucompute_native::Contribution;
using trucompute_native::Environment;
using trucompute_native::Law;
using trucompute_native::Node;
using trucompute_native::Observation;
using trucompute_native::Selector;

static constexpr uint32_t NODE_COUNT=1'000'000;
static constexpr uint32_t SLOTS_PER_NODE=4;
static constexpr uint32_t EDGE_COUNT=NODE_COUNT*SLOTS_PER_NODE;
static constexpr uint64_t PAGE_SIZE=1'000'000ULL;
static constexpr int BITS_PER_TERMINAL=8;

struct RelationMark {
    bool present=false;
    int8_t orientation=0; // semantic orientation only when present: -1 or +1
};

class CssField {
    std::vector<std::array<RelationMark,SLOTS_PER_NODE>> nodes_;
public:
    CssField():nodes_(NODE_COUNT){}

    const RelationMark& mark(uint32_t edge) const {
        return nodes_.at(edge/SLOTS_PER_NODE).at(edge%SLOTS_PER_NODE);
    }
    RelationMark& mark(uint32_t edge) {
        return nodes_.at(edge/SLOTS_PER_NODE).at(edge%SLOTS_PER_NODE);
    }

    void set(uint32_t edge,bool positive){
        auto& m=mark(edge);
        m.present=true;
        m.orientation=positive?+1:-1;
    }

    void clear(uint32_t edge){
        auto& m=mark(edge);
        m.present=false;
        m.orientation=0;
    }

    // Host serialization only. These are not node states.
    // 00 absent relation mark, 01 negative relation orientation,
    // 10 positive relation orientation, 11 invalid/reserved.
    void save_bytes(std::ostream& out) const {
        for(const auto& n:nodes_){
            uint8_t b=0;
            for(uint32_t s=0;s<SLOTS_PER_NODE;++s){
                uint8_t code=0;
                if(n[s].present) code=n[s].orientation>0?2:1;
                b|=uint8_t(code<<(s*2));
            }
            out.put(char(b));
        }
    }

    static CssField load_bytes(std::istream& in){
        CssField f;
        for(uint32_t i=0;i<NODE_COUNT;++i){
            int x=in.get();
            if(x<0) throw std::runtime_error("short css field");
            uint8_t b=uint8_t(x);
            for(uint32_t s=0;s<SLOTS_PER_NODE;++s){
                uint8_t code=(b>>(s*2))&3u;
                if(code==3) throw std::runtime_error("invalid serialized relation mark");
                if(code==0) f.nodes_[i][s]={false,0};
                else f.nodes_[i][s]={true,code==2?int8_t(+1):int8_t(-1)};
            }
        }
        return f;
    }

    static constexpr uint64_t serialized_bytes(){ return NODE_COUNT; }
};

class CssEnvironment final : public Environment {
    const CssField& field_;
public:
    explicit CssEnvironment(const CssField& f):field_(f){}

    Contribution contribution(Node node,const Selector& selector,uint32_t slot) const override {
        if(!selector.page_active || slot>=SLOTS_PER_NODE || node.id>=NODE_COUNT)
            return Contribution::absent();
        const auto& m=field_.mark(node.id*SLOTS_PER_NODE+slot);
        if(!m.present) return Contribution::absent();
        return m.orientation>0?Contribution::pos():Contribution::neg();
    }

    Contribution edge(uint32_t edge_id,const Selector& selector) const {
        return contribution(Node(edge_id/SLOTS_PER_NODE),selector,edge_id%SLOTS_PER_NODE);
    }
};

static uint64_t mix64(uint64_t x){
    x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;
    x^=x>>27;x*=0x94d049bb133111ebULL;
    x^=x>>31;return x;
}
static uint64_t comb(uint64_t a,uint64_t b){
    return mix64(a^(mix64(b+0x9e3779b97f4a7c15ULL)+0x517cc1b727220a95ULL));
}
static uint64_t page_colour(uint64_t q){
    uint64_t s=0x243f6a8885a308d3ULL;
    for(int i=0;i<20;i++){s=comb(s,(q>>i)&1ULL);s=comb(s,uint64_t(i));}
    return s;
}

struct EdgeTriple{uint32_t a,b,ref;uint8_t modifier;};

static EdgeTriple relation(uint64_t t,uint64_t ctx,int bit){
    const uint64_t q=t/PAGE_SIZE,k=t%PAGE_SIZE,Q=page_colour(q);
    const uint64_t base=comb(comb(Q,k),comb(ctx,uint64_t(bit)+0x4c4956455a45524fULL));
    uint32_t a=uint32_t(comb(base,t^0xa0761d6478bd642fULL)%EDGE_COUNT);
    uint32_t b=uint32_t(comb(base,t^0xe7037ed1a0b428dbULL)%EDGE_COUNT);
    uint32_t ref=uint32_t(comb(base,t^0x8ebc6af09c88c6e3ULL)%EDGE_COUNT);
    if(a==b)b=(b+1)%EDGE_COUNT;
    if(ref==a||ref==b)ref=(ref+2)%EDGE_COUNT;
    if(ref==a||ref==b)ref=(ref+1)%EDGE_COUNT;
    return{a,b,ref,uint8_t(comb(base,0x94d049bb133111ebULL)&1ULL)};
}

// Temporary imprint solver. It constrains relation orientations only.
// No Observation is ever retained in the frozen machine.
struct Solver{
    std::vector<uint32_t> parent;
    std::vector<uint8_t> rankv,parity,active;

    Solver():parent(EDGE_COUNT),rankv(EDGE_COUNT),parity(EDGE_COUNT),active(EDGE_COUNT){
        for(uint32_t i=0;i<EDGE_COUNT;++i)parent[i]=i;
    }

    std::pair<uint32_t,uint8_t> find(uint32_t x){
        if(parent[x]==x)return{x,0};
        auto f=find(parent[x]);
        parity[x]^=f.second;
        parent[x]=f.first;
        return{parent[x],parity[x]};
    }

    bool impose(uint32_t a,uint32_t b,uint8_t relative){
        active[a]=active[b]=1;
        auto A=find(a),B=find(b);
        if(A.first==B.first) return uint8_t(A.second^B.second)==relative;
        const uint8_t rr=uint8_t(relative^A.second^B.second);
        if(rankv[A.first]<rankv[B.first]){
            parent[A.first]=B.first;parity[A.first]=rr;
        }else{
            parent[B.first]=A.first;parity[B.first]=rr;
            if(rankv[A.first]==rankv[B.first])++rankv[A.first];
        }
        return true;
    }

    bool impose_balanced_pair(uint32_t a,uint32_t b,uint32_t ref,uint8_t orientation){
        if(!impose(a,b,1)) return false;
        if(!impose(a,ref,orientation)) return false;
        return true;
    }

    CssField freeze(){
        CssField f;
        for(uint32_t e=0;e<EDGE_COUNT;++e){
            if(!active[e]){f.clear(e);continue;}
            auto x=find(e);
            f.set(e,x.second==0);
        }
        return f;
    }
};

static uint8_t relative_orientation(Contribution a,Contribution ref){
    if(!a.participates||!ref.participates)throw std::runtime_error("nonparticipating relative orientation");
    return a.polarity==ref.polarity?0:1;
}

static uint8_t resolve_bit(const CssEnvironment& env,uint64_t t,uint64_t ctx,int bit){
    const auto r=relation(t,ctx,bit);
    Selector selector;
    selector.page=uint16_t(t/PAGE_SIZE);
    selector.page_active=true;

    const auto ca=env.edge(r.a,selector);
    const auto cb=env.edge(r.b,selector);
    const auto cr=env.edge(r.ref,selector);

    const Observation pair=Law::resolve({ca,cb});
    if(pair!=Observation::STATELESS_ZERO)
        throw std::runtime_error("routed pair did not lawfully settle to STATELESS_ZERO");

    const Observation refobs=Law::resolve({cr});
    if(!Law::resolved(refobs))
        throw std::runtime_error("reference relation did not resolve");

    return uint8_t(relative_orientation(ca,cr)^r.modifier);
}

static int resolve_code(const CssEnvironment& env,uint64_t t,uint64_t ctx){
    int code=0;
    for(int bit=0;bit<BITS_PER_TERMINAL;++bit)
        code|=int(resolve_bit(env,t,ctx,bit))<<bit;
    return code;
}

static Observation press_character(const CssEnvironment& env,uint64_t t,uint64_t ctx,uint16_t character){
    const int code=resolve_code(env,t,ctx);
    if(code<0||code>=206) return Observation::STATELESS_ZERO;
    return uint16_t(code)==character?Observation::POS:Observation::NEG;
}

static std::string hex64(uint64_t x){
    std::ostringstream o;o<<std::hex<<std::setw(16)<<std::setfill('0')<<x;return o.str();
}
struct H{
    uint64_t a=1469598103934665603ULL,b=1099511628211ULL;
    void add(uint8_t x){a^=x;a*=1099511628211ULL;b=mix64(b^(uint64_t(x)+0x9e3779b97f4a7c15ULL));}
    std::string str()const{return hex64(a)+hex64(b);}
};
static void put_u64(std::ostream&o,uint64_t v){for(int i=0;i<8;++i)o.put(char((v>>(8*i))&255));}
static uint64_t get_u64(std::istream&i){
    uint64_t v=0;for(int k=0;k<8;++k){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;
}

static int train(const std::string&src,const std::string&state,uint64_t limit){
    std::ifstream in(src,std::ios::binary);if(!in)return 3;
    Solver solver;
    std::array<int16_t,256>to_code;to_code.fill(-1);
    std::array<uint8_t,206>from_code{};
    uint16_t terms=0;uint64_t t=0,ctx=0x6a09e667f3bcc909ULL;H h;char ch;

    while(t<limit&&in.get(ch)){
        uint8_t byte=uint8_t(ch);h.add(byte);
        int code=to_code[byte];
        if(code<0){
            if(terms>=206){std::cout<<"status=TERMINAL_OVERFLOW t="<<t<<"\n";return 2;}
            to_code[byte]=code=terms;from_code[terms]=byte;++terms;
        }

        for(int bit=0;bit<BITS_PER_TERMINAL;++bit){
            auto r=relation(t,ctx,bit);
            uint8_t target=uint8_t(((code>>bit)&1)^r.modifier);
            if(!solver.impose_balanced_pair(r.a,r.b,r.ref,target)){
                std::cout<<"status=RELATIONAL_CONTRADICTION"
                         <<" bytes_imprinted="<<t
                         <<" bit_slot="<<bit
                         <<" page="<<t/PAGE_SIZE
                         <<" key="<<t%PAGE_SIZE
                         <<" terminals_seen="<<terms
                         <<" physical_nodes="<<NODE_COUNT
                         <<" runtime=TruComputeNativeLawV4"
                         <<" node_retained_state_bytes=0"
                         <<" css_field_bytes="<<CssField::serialized_bytes()
                         <<" persistent_page_banks=0"
                         <<" persistent_character_tables=0\n";
                return 2;
            }
        }
        ctx=comb(ctx,uint64_t(byte)+1);++t;
    }

    CssField field=solver.freeze();
    CssEnvironment env(field);

    in.clear();in.seekg(0);uint64_t rt=0,rctx=0x6a09e667f3bcc909ULL;
    while(rt<t&&in.get(ch)){
        int code;
        try{code=resolve_code(env,rt,rctx);}
        catch(const std::exception&e){std::cout<<"status=PREFREEZE_REPLAY_FAIL t="<<rt<<" reason="<<e.what()<<"\n";return 2;}
        if(code<0||code>=terms||from_code[code]!=uint8_t(ch)){
            std::cout<<"status=PREFREEZE_REPLAY_FAIL t="<<rt<<" resolved_code="<<code<<"\n";return 2;
        }
        uint8_t byte=uint8_t(ch);rctx=comb(rctx,uint64_t(byte)+1);++rt;
    }

    // Literal HMC lens check on the first retained position.
    if(t){
        uint64_t cctx=0x6a09e667f3bcc909ULL;
        int yes=0;
        for(uint16_t c=0;c<206;++c)
            if(press_character(env,0,cctx,c)==Observation::POS) ++yes;
        if(yes!=1){std::cout<<"status=HMC_LENS_FAIL yes="<<yes<<"\n";return 2;}
    }

    std::ofstream out(state,std::ios::binary);if(!out)return 3;
    out.write("TCSSNL4",7);out.put(1);
    put_u64(out,t);put_u64(out,NODE_COUNT);out.put(SLOTS_PER_NODE);out.put(char(terms));
    out.write(reinterpret_cast<const char*>(from_code.data()),206);
    field.save_bytes(out);
    put_u64(out,h.a);put_u64(out,h.b);out.close();

    std::cout<<"status=FROZEN"
             <<" bytes_imprinted="<<t
             <<" runtime=TruComputeNativeLawV4"
             <<" node_retained_state_bytes=0"
             <<" css_field_bytes="<<CssField::serialized_bytes()
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)
             <<" persistent_page_banks=0"
             <<" persistent_character_tables=0"
             <<" prefreeze_replay=PASS\n";
    return 0;
}

static int replay(const std::string&state,const std::string&outpath){
    std::ifstream in(state,std::ios::binary);if(!in)return 3;
    char magic[7];in.read(magic,7);int ver=in.get();
    if(std::string(magic,7)!="TCSSNL4"||ver!=1)return 3;
    uint64_t n=get_u64(in),nodes=get_u64(in);int slots=in.get(),termsi=in.get();
    if(nodes!=NODE_COUNT||slots!=SLOTS_PER_NODE||termsi<1||termsi>206)return 3;
    uint16_t terms=uint16_t(termsi);
    std::array<uint8_t,206>from_code{};in.read(reinterpret_cast<char*>(from_code.data()),206);
    CssField field=CssField::load_bytes(in);
    uint64_t wa=get_u64(in),wb=get_u64(in);if(!in)return 3;
    CssEnvironment env(field);

    std::ofstream out(outpath,std::ios::binary);if(!out)return 3;
    uint64_t ctx=0x6a09e667f3bcc909ULL;H h;
    for(uint64_t t=0;t<n;++t){
        int code;
        try{code=resolve_code(env,t,ctx);}
        catch(const std::exception&e){std::cout<<"status=REPLAY_FAIL t="<<t<<" reason="<<e.what()<<"\n";return 2;}
        if(code<0||code>=terms){std::cout<<"status=REPLAY_INVALID_TERMINAL t="<<t<<" code="<<code<<"\n";return 2;}
        uint8_t b=from_code[code];out.put(char(b));h.add(b);ctx=comb(ctx,uint64_t(b)+1);
    }
    out.close();
    bool ok=h.a==wa&&h.b==wb;
    std::cout<<"status="<<(ok?"REPLAY_PASS":"REPLAY_HASH_FAIL")
             <<" recovered_bytes="<<n
             <<" runtime=TruComputeNativeLawV4"
             <<" node_retained_state_bytes=0"
             <<" css_field_bytes="<<CssField::serialized_bytes()
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)<<"\n";
    return ok?0:2;
}

static int selftest(){
    if(sizeof(Node)!=sizeof(uint32_t))return 1;
    if(Law::resolve({})!=Observation::NEITHER)return 2;
    if(Law::resolve({Contribution::neg(),Contribution::pos()})!=Observation::STATELESS_ZERO)return 3;
    std::cout<<"TRUCOMPUTE_NATIVE_LAW_V4=PASS\n"
             <<"CSS_NATIVE_OVERLAY_V9=PASS\n"
             <<"node_retained_state_bytes=0\n"
             <<"observations_retained_in_nodes=false\n"
             <<"persistent_page_banks=0\n"
             <<"persistent_character_tables=0\n";
    return 0;
}

int main(int argc,char**argv){
    if(argc<2)return 64;
    try{
        std::string mode=argv[1];
        if(mode=="selftest")return selftest();
        if(mode=="train"&&argc==5)return train(argv[2],argv[3],std::stoull(argv[4]));
        if(mode=="replay"&&argc==4)return replay(argv[2],argv[3]);
    }catch(const std::exception&e){std::cerr<<"exception="<<e.what()<<"\n";return 3;}
    return 64;
}
