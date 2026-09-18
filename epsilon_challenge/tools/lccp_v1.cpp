#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static void put_u16(std::ostream& o, uint16_t v){ o.put(char(v&255)); o.put(char(v>>8)); }
static uint16_t get_u16(std::istream& i){ int a=i.get(),b=i.get(); if(a<0||b<0) throw std::runtime_error("short u16"); return uint16_t(a|(b<<8)); }
static void put_u32(std::ostream& o,uint32_t v){ for(int k=0;k<4;k++) o.put(char((v>>(8*k))&255)); }
static uint32_t get_u32(std::istream& i){ uint32_t v=0; for(int k=0;k<4;k++){int c=i.get(); if(c<0) throw std::runtime_error("short u32"); v|=uint32_t(c)<<(8*k);} return v; }
static void put_u64(std::ostream& o,uint64_t v){ for(int k=0;k<8;k++) o.put(char((v>>(8*k))&255)); }
static uint64_t get_u64(std::istream& i){ uint64_t v=0; for(int k=0;k<8;k++){int c=i.get(); if(c<0) throw std::runtime_error("short u64"); v|=uint64_t(c)<<(8*k);} return v; }
static void put_var(std::ostream& o,uint64_t v){ while(v>=128){o.put(char((v&127)|128));v>>=7;} o.put(char(v)); }
static uint64_t get_var(std::istream& i){ uint64_t v=0; int sh=0; for(;;){int c=i.get(); if(c<0) throw std::runtime_error("short varint"); v|=uint64_t(c&127)<<sh; if(!(c&128)) return v; sh+=7; if(sh>63) throw std::runtime_error("varint overflow");} }

struct HNode { uint64_t f=0; int l=-1,r=-1,sym=-1; int min_sym=999; };
struct QItem { uint64_t f; int min_sym; int idx; };
struct QCmp { bool operator()(const QItem&a,const QItem&b) const { if(a.f!=b.f) return a.f>b.f; if(a.min_sym!=b.min_sym) return a.min_sym>b.min_sym; return a.idx>b.idx; } };

static std::array<uint8_t,256> huffman_lengths(const std::array<uint64_t,256>& cnt){
    std::array<uint8_t,256> len{};
    std::priority_queue<QItem,std::vector<QItem>,QCmp> q;
    std::vector<HNode> n;
    for(int s=0;s<256;s++) if(cnt[s]){ int idx=n.size(); n.push_back({cnt[s],-1,-1,s,s}); q.push({cnt[s],s,idx}); }
    if(q.empty()) return len;
    if(q.size()==1){ len[n[q.top().idx].sym]=1; return len; }
    while(q.size()>1){ auto a=q.top();q.pop(); auto b=q.top();q.pop(); int idx=n.size(); n.push_back({a.f+b.f,a.idx,b.idx,-1,std::min(a.min_sym,b.min_sym)}); q.push({a.f+b.f,n.back().min_sym,idx}); }
    std::vector<std::pair<int,int>> st{{q.top().idx,0}};
    while(!st.empty()){ auto [x,d]=st.back(); st.pop_back(); if(n[x].sym>=0){ if(d<=0||d>63) throw std::runtime_error("huffman depth"); len[n[x].sym]=uint8_t(d);} else {st.push_back({n[x].l,d+1});st.push_back({n[x].r,d+1});} }
    return len;
}

struct TrieNode { int child[2]={-1,-1}; int sym=-1; };
struct PathStep { uint16_t node; uint8_t bit; };
struct TreeModel {
    std::vector<TrieNode> t;
    std::array<std::vector<PathStep>,256> path;
    std::array<uint8_t,256> lengths{};
};

