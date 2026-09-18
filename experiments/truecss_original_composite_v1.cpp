#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <iomanip>
#include <sstream>

static constexpr uint32_t NODE_COUNT = 1'000'000;
static constexpr uint32_t MOD = 206;
static constexpr uint64_t PAGE = 1'000'000ULL;
static constexpr uint8_t UNKNOWN = 255;

static uint64_t mix64(uint64_t x){
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31; return x;
}
static uint64_t comb(uint64_t a,uint64_t b){
    return mix64(a ^ (mix64(b + 0x9e3779b97f4a7c15ULL) + 0x517cc1b727220a95ULL));
}
static uint64_t page_state(uint64_t q){
    uint64_t s=0x243f6a8885a308d3ULL;
    for(int i=0;i<20;i++){ s=comb(s,(q>>i)&1ULL); s=comb(s,(uint64_t)i); }
    return s;
}
static uint32_t egcd_inv(uint32_t a,uint32_t m){
    int64_t t=0, nt=1, r=m, nr=a;
    while(nr){ int64_t q=r/nr; int64_t z=t-q*nt; t=nt; nt=z; z=r-q*nr; r=nr; nr=z; }
    if(r!=1) return 0; if(t<0) t+=m; return (uint32_t)t;
}
static std::array<uint8_t,206> INV206=[]{
    std::array<uint8_t,206>a{};
    for(int i=1;i<206;i++) a[i]=(uint8_t)egcd_inv(i,206);
    return a;
}();
static std::array<uint8_t,103> INV103=[]{
    std::array<uint8_t,103>a{};
    for(int i=1;i<103;i++) a[i]=(uint8_t)egcd_inv(i,103);
    return a;
}();
static uint8_t unit206(uint64_t h){
    // odd and not divisible by 103 => unit modulo 206
    uint16_t x=(uint16_t)(2*(h%102)+1);
    if(x==103) x=205;
    return (uint8_t)x;
}

struct F{ uint32_t root; uint8_t mul, add; };

struct DSU {
    std::vector<uint32_t> parent;
    std::vector<uint8_t> rankv,mul,add,parity,r103;

    DSU(): parent(NODE_COUNT), rankv(NODE_COUNT), mul(NODE_COUNT,1),
           add(NODE_COUNT), parity(NODE_COUNT,UNKNOWN), r103(NODE_COUNT,UNKNOWN) {
        for(uint32_t i=0;i<NODE_COUNT;i++) parent[i]=i;
    }

    F find(uint32_t x){
        if(parent[x]==x) return {x,1,0};
        uint32_t p=parent[x]; uint8_t m0=mul[x], a0=add[x];
        F f=find(p);
        uint8_t nm=(uint8_t)((uint32_t)m0*f.mul%MOD);
        uint8_t na=(uint8_t)(((uint32_t)m0*f.add+a0)%MOD);
        parent[x]=f.root; mul[x]=nm; add[x]=na;
        return {f.root,nm,na};
    }

    bool impose_root(uint32_t r,uint16_t c,uint16_t rhs){
        // Solve c*x = rhs modulo 2 and modulo 103.
        if((c&1)==0){ if(rhs&1) return false; }
        else {
            uint8_t v=(uint8_t)(rhs&1);
            if(parity[r]!=UNKNOWN && parity[r]!=v) return false;
            parity[r]=v;
        }
        uint16_t c3=c%103, rhs3=rhs%103;
        if(c3==0){ if(rhs3) return false; }
        else {
            uint8_t v=(uint8_t)((rhs3*INV103[c3])%103);
            if(r103[r]!=UNKNOWN && r103[r]!=v) return false;
            r103[r]=v;
        }
        return true;
    }

