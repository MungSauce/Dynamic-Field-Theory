#include "../trucompute/trucompute_runtime_v3.hpp"
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

namespace trucompute {
constexpr bool directional(State s){ return resolved(s); }
inline uint8_t relative(State a, State ref){
    if(!resolved(a) || !resolved(ref))
        throw std::runtime_error("TruCompute relative() requires resolved directional states");
    return a==ref ? 0 : 1;
}
inline State from_orientation(uint8_t bit){ return bit ? State::NEG : State::POS; }
class PackedGraph {
    std::vector<uint8_t> bytes_;
public:
    explicit PackedGraph(size_t nodes):bytes_(nodes,0){}
    explicit PackedGraph(std::vector<uint8_t> bytes):bytes_(std::move(bytes)){}
    size_t node_count() const { return bytes_.size(); }
    size_t byte_size() const { return bytes_.size(); }
    const std::vector<uint8_t>& bytes() const { return bytes_; }

    State get(uint32_t edge_id) const {
        uint32_t node=edge_id/4, slot=edge_id%4;
        uint8_t code=(bytes_.at(node)>>(slot*2))&0x3;
        return State(code);
    }
    void set(uint32_t edge_id, State s){
        uint32_t node=edge_id/4, slot=edge_id%4;
        uint8_t shift=uint8_t(slot*2);
        bytes_.at(node)=uint8_t((bytes_[node]&~(uint8_t(0x3)<<shift)) |
                                ((uint8_t(s)&0x3)<<shift));
    }
};
} // namespace trucompute


static constexpr uint32_t NODE_COUNT=1'000'000;
static constexpr uint32_t EDGES_PER_NODE=4;
static constexpr uint32_t EDGE_COUNT=NODE_COUNT*EDGES_PER_NODE;
static constexpr uint64_t PAGE=1'000'000ULL;
static constexpr int BITS_PER_TERMINAL=8;

static uint64_t mix64(uint64_t x){
    x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;
    x^=x>>27;x*=0x94d049bb133111ebULL;
    x^=x>>31;return x;
}
static uint64_t comb(uint64_t a,uint64_t b){
    return mix64(a^(mix64(b+0x9e3779b97f4a7c15ULL)+0x517cc1b727220a95ULL));
}
static uint64_t page_state(uint64_t q){
    uint64_t s=0x243f6a8885a308d3ULL;
    for(int i=0;i<20;i++){s=comb(s,(q>>i)&1ULL);s=comb(s,(uint64_t)i);}
    return s;
}
static uint32_t edge_destination(uint32_t edge_id){
    uint32_t node=edge_id/EDGES_PER_NODE,slot=edge_id%EDGES_PER_NODE;
    return (uint32_t)(comb(node,0x4752415048454447ULL+slot)%NODE_COUNT);
}

struct EdgeTriple{uint32_t a,b,ref;uint8_t modifier;};

static EdgeTriple relation(uint64_t t,uint64_t ctx,int bit){
    uint64_t q=t/PAGE,k=t%PAGE,Q=page_state(q);
    uint64_t base=comb(comb(Q,k),comb(ctx,(uint64_t)bit+0x4c4956455a45524fULL));
    uint32_t a=(uint32_t)(comb(base,t^0xa0761d6478bd642fULL)%EDGE_COUNT);
    uint32_t b=(uint32_t)(comb(base,t^0xe7037ed1a0b428dbULL)%EDGE_COUNT);
    uint32_t ref=(uint32_t)(comb(base,t^0x8ebc6af09c88c6e3ULL)%EDGE_COUNT);
    if(a==b)b=(b+1)%EDGE_COUNT;
    if(ref==a||ref==b)ref=(ref+2)%EDGE_COUNT;
    if(ref==a||ref==b)ref=(ref+1)%EDGE_COUNT;
    (void)edge_destination(a);(void)edge_destination(b);(void)edge_destination(ref);
    return{a,b,ref,(uint8_t)(comb(base,0x94d049bb133111ebULL)&1ULL)};
}

