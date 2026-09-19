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
static constexpr uint64_t PAGE=1'000'000ULL;
static constexpr int BITS_PER_TERMINAL=8;

static uint64_t mix64(uint64_t x){
    x^=x>>30; x*=0xbf58476d1ce4e5b9ULL;
    x^=x>>27; x*=0x94d049bb133111ebULL;
    x^=x>>31; return x;
}
static uint64_t comb(uint64_t a,uint64_t b){
    return mix64(a^(mix64(b+0x9e3779b97f4a7c15ULL)+0x517cc1b727220a95ULL));
}
static uint64_t page_state(uint64_t q){
    uint64_t s=0x243f6a8885a308d3ULL;
    for(int i=0;i<20;i++){s=comb(s,(q>>i)&1ULL);s=comb(s,(uint64_t)i);}
    return s;
}

struct Relation{uint32_t a,b;uint8_t modifier;};

static Relation relation(uint64_t t,uint64_t ctx,int bit){
    uint64_t q=t/PAGE,k=t%PAGE,Q=page_state(q);
    uint64_t base=comb(comb(Q,k),comb(ctx,(uint64_t)bit+0x3b17ULL));
    uint64_t ha=comb(base,t^0xa0761d6478bd642fULL);
    uint64_t hb=comb(base,t^0xe7037ed1a0b428dbULL);
    Relation r{
        (uint32_t)(ha%NODE_COUNT),
        (uint32_t)(hb%NODE_COUNT),
        (uint8_t)(comb(base,0x8ebc6af09c88c6e3ULL)&1ULL)
    };
    if(r.a==r.b)r.b=(r.b+1)%NODE_COUNT;
    return r;
}

struct ParityDSU{
    // parity[x] = orientation(x) XOR orientation(parent[x]).
    // Nodes begin conceptually zero/uncommitted. Only relative polarity is taught.
    std::vector<uint32_t> parent;
    std::vector<uint8_t> rankv,parity,used;
    ParityDSU():parent(NODE_COUNT),rankv(NODE_COUNT),parity(NODE_COUNT),used(NODE_COUNT){
        for(uint32_t i=0;i<NODE_COUNT;i++)parent[i]=i;
    }
    std::pair<uint32_t,uint8_t> find(uint32_t x){
        if(parent[x]==x)return{x,0};
        auto f=find(parent[x]);
        parity[x]^=f.second;
        parent[x]=f.first;
        return{parent[x],parity[x]};
    }
    bool impose(uint32_t a,uint32_t b,uint8_t rel){
        used[a]=used[b]=1;
        auto A=find(a),B=find(b);
        if(A.first==B.first)return (uint8_t)(A.second^B.second)==rel;
        // Need orientation(rootA) XOR orientation(rootB) = rel ^ pa ^ pb.
        uint8_t rp=(uint8_t)(rel^A.second^B.second);
        if(rankv[A.first]<rankv[B.first]){
            parent[A.first]=B.first;parity[A.first]=rp;
        }else{
            parent[B.first]=A.first;parity[B.first]=rp;
            if(rankv[A.first]==rankv[B.first])rankv[A.first]++;
        }
        return true;
    }
    std::vector<int8_t> freeze(){
        // Zero is the conceptual/rest state. Active relational components are
        // realized as +/-1 orientations with arbitrary root +1; only relative
        // polarity carries source information.
        std::vector<int8_t> out(NODE_COUNT,0);
        for(uint32_t i=0;i<NODE_COUNT;i++){
            if(!used[i])continue;
            auto f=find(i);
            out[i]=f.second? -1 : +1;
        }
        return out;
    }
};

static uint8_t resolve_bit(const std::vector<int8_t>&nodes,uint64_t t,uint64_t ctx,int bit){
    auto r=relation(t,ctx,bit);
    int8_t a=nodes[r.a],b=nodes[r.b];
    if(a==0||b==0)throw std::runtime_error("uncommitted node in replay");
    // Same orientation = 0, opposite orientation = 1, then undo generic modifier.
    uint8_t raw=(a==b)?0:1;
    return (uint8_t)(raw^r.modifier);
}