static TreeModel build_tree(const std::array<uint8_t,256>& lengths){
    TreeModel m; m.lengths=lengths; m.t.push_back({});
    std::vector<std::pair<uint8_t,int>> ord;
    for(int s=0;s<256;s++) if(lengths[s]) ord.push_back({lengths[s],s});
    std::sort(ord.begin(),ord.end(),[](auto a,auto b){return a.first!=b.first?a.first<b.first:a.second<b.second;});
    uint64_t code=0; uint8_t prev=0;
    for(auto [l,s]:ord){ if(l>prev) code<<=(l-prev); int node=0; for(int b=int(l)-1;b>=0;b--){ int bit=(code>>b)&1; if(m.t[node].child[bit]<0){ m.t[node].child[bit]=int(m.t.size()); m.t.push_back({}); } m.path[s].push_back({uint16_t(node),uint8_t(bit)}); node=m.t[node].child[bit]; } if(m.t[node].sym>=0) throw std::runtime_error("duplicate code"); m.t[node].sym=s; code++; prev=l; }
    return m;
}

struct PackedBits {
    std::vector<uint64_t> w; uint64_t n=0; uint64_t ones=0;
    void alloc(uint64_t bits){ n=0; ones=0; w.assign((bits+63)/64,0); }
    void push(uint8_t b){ if(b){ w[n>>6] |= uint64_t(1)<<(n&63); ones++; } n++; }
    uint8_t get(uint64_t i) const { return uint8_t((w[i>>6]>>(i&63))&1); }
    std::vector<uint8_t> raw_bytes() const { std::vector<uint8_t> out((n+7)/8,0); for(uint64_t i=0;i<n;i++) if(get(i)) out[i>>3]|=uint8_t(1u<<(i&7)); return out; }
};

struct BitOut { std::vector<uint8_t> b; uint8_t cur=0; int used=0; void bit(int x){cur|=uint8_t((x&1)<<used); if(++used==8){b.push_back(cur);cur=0;used=0;}} void finish(){if(used)b.push_back(cur);} };
struct BitIn { const std::vector<uint8_t>& b; size_t p=0; int used=8; uint8_t cur=0; explicit BitIn(const std::vector<uint8_t>&x):b(x){} int bit(){ if(used==8){ if(p>=b.size()) throw std::runtime_error("short gamma");cur=b[p++];used=0;} return (cur>>(used++))&1; } };
static void gamma_put(BitOut& o,uint64_t x){ if(!x) throw std::runtime_error("gamma zero"); int l=63-__builtin_clzll(x); for(int i=0;i<l;i++) o.bit(0); for(int i=l;i>=0;i--) o.bit((x>>i)&1); }
static uint64_t gamma_get(BitIn& i){ int z=0; while(i.bit()==0){ if(++z>63) throw std::runtime_error("gamma overflow"); } uint64_t x=1; for(int k=0;k<z;k++) x=(x<<1)|i.bit(); return x; }

static std::vector<uint8_t> rle_encode(const PackedBits& p){
    std::vector<uint8_t> out; if(!p.n) return out; BitOut bo; uint8_t cur=p.get(0); out.push_back(cur); uint64_t run=1; for(uint64_t i=1;i<p.n;i++){ uint8_t x=p.get(i); if(x==cur) run++; else { gamma_put(bo,run); cur=x; run=1; } } gamma_put(bo,run); bo.finish(); out.insert(out.end(),bo.b.begin(),bo.b.end()); return out;
}

static constexpr uint32_t RANS_L = 1u<<23;
static constexpr uint32_t SCALE_BITS = 12;
static constexpr uint32_t TOT = 1u<<SCALE_BITS;
static std::vector<uint8_t> rans_encode(const PackedBits& p,uint16_t f1){
    if(!p.n) return {}; uint32_t f[2]={TOT-f1,f1}; uint32_t c[2]={0,f[0]};
    std::vector<uint8_t> buf((p.n+7)/8+64); size_t ptr=buf.size(); uint32_t st=RANS_L;
    for(uint64_t ii=p.n; ii-->0;){ uint32_t s=p.get(ii); uint32_t fs=f[s]; uint32_t x_max=((RANS_L>>SCALE_BITS)<<8)*fs; while(st>=x_max){ if(ptr==0) throw std::runtime_error("rans buffer"); buf[--ptr]=uint8_t(st); st>>=8; } st=((st/fs)<<SCALE_BITS)+(st%fs)+c[s]; }
    if(ptr<4) throw std::runtime_error("rans state buffer"); ptr-=4; buf[ptr]=uint8_t(st);buf[ptr+1]=uint8_t(st>>8);buf[ptr+2]=uint8_t(st>>16);buf[ptr+3]=uint8_t(st>>24);
    return std::vector<uint8_t>(buf.begin()+ptr,buf.end());
}