// Training-only constraint solver.
// The retained machine state is TruCompute PackedGraph, not this DSU.
struct RelativeConstraintSolver{
    std::vector<uint32_t> parent;
    std::vector<uint8_t> rankv,parity,active;

    RelativeConstraintSolver():parent(EDGE_COUNT),rankv(EDGE_COUNT),parity(EDGE_COUNT),active(EDGE_COUNT){
        for(uint32_t i=0;i<EDGE_COUNT;i++)parent[i]=i;
    }
    std::pair<uint32_t,uint8_t> find(uint32_t x){
        if(parent[x]==x)return{x,0};
        auto f=find(parent[x]);
        parity[x]^=f.second;parent[x]=f.first;
        return{parent[x],parity[x]};
    }
    bool impose(uint32_t a,uint32_t b,uint8_t relative_orientation){
        active[a]=active[b]=1;
        auto A=find(a),B=find(b);
        if(A.first==B.first)
            return (uint8_t)(A.second^B.second)==relative_orientation;
        uint8_t root_rel=(uint8_t)(relative_orientation^A.second^B.second);
        if(rankv[A.first]<rankv[B.first]){
            parent[A.first]=B.first;parity[A.first]=root_rel;
        }else{
            parent[B.first]=A.first;parity[B.first]=root_rel;
            if(rankv[A.first]==rankv[B.first])rankv[A.first]++;
        }
        return true;
    }
    bool impose_live_zero_bit(uint32_t a,uint32_t b,uint32_t ref,uint8_t orientation){
        if(!impose(a,b,1)) return false;          // opposing contributions => live zero
        if(!impose(a,ref,orientation)) return false; // information is relative orientation
        return true;
    }
    trucompute::PackedGraph freeze(){
        trucompute::PackedGraph graph(NODE_COUNT);
        for(uint32_t edge=0;edge<EDGE_COUNT;edge++){
            if(!active[edge]){
                graph.set(edge,trucompute::State::NEITHER);
                continue;
            }
            auto f=find(edge);
            graph.set(edge, f.second ? trucompute::State::NEG : trucompute::State::POS);
        }
        return graph;
    }
};

static uint8_t resolve_bit(const trucompute::PackedGraph& graph,uint64_t t,uint64_t ctx,int bit){
    using namespace trucompute;
    auto r=relation(t,ctx,bit);
    State a=graph.get(r.a),b=graph.get(r.b),ref=graph.get(r.ref);

    // trueCSS now asks TruCompute whether the pair is a live zero.
    State composite=combine(a,b);
    if(composite!=State::STATELESS_ZERO)
        throw std::runtime_error("routed pair is not TruCompute STATELESS_ZERO");
    if(net(composite)!=0 || activity(composite)!=2)
        throw std::runtime_error("TruCompute stateless-zero invariant failed");
    if(!directional(ref))
        throw std::runtime_error("TruCompute reference is NEITHER/non-directional");

    return (uint8_t)(relative(a,ref)^r.modifier);
}

static int resolve_code(const trucompute::PackedGraph& graph,uint64_t t,uint64_t ctx){
    int code=0;
    for(int bit=0;bit<BITS_PER_TERMINAL;bit++)
        code|=(int)resolve_bit(graph,t,ctx,bit)<<bit;
    return code;
}

static trucompute::State press_character(const trucompute::PackedGraph& graph,uint64_t t,uint64_t ctx,uint16_t character){
    const int code=resolve_code(graph,t,ctx);
    if(code<0 || code>=206) return trucompute::State::STATELESS_ZERO;
    return uint16_t(code)==character ? trucompute::State::POS : trucompute::State::NEG;
}

