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

struct EdgePair{uint32_t a,b;uint8_t modifier;};

static EdgePair relation(uint64_t t,uint64_t ctx,int bit){
    uint64_t q=t/PAGE,k=t%PAGE,Q=page_state(q);
    uint64_t base=comb(comb(Q,k),comb(ctx,(uint64_t)bit+0x4c4956455a45524fULL));
    uint32_t a=(uint32_t)(comb(base,t^0xa0761d6478bd642fULL)%EDGE_COUNT);
    uint32_t b=(uint32_t)(comb(base,t^0xe7037ed1a0b428dbULL)%EDGE_COUNT);
    if(a==b)b=(b+1)%EDGE_COUNT;
    // Touch endpoint derivation so graph identity includes the fixed topology.
    (void)edge_destination(a);(void)edge_destination(b);
    return{a,b,(uint8_t)(comb(base,0x8ebc6af09c88c6e3ULL)&1ULL)};
}

struct ParityDSU{
    // Each DSU variable is one fixed graphical edge-weight slot.
    // 0/unconstrained = dead. Once participating, final weights are +/-1.
    std::vector<uint32_t> parent;
    std::vector<uint8_t> rankv,parity,active;

    ParityDSU():parent(EDGE_COUNT),rankv(EDGE_COUNT),parity(EDGE_COUNT),active(EDGE_COUNT){
        for(uint32_t i=0;i<EDGE_COUNT;i++)parent[i]=i;
    }
    std::pair<uint32_t,uint8_t> find(uint32_t x){
        if(parent[x]==x)return{x,0};
        auto f=find(parent[x]);
        parity[x]^=f.second;parent[x]=f.first;
        return{parent[x],parity[x]};
    }
    bool impose_opposition(uint32_t a,uint32_t b,uint8_t orientation){
        // orientation 0: a=+1,b=-1 ; orientation 1: a=-1,b=+1.
        // Both cases require opposite signs; orientation relative to a generic
        // component reference is carried separately by the target modifier.
        active[a]=active[b]=1;
        auto A=find(a),B=find(b);
        uint8_t rel=1; // signs must differ -> live zero
        if(A.first==B.first)return (uint8_t)(A.second^B.second)==rel;
        uint8_t rp=(uint8_t)(rel^A.second^B.second);
        if(rankv[A.first]<rankv[B.first]){
            parent[A.first]=B.first;parity[A.first]=rp;
        }else{
            parent[B.first]=A.first;parity[B.first]=rp;
            if(rankv[A.first]==rankv[B.first])rankv[A.first]++;
        }

        // The graph relation itself only requires opposition. The logical
        // orientation is encoded by ordering the two edge ids selected by
        // the routing law; swapping a/b flips the observed bit.
        (void)orientation;
        return true;
    }
    std::vector<int8_t> freeze(){
        std::vector<int8_t> out(EDGE_COUNT,0);
        for(uint32_t i=0;i<EDGE_COUNT;i++){
            if(!active[i])continue; // dead zero
            auto f=find(i);
            out[i]=f.second?-1:+1;
        }
        return out;
    }
};

static uint8_t resolve_bit(const std::vector<int8_t>&w,uint64_t t,uint64_t ctx,int bit){
    auto r=relation(t,ctx,bit);
    int8_t a=w[r.a],b=w[r.b];
    int net=(int)a+(int)b;
    int activity=(a!=0)+(b!=0);

    // Dead zero and nonbalanced active states are distinct and invalid here.
    if(activity==0)throw std::runtime_error("dead-zero queried");
    if(activity!=2||net!=0)throw std::runtime_error("relation is not live-zero");

    // Two live-zero orientations:
    // (+1,-1) => 0, (-1,+1) => 1, then generic modifier.
    uint8_t orientation=(a<0)?1:0;
    return (uint8_t)(orientation^r.modifier);
}