struct RansDec { const std::vector<uint8_t>* b=nullptr; size_t p=4; uint32_t st=0; uint16_t f1=0; void init(const std::vector<uint8_t>&x,uint16_t q){ if(x.size()<4) throw std::runtime_error("short rans"); b=&x;f1=q;st=uint32_t(x[0])|(uint32_t(x[1])<<8)|(uint32_t(x[2])<<16)|(uint32_t(x[3])<<24);p=4;} int next(){ uint32_t f0=TOT-f1; uint32_t xm=st&(TOT-1); int s=xm>=f0; uint32_t fs=s?f1:f0; uint32_t cum=s?f0:0; st=fs*(st>>SCALE_BITS)+xm-cum; while(st<RANS_L){ if(p>=b->size()) throw std::runtime_error("short rans stream"); st=(st<<8)|(*b)[p++]; } return s; } };

static uint16_t choose_f1(uint64_t n,uint64_t ones){ if(!n||!ones||ones==n) return 0; uint64_t q=(ones*TOT+n/2)/n; if(q<1)q=1; if(q>TOT-1)q=TOT-1; return uint16_t(q); }

enum : uint8_t { M_CONST0=0, M_CONST1=1, M_RAW=2, M_RLE=3, M_RANS=4 };
struct EncStream { uint64_t n=0; uint8_t mode=0; uint16_t f1=0; std::vector<uint8_t> payload; };
static EncStream compress_stream(const PackedBits& p){ EncStream e; e.n=p.n; if(!p.n||p.ones==0){e.mode=M_CONST0;return e;} if(p.ones==p.n){e.mode=M_CONST1;return e;} auto raw=p.raw_bytes(); auto rle=rle_encode(p); uint16_t f1=choose_f1(p.n,p.ones); auto ra=rans_encode(p,f1); e.mode=M_RAW;e.payload=std::move(raw); if(rle.size()<e.payload.size()){e.mode=M_RLE;e.payload=std::move(rle);} if(ra.size()<e.payload.size()){e.mode=M_RANS;e.f1=f1;e.payload=std::move(ra);} return e; }

struct StreamDec { uint8_t mode=0; uint64_t n=0,pos=0; std::vector<uint8_t> payload; uint16_t f1=0; RansDec rd; uint8_t rbit=0; uint64_t rleft=0; std::unique_ptr<BitIn> bin; void init(){ if(mode==M_RANS) rd.init(payload,f1); if(mode==M_RLE){ if(payload.empty()) throw std::runtime_error("empty rle"); rbit=payload[0]&1; std::vector<uint8_t> rest(payload.begin()+1,payload.end()); payload.swap(rest); bin=std::make_unique<BitIn>(payload); rleft=gamma_get(*bin);} } int next(){ if(pos>=n) throw std::runtime_error("stream overrun"); pos++; if(mode==M_CONST0)return 0; if(mode==M_CONST1)return 1; if(mode==M_RAW){uint64_t i=pos-1;return (payload[i>>3]>>(i&7))&1;} if(mode==M_RANS)return rd.next(); if(mode==M_RLE){ if(!rleft){rbit^=1;rleft=gamma_get(*bin);} int x=rbit; rleft--; return x;} throw std::runtime_error("bad mode"); } };

static std::array<uint64_t,256> count_file(const std::string& path,uint64_t& total){ std::ifstream in(path,std::ios::binary); if(!in) throw std::runtime_error("open source"); std::array<uint64_t,256> c{}; std::vector<char> b(1<<20); total=0; while(in){in.read(b.data(),b.size()); size_t n=in.gcount(); total+=n; for(size_t i=0;i<n;i++) c[(uint8_t)b[i]]++;} return c; }

