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
static constexpr int MOD=3;
static constexpr int TRITS_PER_TERMINAL=5; // 3^5 = 243 >= 206

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
static int m3(int x){x%=3;if(x<0)x+=3;return x;}
static int to_bal(int x){ // Z3 -> {-1,0,+1}
    x=m3(x); return x==2?-1:x;
}
static int from_bal(int x){return m3(x);}

struct FindResult{uint32_t root;uint8_t mul,add;};

struct AffineDSU3{
    // value(x) = mul[x]*value(parent[x]) + add[x] mod 3
    std::vector<uint32_t> parent;
    std::vector<uint8_t> rankv,mul,add;
    AffineDSU3():parent(NODE_COUNT),rankv(NODE_COUNT),mul(NODE_COUNT,1),add(NODE_COUNT){
        for(uint32_t i=0;i<NODE_COUNT;i++)parent[i]=i;
    }
    FindResult find(uint32_t x){
        if(parent[x]==x)return{x,1,0};
        uint32_t p=parent[x];uint8_t m0=mul[x],a0=add[x];
        auto f=find(p);
        uint8_t nm=(uint8_t)m3(m0*f.mul);
        uint8_t na=(uint8_t)m3(m0*f.add+a0);
        parent[x]=f.root;mul[x]=nm;add[x]=na;
        return{f.root,nm,na};
    }
    static uint8_t inv(uint8_t x){return x==1?1:2;} // nonzero in GF(3)

    bool impose(uint32_t a,uint8_t ca,uint32_t b,uint8_t cb,uint8_t rhs){
        // ca*A + cb*B = rhs over GF(3); ca/cb are +/-1 => 1 or 2.
        auto A=find(a),B=find(b);
        uint8_t Acoef=(uint8_t)m3(ca*A.mul);
        uint8_t Bcoef=(uint8_t)m3(cb*B.mul);
        uint8_t constant=(uint8_t)m3(ca*A.add+cb*B.add);
        uint8_t rem=(uint8_t)m3(rhs-constant);

        if(A.root==B.root){
            uint8_t coef=(uint8_t)m3(Acoef+Bcoef);
            if(coef==0)return rem==0;
            // A relation inside one component can pin its root. Rather than
            // introduce separate root-state storage, absorb the pin by
            // attaching the root to a dedicated implicit zero is forbidden.
            // We therefore record pins explicitly in fixed root arrays below.
            return pin_root(A.root,(uint8_t)m3(rem*inv(coef)));
        }

        // Acoef*RA + Bcoef*RB = rem
        // RA = u*RB + v
        uint8_t u=(uint8_t)m3(-Bcoef*inv(Acoef));
        uint8_t v=(uint8_t)m3(rem*inv(Acoef));
        if(rankv[A.root]<rankv[B.root]){
            if(!merge_root_state(A.root,B.root,u,v))return false;
            parent[A.root]=B.root;mul[A.root]=u;add[A.root]=v;
        }else{
            // RB = u2*RA + v2
            uint8_t u2=inv(u);
            uint8_t v2=(uint8_t)m3(-u2*v);
            if(!merge_root_state(B.root,A.root,u2,v2))return false;
            parent[B.root]=A.root;mul[B.root]=u2;add[B.root]=v2;
            if(rankv[A.root]==rankv[B.root])rankv[A.root]++;
        }
        return true;
    }

    // Fixed-size root constraints: one trit per physical node allocated at start.
    std::vector<int8_t> pinned=std::vector<int8_t>(NODE_COUNT,3); // 3 = unknown

    bool pin_root(uint32_t r,uint8_t value){
        if(pinned[r]!=3)return pinned[r]==(int8_t)value;
        pinned[r]=(int8_t)value;return true;
    }
    bool merge_root_state(uint32_t child,uint32_t par,uint8_t u,uint8_t v){
        if(pinned[child]==3)return true;
        uint8_t need=(uint8_t)m3((pinned[child]-v)*inv(u));
        if(pinned[par]!=3 && pinned[par]!=(int8_t)need)return false;
        pinned[par]=(int8_t)need;return true;
    }