    bool inherit_root_constraints(uint32_t child,uint32_t par,uint8_t u,uint8_t v){
        // child_root = u * parent_root + v
        if(parity[child]!=UNKNOWN){
            uint8_t need=(uint8_t)((parity[child]+2-(v&1))&1);
            if(parity[par]!=UNKNOWN && parity[par]!=need) return false;
            parity[par]=need;
        }
        if(r103[child]!=UNKNOWN){
            uint8_t um=u%103, vm=v%103;
            uint8_t need=(uint8_t)(((r103[child]+103-vm)%103*INV103[um])%103);
            if(r103[par]!=UNKNOWN && r103[par]!=need) return false;
            r103[par]=need;
        }
        return true;
    }

    bool link(uint32_t a,uint32_t b,uint8_t rm,uint8_t rc){
        // Enforce value(a) = rm*value(b) + rc (mod 206).
        F A=find(a), B=find(b);
        if(A.root==B.root){
            uint16_t c=(A.mul+MOD-(uint32_t)rm*B.mul%MOD)%MOD;
            uint16_t rhs=((uint32_t)rm*B.add+rc+MOD-A.add)%MOD;
            return impose_root(A.root,c,rhs);
        }
        if(rankv[A.root] < rankv[B.root]){
            uint8_t ia=INV206[A.mul];
            uint8_t u=(uint8_t)((uint32_t)ia*rm%MOD*B.mul%MOD);
            uint8_t v=(uint8_t)((uint32_t)ia*(((uint32_t)rm*B.add+rc+MOD-A.add)%MOD)%MOD);
            if(!inherit_root_constraints(A.root,B.root,u,v)) return false;
            parent[A.root]=B.root; mul[A.root]=u; add[A.root]=v;
        } else {
            uint8_t den=(uint8_t)((uint32_t)rm*B.mul%MOD);
            uint8_t id=INV206[den];
            uint8_t u=(uint8_t)((uint32_t)id*A.mul%MOD);
            uint16_t num=(A.add+MOD-(uint32_t)rm*B.add%MOD+MOD-rc)%MOD;
            uint8_t v=(uint8_t)((uint32_t)id*num%MOD);
            if(!inherit_root_constraints(B.root,A.root,u,v)) return false;
            parent[B.root]=A.root; mul[B.root]=u; add[B.root]=v;
            if(rankv[A.root]==rankv[B.root]) rankv[A.root]++;
        }
        return true;
    }

    std::vector<uint8_t> freeze_values(){
        std::vector<uint8_t> root_value(NODE_COUNT,0), out(NODE_COUNT,0);
        for(uint32_t i=0;i<NODE_COUNT;i++){
            if(parent[i]!=i) continue;
            bool hp=parity[i]!=UNKNOWN, hr=r103[i]!=UNKNOWN;
            uint16_t v=0;
            if(hr){
                v=r103[i];
                if(hp && ((v&1)!=parity[i])) v+=103;
            } else if(hp) v=parity[i];
            root_value[i]=(uint8_t)v;
        }
        for(uint32_t i=0;i<NODE_COUNT;i++){
            F f=find(i);
            out[i]=(uint8_t)(((uint32_t)f.mul*root_value[f.root]+f.add)%MOD);
        }
        return out;
    }
};

struct Rel{ uint32_t a,b; uint8_t ca,cb; uint16_t salt; };

static Rel relation(uint64_t t,uint64_t ctx){
    uint64_t q=t/PAGE, k=t%PAGE, Q=page_state(q);

    // Composite law: preserve q and k as explicit chronology coordinates,
    // add only the later compatible context-conditioned affine expressivity.
    uint64_t h0=comb(Q,comb(k,ctx));
    uint64_t h1=comb(comb(k^0x589965cc75374cc3ULL,Q),mix64(ctx^0xd1b54a32d192ed03ULL));
    uint64_t hs=comb(comb(Q,k),comb(ctx,t));

    Rel z{
        (uint32_t)(h0%NODE_COUNT),
        (uint32_t)(h1%NODE_COUNT),
        unit206(comb(h0,t^0x11ULL)),
        unit206(comb(h1,t^0x22ULL)),
        (uint16_t)(hs%MOD)
    };
    if(z.a==z.b) z.b=(z.b+1)%NODE_COUNT;
    return z;
}