static void encode(const std::string& src,const std::string& dst,uint64_t tile_cells){
    uint64_t total=0; auto counts=count_file(src,total); auto lengths=huffman_lengths(counts); auto tree=build_tree(lengths); uint32_t internal=0; std::vector<int> internal_id(tree.t.size(),-1); for(size_t i=0;i<tree.t.size();i++) if(tree.t[i].sym<0) internal_id[i]=internal++;
    std::ofstream out(dst,std::ios::binary); if(!out) throw std::runtime_error("open output"); out.write("LCP1",4); out.put(1); put_u64(out,total); put_u32(out,10000); put_u32(out,10000); put_u64(out,tile_cells); put_u16(out,uint16_t(internal)); for(auto l:lengths) out.put(char(l));
    uint64_t tiles=(total+tile_cells-1)/tile_cells; put_u32(out,uint32_t(tiles)); std::ifstream in(src,std::ios::binary); std::vector<uint8_t> data;
    uint64_t carrier_payload=0;
    for(uint64_t ti=0;ti<tiles;ti++){
        uint64_t rem=total-ti*tile_cells; uint64_t n=std::min<uint64_t>(tile_cells,rem); data.resize(n); in.read((char*)data.data(),n); if((uint64_t)in.gcount()!=n) throw std::runtime_error("short tile");
        std::array<uint64_t,256> tc{}; for(uint8_t b:data) tc[b]++;
        std::vector<uint64_t> nb(internal,0); for(int s=0;s<256;s++) if(tc[s]) for(auto st:tree.path[s]) nb[internal_id[st.node]]+=tc[s];
        std::vector<PackedBits> ps(internal); for(uint32_t j=0;j<internal;j++) ps[j].alloc(nb[j]);
        for(uint8_t b:data) for(auto st:tree.path[b]) ps[internal_id[st.node]].push(st.bit);
        put_u64(out,n); put_u16(out,uint16_t(internal)); uint64_t tile_bytes=0;
        for(uint32_t j=0;j<internal;j++){ auto e=compress_stream(ps[j]); put_var(out,e.n); out.put(char(e.mode)); put_u16(out,e.f1); put_var(out,e.payload.size()); if(!e.payload.empty()) out.write((char*)e.payload.data(),e.payload.size()); tile_bytes+=e.payload.size(); }
        carrier_payload+=tile_bytes; std::cerr<<"V1_TILE="<<(ti+1)<<"/"<<tiles<<" SOURCE="<<n<<" PAYLOAD="<<tile_bytes<<"\n";
    }
    out.flush(); std::ifstream ck(dst,std::ios::binary|std::ios::ate); std::cerr<<"V1_SOURCE_BYTES="<<total<<"\nV1_ARTIFACT_BYTES="<<(uint64_t)ck.tellg()<<"\nV1_STREAM_PAYLOAD="<<carrier_payload<<"\nV1_INTERNAL_NODES="<<internal<<"\n";
}