static int resolve_code(const std::vector<int8_t>&nodes,uint64_t t,uint64_t ctx){
    int code=0;
    for(int bit=0;bit<BITS_PER_TERMINAL;bit++)
        code|=(int)resolve_bit(nodes,t,ctx,bit)<<bit;
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

// 2-bit physical serialization of {-1,0,+1}. This is accounting, not the logic.
static std::vector<uint8_t> pack_nodes(const std::vector<int8_t>&nodes){
    std::vector<uint8_t> out((nodes.size()+3)/4,0);
    for(size_t i=0;i<nodes.size();i++){
        uint8_t c=nodes[i]<0?0:(nodes[i]==0?1:2);
        out[i/4]|=(uint8_t)(c<<((i%4)*2));
    }
    return out;
}
static std::vector<int8_t> unpack_nodes(const std::vector<uint8_t>&raw){
    std::vector<int8_t> out(NODE_COUNT);
    for(size_t i=0;i<out.size();i++){
        uint8_t c=(raw[i/4]>>((i%4)*2))&3;
        if(c>2)throw std::runtime_error("reserved node code");
        out[i]=c==0?-1:(c==1?0:+1);
    }
    return out;
}

static int train(const std::string&src,const std::string&state,uint64_t limit){
    std::ifstream in(src,std::ios::binary);if(!in){std::cerr<<"open source\n";return 3;}
    ParityDSU dsu;
    std::array<int16_t,256> to_code;to_code.fill(-1);
    std::array<uint8_t,206> from_code{};
    uint16_t terms=0;uint64_t t=0,ctx=0x6a09e667f3bcc909ULL;H h;
    char ch;
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
            if(!dsu.impose(r.a,r.b,target)){
                std::cout<<"status=RELATIONAL_CONTRADICTION bytes_imprinted="<<t
                         <<" bit_slot="<<bit<<" page="<<t/PAGE<<" key="<<t%PAGE
                         <<" terminals_seen="<<terms<<" physical_nodes="<<NODE_COUNT
                         <<" node_states=3 zero_field=true\n";
                return 2;
            }
        }
        ctx=comb(ctx,(uint64_t)byte+1);t++;
        if(t%1'000'000ULL==0)
            std::cout<<"progress="<<t<<" pages="<<t/PAGE<<" terminals="<<terms<<"\n";
    }

    auto nodes=dsu.freeze();

    in.clear();in.seekg(0);
    uint64_t rt=0,rctx=0x6a09e667f3bcc909ULL;
    while(rt<t&&in.get(ch)){
        int code=resolve_code(nodes,rt,rctx);
        if(code<0||code>=terms||from_code[code]!=(uint8_t)ch){
            std::cout<<"status=PREFREEZE_REPLAY_FAIL t="<<rt<<" resolved_code="<<code<<"\n";return 2;
        }
        uint8_t b=(uint8_t)ch;rctx=comb(rctx,(uint64_t)b+1);rt++;
    }

    auto packed=pack_nodes(nodes);
    std::ofstream out(state,std::ios::binary);if(!out)return 3;
    out.write("TC3ZERO",7);out.put(1);
    put_u64(out,t);put_u64(out,NODE_COUNT);out.put(BITS_PER_TERMINAL);out.put((char)terms);
    out.write((const char*)from_code.data(),206);
    out.write((const char*)packed.data(),packed.size());
    put_u64(out,h.a);put_u64(out,h.b);out.close();

    uint64_t active=0;for(auto x:nodes)if(x)active++;
    std::cout<<"status=FROZEN bytes_imprinted="<<t
             <<" physical_nodes="<<NODE_COUNT<<" node_states=3 zero_field=true"
             <<" active_nodes="<<active<<" neutral_nodes="<<(NODE_COUNT-active)
             <<" packed_node_bytes="<<packed.size()
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)
             <<" source_hash="<<h.str()<<" prefreeze_replay=PASS\n";
    return 0;
}

static int replay(const std::string&state,const std::string&outpath){
    std::ifstream in(state,std::ios::binary);if(!in)return 3;
    char magic[7];in.read(magic,7);int ver=in.get();
    if(std::string(magic,7)!="TC3ZERO"||ver!=1)return 3;
    uint64_t n=get_u64(in),nodesn=get_u64(in);int bpt=in.get(),termsi=in.get();
    if(nodesn!=NODE_COUNT||bpt!=BITS_PER_TERMINAL||termsi<1||termsi>206)return 3;
    uint16_t terms=(uint16_t)termsi;
    std::array<uint8_t,206> from_code{};in.read((char*)from_code.data(),206);
    std::vector<uint8_t> packed((NODE_COUNT+3)/4);in.read((char*)packed.data(),packed.size());
    uint64_t wa=get_u64(in),wb=get_u64(in);if(!in)return 3;
    auto nodes=unpack_nodes(packed);

    std::ofstream out(outpath,std::ios::binary);if(!out)return 3;
    uint64_t ctx=0x6a09e667f3bcc909ULL;H h;
    try{
        for(uint64_t t=0;t<n;t++){
            int code=resolve_code(nodes,t,ctx);
            if(code<0||code>=terms){std::cout<<"status=REPLAY_INVALID_TERMINAL t="<<t<<" code="<<code<<"\n";return 2;}
            uint8_t b=from_code[code];out.put((char)b);h.add(b);ctx=comb(ctx,(uint64_t)b+1);
        }
    }catch(const std::exception&e){std::cout<<"status=REPLAY_FAIL reason="<<e.what()<<"\n";return 2;}
    out.close();
    bool ok=h.a==wa&&h.b==wb;
    std::cout<<"status="<<(ok?"REPLAY_PASS":"REPLAY_HASH_FAIL")
             <<" recovered_bytes="<<n<<" recovered_hash="<<h.str()
             <<" physical_nodes="<<NODE_COUNT<<" node_states=3 zero_field=true"
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)<<"\n";
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