static uint16_t resolve_code(const std::vector<uint8_t>&nodes,uint64_t t,uint64_t ctx){
    Rel z=relation(t,ctx);
    uint32_t lhs=(uint32_t)z.ca*nodes[z.a]%MOD;
    uint32_t rhs=(uint32_t)z.cb*nodes[z.b]%MOD;
    return (uint16_t)((lhs+MOD-rhs+z.salt)%MOD);
}

static std::string hex64(uint64_t x){
    std::ostringstream o; o<<std::hex<<std::setw(16)<<std::setfill('0')<<x; return o.str();
}

struct FNV256 {
    // Two independent 64-bit rolling hashes used only for experiment integrity.
    uint64_t a=1469598103934665603ULL,b=1099511628211ULL;
    void add(uint8_t x){
        a^=x; a*=1099511628211ULL;
        b^=(uint64_t)x+0x9e3779b97f4a7c15ULL; b=mix64(b);
    }
    std::string str() const { return hex64(a)+hex64(b); }
};

static void put_u64(std::ostream&o,uint64_t v){ for(int i=0;i<8;i++) o.put(char((v>>(8*i))&255)); }
static uint64_t get_u64(std::istream&i){ uint64_t v=0; for(int k=0;k<8;k++){int c=i.get(); if(c<0) throw std::runtime_error("short u64"); v|=uint64_t(c)<<(8*k);} return v; }

static int train(const std::string&src,const std::string&state,uint64_t limit){
    std::ifstream in(src,std::ios::binary); if(!in){std::cerr<<"open source\n";return 3;}
    DSU dsu;
    std::array<int16_t,256> to_code; to_code.fill(-1);
    std::array<uint8_t,206> from_code{};
    uint16_t terms=0;
    uint64_t t=0,ctx=0x6a09e667f3bcc909ULL;
    FNV256 source_hash;
    char ch;
    while(t<limit && in.get(ch)){
        uint8_t byte=(uint8_t)ch; source_hash.add(byte);
        int code=to_code[byte];
        if(code<0){
            if(terms>=MOD){std::cout<<"status=TERMINAL_OVERFLOW t="<<t<<"\n";return 2;}
            to_code[byte]=code=terms; from_code[terms]=(uint8_t)byte; terms++;
        }
        Rel z=relation(t,ctx);
        uint16_t w=(uint16_t)((code+MOD-z.salt)%MOD);
        uint8_t inva=INV206[z.ca];
        uint8_t rm=(uint8_t)((uint32_t)inva*z.cb%MOD);
        uint8_t rc=(uint8_t)((uint32_t)inva*w%MOD);
        if(!dsu.link(z.a,z.b,rm,rc)){
            std::cout<<"status=RELATIONAL_CONTRADICTION bytes_imprinted="<<t
                     <<" page="<<t/PAGE<<" key="<<t%PAGE
                     <<" terminals_seen="<<terms
                     <<" node_count="<<NODE_COUNT
                     <<" node_width_bytes=1\n";
            return 2;
        }
        ctx=comb(ctx,(uint64_t)byte+1); t++;
        if(t%1'000'000ULL==0)
            std::cout<<"progress="<<t<<" pages="<<(t/PAGE)<<" terminals="<<terms<<"\n";
    }
    if(t==0){std::cout<<"status=EMPTY\n";return 2;}

    auto nodes=dsu.freeze_values();

    // Pre-freeze replay while source is still available. This validates that
    // DSU training memory is not needed by the generic resolver.
    in.clear(); in.seekg(0);
    uint64_t rt=0,rctx=0x6a09e667f3bcc909ULL; FNV256 verify_hash;
    while(rt<t && in.get(ch)){
        uint16_t c=resolve_code(nodes,rt,rctx);
        if(c>=terms || from_code[c]!=(uint8_t)ch){
            std::cout<<"status=PREFREEZE_REPLAY_FAIL t="<<rt<<" resolved_code="<<c<<"\n";
            return 2;
        }
        uint8_t byte=(uint8_t)ch; verify_hash.add(byte);
        rctx=comb(rctx,(uint64_t)byte+1); rt++;
    }

    std::ofstream out(state,std::ios::binary); if(!out){std::cerr<<"open state\n";return 3;}
    out.write("TCOMPV1",7); out.put(1);
    put_u64(out,t); put_u64(out,NODE_COUNT); out.put(1); // node width
    out.put((char)terms);
    out.write((const char*)from_code.data(),MOD);
    out.write((const char*)nodes.data(),nodes.size());
    put_u64(out,source_hash.a); put_u64(out,source_hash.b);
    out.close();

    std::cout<<"status=FROZEN"
             <<" bytes_imprinted="<<t
             <<" pages="<<((t+PAGE-1)/PAGE)
             <<" terminals_seen="<<terms
             <<" physical_nodes="<<NODE_COUNT
             <<" node_width_bytes=1"
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)
             <<" source_hash="<<source_hash.str()
             <<" prefreeze_replay=PASS\n";
    return 0;
}