    std::vector<int8_t> freeze_balanced(){
        std::vector<uint8_t> rootv(NODE_COUNT,0);
        for(uint32_t i=0;i<NODE_COUNT;i++){
            if(parent[i]==i && pinned[i]!=3)rootv[i]=(uint8_t)pinned[i];
        }
        std::vector<int8_t> out(NODE_COUNT);
        for(uint32_t i=0;i<NODE_COUNT;i++){
            auto f=find(i);
            int z=m3(f.mul*rootv[f.root]+f.add);
            out[i]=(int8_t)to_bal(z);
        }
        return out;
    }
};

struct TritRelation{uint32_t a,b;uint8_t ca,cb,salt;};

static TritRelation relation(uint64_t t,uint64_t ctx,int slot){
    uint64_t q=t/PAGE,k=t%PAGE,Q=page_state(q);
    uint64_t base=comb(comb(Q,k),comb(ctx,(uint64_t)slot+0x3b17ULL));
    uint64_t ha=comb(base,t^0xa0761d6478bd642fULL);
    uint64_t hb=comb(base,t^0xe7037ed1a0b428dbULL);
    TritRelation z{
        (uint32_t)(ha%NODE_COUNT),
        (uint32_t)(hb%NODE_COUNT),
        (uint8_t)((ha&1)?1:2), // +1 / -1 coefficient
        (uint8_t)((hb&1)?1:2),
        (uint8_t)(comb(base,0x8ebc6af09c88c6e3ULL)%3)
    };
    if(z.a==z.b)z.b=(z.b+1)%NODE_COUNT;
    return z;
}

static std::array<uint8_t,TRITS_PER_TERMINAL> code_to_trits(uint16_t code){
    std::array<uint8_t,TRITS_PER_TERMINAL> d{};
    for(int i=0;i<TRITS_PER_TERMINAL;i++){d[i]=(uint8_t)(code%3);code/=3;}
    return d;
}
static int trits_to_code(const std::array<uint8_t,TRITS_PER_TERMINAL>&d){
    int code=0,p=1;
    for(int i=0;i<TRITS_PER_TERMINAL;i++){code+=d[i]*p;p*=3;}
    return code;
}

static uint8_t node_z3(const std::vector<int8_t>&nodes,uint32_t i){return (uint8_t)from_bal(nodes[i]);}

static uint8_t resolve_trit(const std::vector<int8_t>&nodes,uint64_t t,uint64_t ctx,int slot){
    auto z=relation(t,ctx,slot);
    return (uint8_t)m3(z.ca*node_z3(nodes,z.a)+z.cb*node_z3(nodes,z.b)+z.salt);
}