static void decode(const std::string& src,const std::string& dst){
    std::ifstream in(src,std::ios::binary); if(!in) throw std::runtime_error("open carrier"); char m[4];in.read(m,4); if(in.gcount()!=4||std::string(m,4)!="LCP1") throw std::runtime_error("magic"); if(in.get()!=1) throw std::runtime_error("version"); uint64_t total=get_u64(in); uint32_t rows=get_u32(in),cols=get_u32(in); (void)rows;(void)cols; uint64_t tile_cells=get_u64(in); (void)tile_cells; uint16_t internal=get_u16(in); std::array<uint8_t,256> lengths{}; for(int s=0;s<256;s++){int c=in.get();if(c<0)throw std::runtime_error("short lengths");lengths[s]=uint8_t(c);} auto tree=build_tree(lengths); std::vector<int> internal_id(tree.t.size(),-1); uint16_t got=0; for(size_t i=0;i<tree.t.size();i++) if(tree.t[i].sym<0) internal_id[i]=got++; if(got!=internal) throw std::runtime_error("internal mismatch"); uint32_t tiles=get_u32(in); std::ofstream out(dst,std::ios::binary); if(!out) throw std::runtime_error("open decoded"); uint64_t written=0;
    for(uint32_t ti=0;ti<tiles;ti++){ uint64_t n=get_u64(in); uint16_t ns=get_u16(in); if(ns!=internal) throw std::runtime_error("node count"); std::vector<StreamDec> sd(internal); for(uint16_t j=0;j<internal;j++){ sd[j].n=get_var(in); int mode=in.get(); if(mode<0)throw std::runtime_error("short mode"); sd[j].mode=uint8_t(mode); sd[j].f1=get_u16(in); uint64_t pl=get_var(in); sd[j].payload.resize(pl); if(pl){in.read((char*)sd[j].payload.data(),pl);if((uint64_t)in.gcount()!=pl)throw std::runtime_error("short payload");} sd[j].init(); }
        std::vector<char> buf(1<<20); size_t bp=0; for(uint64_t pos=0;pos<n;pos++){ int node=0; while(tree.t[node].sym<0){int id=internal_id[node];int bit=sd[id].next();node=tree.t[node].child[bit];if(node<0)throw std::runtime_error("bad child");} buf[bp++]=char(uint8_t(tree.t[node].sym)); if(bp==buf.size()){out.write(buf.data(),bp);bp=0;} } if(bp)out.write(buf.data(),bp); written+=n; std::cerr<<"V1_DECODE_TILE="<<(ti+1)<<"/"<<tiles<<" BYTES="<<n<<"\n"; }
    if(written!=total) throw std::runtime_error("size mismatch"); std::cerr<<"V1_RECOVERED_BYTES="<<written<<"\n";
}

static int encoder_action(const std::string& input,
                          const std::string& output,
                          uint64_t tile_cells) {
    std::cerr << "LCCP_ACTION=encode\n";
    std::cerr << "LCCP_INPUT=" << input << "\n";
    std::cerr << "LCCP_OUTPUT=" << output << "\n";
    encode(input, output, tile_cells);
    std::ifstream ck(output, std::ios::binary | std::ios::ate);
    if (!ck) throw std::runtime_error("encoded artifact missing");
    std::cerr << "LCCP_ENCODED_BYTES=" << uint64_t(ck.tellg()) << "\n";
    std::cerr << "LCCP_ACTION_STATUS=PASS\n";
    return 0;
}

static int decoder_action(const std::string& input,
                          const std::string& output) {
    std::cerr << "LCCP_ACTION=decode\n";
    std::cerr << "LCCP_INPUT=" << input << "\n";
    std::cerr << "LCCP_OUTPUT=" << output << "\n";
    decode(input, output);
    std::ifstream ck(output, std::ios::binary | std::ios::ate);
    if (!ck) throw std::runtime_error("decoded output missing");
    std::cerr << "LCCP_DECODED_BYTES=" << uint64_t(ck.tellg()) << "\n";
    std::cerr << "LCCP_ACTION_STATUS=PASS\n";
    return 0;
}

static void print_usage() {
    std::cerr
        << "LCCP v1\n"
        << "  lccp encode <input> <output.lccp> [tile_cells]\n"
        << "  lccp decode <input.lccp> <output>\n"
        << "\n"
        << "Aliases: e = encode, d = decode\n";
}

int main(int argc,char**argv){
    try{
        if(argc<2){ print_usage(); return 2; }
        std::string action=argv[1];
        if(action=="encode" || action=="e"){
            if(argc<4){ print_usage(); return 2; }
            uint64_t tile_cells = argc>4 ? std::stoull(argv[4]) : 100000000ull;
            if(tile_cells==0) throw std::runtime_error("tile_cells must be > 0");
            return encoder_action(argv[2],argv[3],tile_cells);
        }
        if(action=="decode" || action=="d"){
            if(argc<4){ print_usage(); return 2; }
            return decoder_action(argv[2],argv[3]);
        }
        throw std::runtime_error("unknown action: "+action);
    }catch(const std::exception&e){
        std::cerr<<"LCCP_ACTION_STATUS=FAIL\n";
        std::cerr<<"ERROR "<<e.what()<<"\n";
        return 1;
    }
}