static int resolve_code(const std::vector<int8_t>&w,uint64_t t,uint64_t ctx){
    int code=0;
    for(int bit=0;bit<BITS_PER_TERMINAL;bit++)
        code|=(int)resolve_bit(w,t,ctx,bit)<<bit;
    return code;
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
static uint64_t get_u64(std::istream&i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short");v|=uint64_t(c)<<(8*k);}return v;}

static std::vector<uint8_t> pack_weights(const std::vector<int8_t>&w){
    // Four fixed {-1,0,+1} edge weights per byte = exactly one byte/node.
    std::vector<uint8_t> out(NODE_COUNT,0);
    for(uint32_t i=0;i<EDGE_COUNT;i++){
        uint8_t c=w[i]<0?0:(w[i]==0?1:2);
        out[i/4]|=(uint8_t)(c<<((i%4)*2));
    }
    return out;
}
static std::vector<int8_t> unpack_weights(const std::vector<uint8_t>&raw){
    std::vector<int8_t>w(EDGE_COUNT);
    for(uint32_t i=0;i<EDGE_COUNT;i++){
        uint8_t c=(raw[i/4]>>((i%4)*2))&3;
        if(c>2)throw std::runtime_error("reserved edge state");
        w[i]=c==0?-1:(c==1?0:+1);
    }
    return w;
}

static int train(const std::string&src,const std::string&state,uint64_t limit){
    std::ifstream in(src,std::ios::binary);if(!in)return 3;
    ParityDSU dsu;
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

            // A bit is always a LIVE zero: opposing signed edges. The target
            // decides which routed edge is treated as the positive-first side.
            // Swap order for the opposite orientation without changing sum.
            uint32_t a=target?r.b:r.a;
            uint32_t b=target?r.a:r.b;
            if(!dsu.impose_opposition(a,b,target)){
                std::cout<<"status=RELATIONAL_CONTRADICTION bytes_imprinted="<<t
                         <<" bit_slot="<<bit<<" page="<<t/PAGE<<" key="<<t%PAGE
                         <<" terminals_seen="<<terms<<" physical_nodes="<<NODE_COUNT
                         <<" edge_slots="<<EDGE_COUNT<<" node_width_bytes=1"
                         <<" live_zero=true\n";
                return 2;
            }
        }

        ctx=comb(ctx,(uint64_t)byte+1);t++;
        if(t%1'000'000ULL==0)std::cout<<"progress="<<t<<" pages="<<t/PAGE<<" terminals="<<terms<<"\n";
    }

    auto weights=dsu.freeze();

    // IMPORTANT: relative DSU components have arbitrary global sign, but the
    // decoder reads ordered orientation. Canonicalize each active component by
    // choosing the smallest active edge as +1, then flip the component if needed.
    std::vector<uint32_t>minedge(EDGE_COUNT,UINT32_MAX);
    for(uint32_t i=0;i<EDGE_COUNT;i++)if(weights[i]){
        auto f=dsu.find(i);if(i<minedge[f.first])minedge[f.first]=i;
    }
    std::vector<uint8_t>flip(EDGE_COUNT,0);
    for(uint32_t r=0;r<EDGE_COUNT;r++)if(minedge[r]!=UINT32_MAX && weights[minedge[r]]<0)flip[r]=1;
    for(uint32_t i=0;i<EDGE_COUNT;i++)if(weights[i]){
        auto f=dsu.find(i);if(flip[f.first])weights[i]=(int8_t)-weights[i];
    }

    // Generic pre-freeze replay.
    in.clear();in.seekg(0);uint64_t rt=0,rctx=0x6a09e667f3bcc909ULL;
    while(rt<t&&in.get(ch)){
        int code;
        try{code=resolve_code(weights,rt,rctx);}
        catch(const std::exception&e){
            std::cout<<"status=PREFREEZE_REPLAY_FAIL t="<<rt<<" reason="<<e.what()<<"\n";return 2;
        }
        if(code<0||code>=terms||from_code[code]!=(uint8_t)ch){
            std::cout<<"status=PREFREEZE_REPLAY_FAIL t="<<rt<<" resolved_code="<<code<<"\n";return 2;
        }
        uint8_t b=(uint8_t)ch;rctx=comb(rctx,(uint64_t)b+1);rt++;
    }

    auto packed=pack_weights(weights);
    std::ofstream out(state,std::ios::binary);if(!out)return 3;
    out.write("TC3GRF1",7);out.put(1);
    put_u64(out,t);put_u64(out,NODE_COUNT);out.put(EDGES_PER_NODE);out.put((char)terms);
    out.write((const char*)from_code.data(),206);
    out.write((const char*)packed.data(),packed.size());
    put_u64(out,h.a);put_u64(out,h.b);out.close();

    uint64_t active=0;for(auto x:weights)if(x)active++;
    std::cout<<"status=FROZEN bytes_imprinted="<<t
             <<" physical_nodes="<<NODE_COUNT<<" fixed_edges_per_node="<<EDGES_PER_NODE
             <<" node_width_bytes=1 active_edge_weights="<<active
             <<" dead_edge_weights="<<(EDGE_COUNT-active)
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)
             <<" source_hash="<<h.str()<<" prefreeze_replay=PASS\n";
    return 0;
}

static int replay(const std::string&state,const std::string&outpath){
    std::ifstream in(state,std::ios::binary);if(!in)return 3;
    char magic[7];in.read(magic,7);int ver=in.get();
    if(std::string(magic,7)!="TC3GRF1"||ver!=1)return 3;
    uint64_t n=get_u64(in),nodes=get_u64(in);int epn=in.get(),termsi=in.get();
    if(nodes!=NODE_COUNT||epn!=EDGES_PER_NODE||termsi<1||termsi>206)return 3;
    uint16_t terms=(uint16_t)termsi;
    std::array<uint8_t,206>from_code{};in.read((char*)from_code.data(),206);
    std::vector<uint8_t>packed(NODE_COUNT);in.read((char*)packed.data(),packed.size());
    uint64_t wa=get_u64(in),wb=get_u64(in);if(!in)return 3;
    auto weights=unpack_weights(packed);

    std::ofstream out(outpath,std::ios::binary);if(!out)return 3;
    uint64_t ctx=0x6a09e667f3bcc909ULL;H h;
    for(uint64_t t=0;t<n;t++){
        int code;
        try{code=resolve_code(weights,t,ctx);}
        catch(const std::exception&e){std::cout<<"status=REPLAY_FAIL t="<<t<<" reason="<<e.what()<<"\n";return 2;}
        if(code<0||code>=terms){std::cout<<"status=REPLAY_INVALID_TERMINAL t="<<t<<" code="<<code<<"\n";return 2;}
        uint8_t b=from_code[code];out.put((char)b);h.add(b);ctx=comb(ctx,(uint64_t)b+1);
    }
    out.close();
    bool ok=h.a==wa&&h.b==wb;
    std::cout<<"status="<<(ok?"REPLAY_PASS":"REPLAY_HASH_FAIL")
             <<" recovered_bytes="<<n<<" frozen_state_bytes="<<std::filesystem::file_size(state)<<"\n";
    return ok?0:2;
}

int main(int argc,char**argv){
    if(argc<2)return 64;
    try{
        std::string mode=argv[1];
        if(mode=="train"&&argc==5)return train(argv[2],argv[3],std::stoull(argv[4]));
        if(mode=="replay"&&argc==4)return replay(argv[2],argv[3]);
    }catch(const std::exception&e){std::cerr<<"exception="<<e.what()<<"\n";return 3;}
    return 64;
}