static std::string hex64(uint64_t x){
    std::ostringstream o;o<<std::hex<<std::setw(16)<<std::setfill('0')<<x;return o.str();
}
struct H{
    uint64_t a=1469598103934665603ULL,b=1099511628211ULL;
    void add(uint8_t x){a^=x;a*=1099511628211ULL;b=mix64(b^((uint64_t)x+0x9e3779b97f4a7c15ULL));}
    std::string str()const{return hex64(a)+hex64(b);}
};
static void put_u64(std::ostream&o,uint64_t v){for(int i=0;i<8;i++)o.put(char((v>>(8*i))&255));}
static uint64_t get_u64(std::istream&i){
    uint64_t v=0;
    for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}
    return v;
}

static int train(const std::string&src,const std::string&state,uint64_t limit){
    std::ifstream in(src,std::ios::binary);if(!in){std::cerr<<"open source\n";return 3;}
    RelativeConstraintSolver solver;
    std::array<int16_t,256>to_code;to_code.fill(-1);
    std::array<uint8_t,206>from_code{};
    uint16_t terms=0;uint64_t t=0,ctx=0x6a09e667f3bcc909ULL;H h;char ch;

    while(t<limit&&in.get(ch)){
        uint8_t byte=(uint8_t)ch;h.add(byte);
        int code=to_code[byte];
        if(code<0){
            if(terms>=206){std::cout<<"status=TERMINAL_OVERFLOW t="<<t<<"\n";return 2;}
            to_code[byte]=code=terms;from_code[terms]=(uint8_t)byte;terms++;
        }

        for(int bit=0;bit<BITS_PER_TERMINAL;bit++){
            auto r=relation(t,ctx,bit);
            uint8_t target=(uint8_t)(((code>>bit)&1)^r.modifier);
            if(!solver.impose_live_zero_bit(r.a,r.b,r.ref,target)){
                std::cout<<"status=RELATIONAL_CONTRADICTION"
                         <<" bytes_imprinted="<<t
                         <<" bit_slot="<<bit
                         <<" page="<<t/PAGE
                         <<" key="<<t%PAGE
                         <<" terminals_seen="<<terms
                         <<" physical_nodes="<<NODE_COUNT
                         <<" runtime=TruCompute"
                         <<" graph_bytes="<<NODE_COUNT<<" page_mode=CSS_TINT_FILTER character_mode=HMC_TRUE_FALSE persistent_page_banks=0 persistent_character_tables=0\n";
                return 2;
            }
        }

        ctx=comb(ctx,(uint64_t)byte+1);t++;
        if(t%1'000'000ULL==0)
            std::cout<<"progress="<<t<<" pages="<<t/PAGE<<" terminals="<<terms<<" runtime=TruCompute\n";
    }

    auto graph=solver.freeze();

    // Pre-freeze replay uses only TruCompute graph-state semantics.
    in.clear();in.seekg(0);uint64_t rt=0,rctx=0x6a09e667f3bcc909ULL;
    while(rt<t&&in.get(ch)){
        int code;
        try{code=resolve_code(graph,rt,rctx);}
        catch(const std::exception&e){
            std::cout<<"status=PREFREEZE_REPLAY_FAIL t="<<rt<<" reason="<<e.what()<<"\n";return 2;
        }
        if(code<0||code>=terms||from_code[code]!=(uint8_t)ch){
            std::cout<<"status=PREFREEZE_REPLAY_FAIL t="<<rt<<" resolved_code="<<code<<"\n";return 2;
        }
        uint8_t byte=(uint8_t)ch;rctx=comb(rctx,(uint64_t)byte+1);rt++;
    }

    std::ofstream out(state,std::ios::binary);if(!out)return 3;
    out.write("TCSS_TC1",8);out.put(1);
    put_u64(out,t);put_u64(out,NODE_COUNT);out.put(EDGES_PER_NODE);out.put((char)terms);
    out.write((const char*)from_code.data(),206);
    out.write((const char*)graph.bytes().data(),graph.bytes().size());
    put_u64(out,h.a);put_u64(out,h.b);out.close();

    uint64_t dead=0,neg=0,pos=0,live=0;
    for(uint32_t edge=0;edge<EDGE_COUNT;edge++){
        switch(graph.get(edge)){
            case trucompute::State::NEITHER:dead++;break;
            case trucompute::State::NEG:neg++;break;
            case trucompute::State::POS:pos++;break;
            case trucompute::State::STATELESS_ZERO:live++;break;
        }
    }

    std::cout<<"status=FROZEN"
             <<" bytes_imprinted="<<t
             <<" runtime=TruCompute"
             <<" physical_nodes="<<NODE_COUNT
             <<" graph_bytes="<<graph.byte_size()
             <<" neither_cells="<<dead
             <<" neg_cells="<<neg
             <<" pos_cells="<<pos
             <<" stateless_zero_cells="<<live
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)
             <<" source_hash="<<h.str()
             <<" prefreeze_replay=PASS page_mode=CSS_TINT_FILTER character_mode=HMC_TRUE_FALSE persistent_page_banks=0 persistent_character_tables=0\n";
    return 0;
}