static int resolve_code(const std::vector<int8_t>&nodes,uint64_t t,uint64_t ctx){
    std::array<uint8_t,TRITS_PER_TERMINAL>d{};
    for(int i=0;i<TRITS_PER_TERMINAL;i++)d[i]=resolve_trit(nodes,t,ctx,i);
    return trits_to_code(d);
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

// Pack four trits/node states per byte using 2-bit codes:
// -1 -> 0, 0 -> 1, +1 -> 2, 3 reserved.
static std::vector<uint8_t> pack_nodes(const std::vector<int8_t>&nodes){
    std::vector<uint8_t> out((nodes.size()+3)/4,0);
    for(size_t i=0;i<nodes.size();i++){
        uint8_t c=nodes[i]==-1?0:(nodes[i]==0?1:2);
        out[i/4]|=(uint8_t)(c<<((i%4)*2));
    }
    return out;
}
static std::vector<int8_t> unpack_nodes(const std::vector<uint8_t>&raw){
    std::vector<int8_t> nodes(NODE_COUNT);
    for(size_t i=0;i<nodes.size();i++){
        uint8_t c=(raw[i/4]>>((i%4)*2))&3;
        if(c>2)throw std::runtime_error("reserved trit code");
        nodes[i]=c==0?-1:(c==1?0:1);
    }
    return nodes;
}

static int train(const std::string&src,const std::string&state,uint64_t limit){
    std::ifstream in(src,std::ios::binary);if(!in){std::cerr<<"open source\n";return 3;}
    AffineDSU3 dsu;
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
        auto trits=code_to_trits((uint16_t)code);
        for(int slot=0;slot<TRITS_PER_TERMINAL;slot++){
            auto z=relation(t,ctx,slot);
            uint8_t rhs=(uint8_t)m3((int)trits[slot]-(int)z.salt);
            if(!dsu.impose(z.a,z.ca,z.b,z.cb,rhs)){
                std::cout<<"status=RELATIONAL_CONTRADICTION bytes_imprinted="<<t
                         <<" trit_slot="<<slot<<" page="<<t/PAGE<<" key="<<t%PAGE
                         <<" terminals_seen="<<terms<<" physical_nodes="<<NODE_COUNT
                         <<" node_states=3 trits_per_terminal="<<TRITS_PER_TERMINAL<<"\n";
                return 2;
            }
        }
        ctx=comb(ctx,(uint64_t)byte+1);t++;
        if(t%1'000'000ULL==0)std::cout<<"progress="<<t<<" pages="<<t/PAGE<<" terminals="<<terms<<"\n";
    }

    auto nodes=dsu.freeze_balanced();

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
    out.write("TCSS3B1",7);out.put(1);
    put_u64(out,t);put_u64(out,NODE_COUNT);
    out.put(TRITS_PER_TERMINAL);out.put((char)terms);
    out.write((const char*)from_code.data(),206);
    out.write((const char*)packed.data(),packed.size());
    put_u64(out,h.a);put_u64(out,h.b);out.close();

    std::cout<<"status=FROZEN bytes_imprinted="<<t
             <<" physical_nodes="<<NODE_COUNT
             <<" node_states=3 trits_per_terminal="<<TRITS_PER_TERMINAL
             <<" packed_node_bytes="<<packed.size()
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)
             <<" source_hash="<<h.str()<<" prefreeze_replay=PASS\n";
    return 0;
}

static int replay(const std::string&state,const std::string&outpath){
    std::ifstream in(state,std::ios::binary);if(!in)return 3;
    char magic[7];in.read(magic,7);int ver=in.get();
    if(std::string(magic,7)!="TCSS3B1"||ver!=1)return 3;
    uint64_t n=get_u64(in),nodesn=get_u64(in);int tpt=in.get(),termsi=in.get();
    if(nodesn!=NODE_COUNT||tpt!=TRITS_PER_TERMINAL||termsi<1||termsi>206)return 3;
    uint16_t terms=(uint16_t)termsi;
    std::array<uint8_t,206> from_code{};in.read((char*)from_code.data(),206);
    std::vector<uint8_t> packed((NODE_COUNT+3)/4);in.read((char*)packed.data(),packed.size());
    uint64_t wa=get_u64(in),wb=get_u64(in);if(!in)return 3;
    auto nodes=unpack_nodes(packed);

    std::ofstream out(outpath,std::ios::binary);if(!out)return 3;
    uint64_t ctx=0x6a09e667f3bcc909ULL;H h;
    for(uint64_t t=0;t<n;t++){
        int code=resolve_code(nodes,t,ctx);
        if(code<0||code>=terms){std::cout<<"status=REPLAY_INVALID_TERMINAL t="<<t<<" code="<<code<<"\n";return 2;}
        uint8_t b=from_code[code];out.put((char)b);h.add(b);ctx=comb(ctx,(uint64_t)b+1);
    }
    out.close();
    bool ok=h.a==wa&&h.b==wb;
    std::cout<<"status="<<(ok?"REPLAY_PASS":"REPLAY_HASH_FAIL")
             <<" recovered_bytes="<<n<<" recovered_hash="<<h.str()
             <<" physical_nodes="<<NODE_COUNT<<" node_states=3"
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