static int replay(const std::string&state,const std::string&outpath){
    std::ifstream in(state,std::ios::binary); if(!in){std::cerr<<"open state\n";return 3;}
    char magic[7]; in.read(magic,7); int ver=in.get();
    if(std::string(magic,7)!="TCOMPV1" || ver!=1){std::cerr<<"format\n";return 3;}
    uint64_t n=get_u64(in), nodes_n=get_u64(in); int nodew=in.get(); int termsi=in.get();
    if(nodes_n!=NODE_COUNT || nodew!=1 || termsi<1 || termsi>206){std::cerr<<"geometry\n";return 3;}
    uint16_t terms=(uint16_t)termsi;
    std::array<uint8_t,206> from_code{}; in.read((char*)from_code.data(),MOD);
    std::vector<uint8_t> nodes(NODE_COUNT); in.read((char*)nodes.data(),nodes.size());
    uint64_t want_a=get_u64(in),want_b=get_u64(in);
    if(!in){std::cerr<<"short state\n";return 3;}

    std::ofstream out(outpath,std::ios::binary); if(!out){std::cerr<<"open output\n";return 3;}
    uint64_t ctx=0x6a09e667f3bcc909ULL; FNV256 h;
    for(uint64_t t=0;t<n;t++){
        uint16_t c=resolve_code(nodes,t,ctx);
        if(c>=terms){std::cout<<"status=REPLAY_INVALID_TERMINAL t="<<t<<" code="<<c<<"\n";return 2;}
        uint8_t b=from_code[c]; out.put((char)b); h.add(b);
        ctx=comb(ctx,(uint64_t)b+1);
    }
    out.close();
    bool ok=h.a==want_a && h.b==want_b;
    std::cout<<"status="<<(ok?"REPLAY_PASS":"REPLAY_HASH_FAIL")
             <<" recovered_bytes="<<n
             <<" recovered_hash="<<h.str()
             <<" frozen_state_bytes="<<std::filesystem::file_size(state)
             <<" physical_nodes="<<NODE_COUNT
             <<" node_width_bytes=1\n";
    return ok?0:2;
}

int main(int argc,char**argv){
    if(argc<2) return 64;
    std::string mode=argv[1];
    try{
        if(mode=="train" && argc==5) return train(argv[2],argv[3],std::stoull(argv[4]));
        if(mode=="replay" && argc==4) return replay(argv[2],argv[3]);
    }catch(const std::exception&e){ std::cerr<<"exception="<<e.what()<<"\n"; return 3; }
    std::cerr<<"usage: truecss_original_composite train SOURCE STATE LIMIT | replay STATE OUTPUT\n";
    return 64;
}