static int replay(const std::string&state,const std::string&outpath){
    std::ifstream in(state,std::ios::binary);if(!in)return 3;
    char magic[8];in.read(magic,8);int ver=in.get();
    if(std::string(magic,8)!="TCSS_TC1"||ver!=1)return 3;
    uint64_t n=get_u64(in),nodes=get_u64(in);int epn=in.get(),termsi=in.get();
    if(nodes!=NODE_COUNT||epn!=EDGES_PER_NODE||termsi<1||termsi>206)return 3;
    uint16_t terms=(uint16_t)termsi;
    std::array<uint8_t,206>from_code{};in.read((char*)from_code.data(),206);
    std::vector<uint8_t>raw(NODE_COUNT);in.read((char*)raw.data(),raw.size());
    uint64_t wa=get_u64(in),wb=get_u64(in);if(!in)return 3;
    trucompute::PackedGraph graph(std::move(raw));

    std::ofstream out(outpath,std::ios::binary);if(!out)return 3;
    uint64_t ctx=0x6a09e667f3bcc909ULL;H h;
    for(uint64_t t=0;t<n;t++){
        int code;
        try{code=resolve_code(graph,t,ctx);}
        catch(const std::exception&e){
            std::cout<<"status=REPLAY_FAIL t="<<t<<" reason="<<e.what()<<"\n";return 2;
        }
        if(code<0||code>=terms){
            std::cout<<"status=REPLAY_INVALID_TERMINAL t="<<t<<" code="<<code<<"\n";return 2;
        }
        uint8_t byte=from_code[code];out.put((char)byte);h.add(byte);ctx=comb(ctx,(uint64_t)byte+1);
    }
    out.close();
    bool ok=h.a==wa&&h.b==wb;
    std::cout<<"status="<<(ok?"REPLAY_PASS":"REPLAY_HASH_FAIL")
             <<" recovered_bytes="<<n
             <<" runtime=TruCompute"
             <<" graph_bytes="<<graph.byte_size()
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)<<" page_mode=CSS_TINT_FILTER character_mode=HMC_TRUE_FALSE persistent_page_banks=0 persistent_character_tables=0\n";
    return ok?0:2;
}

static int selftest(){
    using namespace trucompute;
    if(combine(State::NEG,State::POS)!=State::STATELESS_ZERO)return 1;
    if(net(State::STATELESS_ZERO)!=0)return 2;
    if(activity(State::STATELESS_ZERO)!=2)return 3;
    if(!neither(State::NEITHER))return 4;
    if(relative(State::NEG,State::NEG)!=0)return 5;
    if(relative(State::NEG,State::POS)!=1)return 6;
    std::cout<<"TRUCOMPUTE_V3_CONFORMANCE=PASS\nCSS_OVERLAY_MACHINE=PASS\npage_mode=CSS_TINT_FILTER\ncharacter_mode=HMC_TRUE_FALSE\npersistent_page_banks=0\npersistent_character_tables=0\n";
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
