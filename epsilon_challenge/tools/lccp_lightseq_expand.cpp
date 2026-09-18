#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

static void put_u32(std::ostream& o,uint32_t v){for(int k=0;k<4;k++)o.put(char((v>>(8*k))&255));}
static uint32_t get_u32(std::istream& i){uint32_t v=0;for(int k=0;k<4;k++){int c=i.get();if(c<0)throw std::runtime_error("short u32");v|=uint32_t(c)<<(8*k);}return v;}
static void put_u64(std::ostream& o,uint64_t v){for(int k=0;k<8;k++)o.put(char((v>>(8*k))&255));}
static uint64_t get_u64(std::istream& i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}
static void put_var(std::ostream& o,uint64_t v){while(v>=128){o.put(char((v&127)|128));v>>=7;}o.put(char(v));}
static uint64_t get_var(std::istream& i){uint64_t v=0;int sh=0;for(;;){int c=i.get();if(c<0)throw std::runtime_error("short varint");v|=uint64_t(c&127)<<sh;if(!(c&128))return v;sh+=7;if(sh>63)throw std::runtime_error("varint overflow");}}

struct Key{
    uint64_t a=0,b=0;
    uint32_t n=0;
    bool operator==(const Key& o)const{return a==o.a&&b==o.b&&n==o.n;}
};
struct KeyHash{
    size_t operator()(const Key& k)const{
        uint64_t x=k.a^(k.b+0x9e3779b97f4a7c15ULL+(k.a<<6)+(k.a>>2))^(uint64_t(k.n)*0xbf58476d1ce4e5b9ULL);
        x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;x^=x>>31;
        return size_t(x);
    }
};
static Key token_key(const std::vector<uint8_t>& s){
    uint64_t h1=1469598103934665603ULL;
    uint64_t h2=0x9e3779b97f4a7c15ULL;
    for(uint8_t c:s){
        h1^=c; h1*=1099511628211ULL;
        h2^=uint64_t(c)+0x9e3779b97f4a7c15ULL+(h2<<6)+(h2>>2);
        h2*=0xbf58476d1ce4e5b9ULL;
    }
    return {h1,h2,uint32_t(s.size())};
}

struct BitWriter{
    std::ofstream out;
    uint8_t cur=0;
    int used=0;
    uint64_t bits=0,bytes=0;
    explicit BitWriter(const std::string& p):out(p,std::ios::binary){if(!out)throw std::runtime_error("open bit tmp");}
    void bit(int x){cur|=uint8_t((x&1)<<(7-used));used++;bits++;if(used==8){out.put(char(cur));bytes++;cur=0;used=0;}}
    void bits_msb(uint64_t v,int n){for(int i=n-1;i>=0;i--)bit((v>>i)&1);}
    void gamma(uint64_t x){
        if(!x)throw std::runtime_error("gamma zero");
        int l=63-__builtin_clzll(x);
        for(int i=0;i<l;i++)bit(0);
        bits_msb(x,l+1);
    }
    void finish(){if(used){out.put(char(cur));bytes++;cur=0;used=0;}out.flush();}
};

struct BitReader{
    std::ifstream in;
    uint8_t cur=0;
    int used=8;
    uint64_t bits_left=0;
    BitReader(const std::string& p,uint64_t off,uint64_t bits):in(p,std::ios::binary),bits_left(bits){
        if(!in)throw std::runtime_error("open bit reader");
        in.seekg((std::streamoff)off);
        if(!in)throw std::runtime_error("seek bit reader");
    }
    int bit(){
        if(!bits_left)throw std::runtime_error("bitstream exhausted");
        if(used==8){int c=in.get();if(c<0)throw std::runtime_error("short bitstream");cur=uint8_t(c);used=0;}
        int x=(cur>>(7-used))&1;used++;bits_left--;return x;
    }
    uint64_t gamma(){
        int z=0;
        while(bit()==0){if(++z>63)throw std::runtime_error("gamma overflow");}
        uint64_t x=1;
        for(int i=0;i<z;i++)x=(x<<1)|bit();
        return x;
    }
};

struct HNode{uint64_t f=0;int l=-1,r=-1,s=-1;};
struct HQ{uint64_t f;int s;int idx;};
struct HCmp{bool operator()(const HQ&a,const HQ&b)const{return a.f!=b.f?a.f>b.f:a.s>b.s;}};

static std::vector<uint8_t> huffman_lengths(const std::vector<uint64_t>& freq){
    std::vector<uint8_t> len(freq.size(),0);
    std::priority_queue<HQ,std::vector<HQ>,HCmp> q;
    std::vector<HNode> nodes;
    for(int s=0;s<(int)freq.size();s++)if(freq[s]){
        int i=nodes.size();nodes.push_back({freq[s],-1,-1,s});q.push({freq[s],s,i});
    }
    if(q.empty())return len;
    if(q.size()==1){len[nodes[q.top().idx].s]=1;return len;}
    int serial=(int)freq.size();
    while(q.size()>1){
        auto a=q.top();q.pop();auto b=q.top();q.pop();
        int i=nodes.size();nodes.push_back({a.f+b.f,a.idx,b.idx,-1});
        q.push({a.f+b.f,serial++,i});
    }
    std::vector<std::pair<int,int>> st{{q.top().idx,0}};
    while(!st.empty()){
        auto [i,d]=st.back();st.pop_back();
        if(nodes[i].s>=0){
            if(d<=0||d>63)throw std::runtime_error("Huffman code exceeds 63 bits");
            len[nodes[i].s]=uint8_t(d);
        }else{
            st.push_back({nodes[i].r,d+1});
            st.push_back({nodes[i].l,d+1});
        }
    }
    return len;
}

struct Code{uint64_t code=0;uint8_t len=0;};
static std::vector<Code> canonical_codes(const std::vector<uint8_t>& lens){
    std::vector<std::pair<int,int>> ord;
    for(int s=0;s<(int)lens.size();s++)if(lens[s])ord.push_back({lens[s],s});
    std::sort(ord.begin(),ord.end());
    std::vector<Code> out(lens.size());
    uint64_t code=0;int prev=0;
    for(auto [l,s]:ord){
        if(l>prev)code<<=(l-prev);
        out[s]={code,uint8_t(l)};
        code++;prev=l;
    }
    return out;
}

struct DNode{int ch[2]={-1,-1};int sym=-1;};
static std::vector<DNode> decode_tree(const std::vector<Code>& codes){
    std::vector<DNode> t(1);
    for(int s=0;s<(int)codes.size();s++)if(codes[s].len){
        int n=0;
        for(int k=codes[s].len-1;k>=0;k--){
            int b=(codes[s].code>>k)&1;
            if(t[n].ch[b]<0){t[n].ch[b]=(int)t.size();t.push_back({});}
            n=t[n].ch[b];
        }
        if(t[n].sym>=0)throw std::runtime_error("duplicate Huffman code");
        t[n].sym=s;
    }
    return t;
}
static int decode_symbol(BitReader& br,const std::vector<DNode>& t){
    int n=0;
    while(t[n].sym<0){
        int b=br.bit();
        n=t[n].ch[b];
        if(n<0)throw std::runtime_error("invalid Huffman stream");
    }
    return t[n].sym;
}

static int g_seg_mode=1;

static int byte_class(uint8_t b){
    if((b>='A'&&b<='Z')||(b>='a'&&b<='z')) return 1;
    if(b>='0'&&b<='9') return 2;
    if(b=='_'||b=='-') return 3;
    return 4;
}

template<class F>
static uint64_t scan_tokens(const std::string& path,uint8_t def,F cb,uint64_t* trailing_out=nullptr){
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("open input");
    std::vector<char> buf(1<<20);
    std::vector<uint8_t> tok;
    tok.reserve(1024);
    uint64_t gap=0,current_gap=0,events=0;
    const size_t MAX_SEG=65535;
    int cls=-1;

    auto flush=[&](){
        if(tok.empty()) return;
        cb(tok,current_gap);
        events++;
        tok.clear();
        current_gap=0;
        cls=-1;
    };

    while(in){
        in.read(buf.data(),buf.size());
        size_t n=(size_t)in.gcount();
        for(size_t i=0;i<n;i++){
            uint8_t b=uint8_t(buf[i]);
            if(b==def){
                if(!tok.empty()){ flush(); gap=1; }
                else gap++;
                continue;
            }

            int bc = g_seg_mode==0 ? 0 : byte_class(b);
            if(tok.empty()){
                current_gap=gap; gap=0; cls=bc;
            }else if(g_seg_mode!=0 && bc!=cls){
                flush();
                current_gap=0; cls=bc;
            }

            tok.push_back(b);
            if(tok.size()==MAX_SEG) flush();
        }
    }
    if(!tok.empty()){flush();gap=0;}
    if(trailing_out)*trailing_out=gap;
    return events;
}
static std::pair<uint8_t,uint64_t> choose_default(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("open input");
    std::array<uint64_t,256> c{};
    std::vector<char> b(1<<20);
    uint64_t n=0;
    while(in){in.read(b.data(),b.size());size_t z=(size_t)in.gcount();n+=z;for(size_t i=0;i<z;i++)c[uint8_t(b[i])]++;}
    int best=0;
    for(int i=1;i<256;i++)if(c[i]>c[best])best=i;
    return {uint8_t(best),n};
}

static void copy_file_to(std::ostream& out,const std::string& p){
    std::ifstream in(p,std::ios::binary);
    if(!in)throw std::runtime_error("open temp");
    std::vector<char> b(1<<20);
    while(in){in.read(b.data(),b.size());size_t n=(size_t)in.gcount();if(n)out.write(b.data(),n);}
}
static uint64_t fsize(const std::string& p){
    std::ifstream in(p,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("size open");return uint64_t(in.tellg());
}

static void write_raw(const std::string& src,const std::string& dst,uint64_t n){
    std::ofstream out(dst,std::ios::binary|std::ios::trunc);
    out.write("LLS1",4);out.put(1);out.put(0);put_u64(out,n);
    copy_file_to(out,src);
}

static void encode(const std::string& src,const std::string& dst){
    auto [def,total]=choose_default(src);
    const uint32_t MAX_DICT=262144;
    const uint32_t MAX_CAND_LEN=96;

    std::unordered_map<Key,uint32_t,KeyHash> cnt;
    cnt.reserve(1<<20);
    scan_tokens(src,def,[&](const std::vector<uint8_t>& t,uint64_t){
        if(t.size()<=MAX_CAND_LEN){
            Key k=token_key(t);
            auto it=cnt.find(k);
            if(it==cnt.end())cnt.emplace(k,1);
            else if(it->second!=0xffffffffu)it->second++;
        }
    });

    struct Cand{Key k;uint32_t c;uint64_t score;};
    std::vector<Cand> cand;
    cand.reserve(cnt.size()/8+1);
    for(auto &kv:cnt)if(kv.second>=2){
        uint64_t score=uint64_t(kv.second-1)*kv.first.n;
        if(score>kv.first.n+8)cand.push_back({kv.first,kv.second,score});
    }
    if(cand.size()>MAX_DICT){
        std::nth_element(cand.begin(),cand.begin()+MAX_DICT,cand.end(),[](const Cand&a,const Cand&b){return a.score>b.score;});
        cand.resize(MAX_DICT);
    }
    std::sort(cand.begin(),cand.end(),[](const Cand&a,const Cand&b){return a.score>b.score;});

    std::unordered_map<Key,uint32_t,KeyHash> id;
    id.reserve(cand.size()*2+1);
    for(uint32_t i=0;i<cand.size();i++)id[cand[i].k]=i;
    std::vector<std::vector<uint8_t>> dict(cand.size());

    scan_tokens(src,def,[&](const std::vector<uint8_t>& t,uint64_t){
        if(t.size()>MAX_CAND_LEN)return;
        auto it=id.find(token_key(t));
        if(it!=id.end()&&dict[it->second].empty())dict[it->second]=t;
    });

    // Remove unresolved collision-only candidates.
    std::vector<std::vector<uint8_t>> d2;
    d2.reserve(dict.size());
    for(auto &d:dict)if(!d.empty())d2.push_back(std::move(d));
    dict.swap(d2);
    id.clear();id.reserve(dict.size()*2+1);
    for(uint32_t i=0;i<dict.size();i++)id[token_key(dict[i])]=i;

    uint32_t ESC=(uint32_t)dict.size();
    std::vector<uint64_t> freq(dict.size()+1,0);
    uint64_t events=0,trailing=0,leading=0;
    bool first=true;
    events=scan_tokens(src,def,[&](const std::vector<uint8_t>& t,uint64_t gap){
        if(first){leading=gap;first=false;}
        auto it=id.find(token_key(t));
        bool hit=false;
        if(it!=id.end()&&dict[it->second]==t){freq[it->second]++;hit=true;}
        if(!hit)freq[ESC]++;
    },&trailing);

    auto lens=huffman_lengths(freq);
    auto codes=canonical_codes(lens);

    std::string tokp=dst+".tok.tmp",gapp=dst+".gap.tmp",litp=dst+".lit.tmp";
    BitWriter tokbw(tokp),gapbw(gapp);
    std::ofstream lit(litp,std::ios::binary);
    if(!lit)throw std::runtime_error("open literal tmp");

    uint64_t seen=0,literal_bytes=0,dict_hits=0,literal_events=0;
    scan_tokens(src,def,[&](const std::vector<uint8_t>& t,uint64_t gap){
        if(seen>0){
            if(gap==1)gapbw.bit(0);
            else{gapbw.bit(1);gapbw.gamma(gap+1);}
        }
        auto it=id.find(token_key(t));
        uint32_t sym=ESC;
        if(it!=id.end()&&dict[it->second]==t){sym=it->second;dict_hits++;}
        else{literal_events++;put_var(lit,t.size());if(!t.empty())lit.write((char*)t.data(),t.size());literal_bytes+=t.size();}
        Code c=codes[sym];
        if(!c.len)throw std::runtime_error("missing Huffman code");
        tokbw.bits_msb(c.code,c.len);
        seen++;
    });
    tokbw.finish();gapbw.finish();lit.flush();lit.close();

    uint64_t tokbytes=fsize(tokp),gapbytes=fsize(gapp),litbytes=fsize(litp);

    std::string comp=dst+".cmp.tmp";
    {
        std::ofstream out(comp,std::ios::binary);
        if(!out)throw std::runtime_error("open compressed tmp");
        out.write("LLS1",4);out.put(1);out.put(1);
        put_u64(out,total);out.put(char(def));
        put_u32(out,(uint32_t)dict.size());
        for(auto &d:dict){put_var(out,d.size());if(!d.empty())out.write((char*)d.data(),d.size());}
        put_u32(out,(uint32_t)lens.size());
        if(!lens.empty())out.write((char*)lens.data(),lens.size());
        put_u64(out,events);put_var(out,leading);put_var(out,trailing);
        put_u64(out,tokbw.bits);put_u64(out,tokbytes);
        put_u64(out,gapbw.bits);put_u64(out,gapbytes);
        put_u64(out,litbytes);
        copy_file_to(out,tokp);copy_file_to(out,gapp);copy_file_to(out,litp);
    }
    uint64_t compsize=fsize(comp);
    uint64_t rawsize=4+1+1+8+total;
    if(compsize<rawsize){
        std::remove(dst.c_str());
        if(std::rename(comp.c_str(),dst.c_str())!=0)throw std::runtime_error("rename compressed carrier");
        std::cerr<<"LIGHTSEQ_BRANCH=COMPRESSED\n";
    }else{
        write_raw(src,dst,total);
        std::remove(comp.c_str());
        std::cerr<<"LIGHTSEQ_BRANCH=RAW\n";
    }
    std::remove(tokp.c_str());std::remove(gapp.c_str());std::remove(litp.c_str());

    std::cerr<<"LIGHTSEQ_SEG_MODE="<<g_seg_mode<<"\n";
    std::cerr<<"LIGHTSEQ_SOURCE_BYTES="<<total<<"\n";
    std::cerr<<"LIGHTSEQ_DEFAULT_BYTE="<<unsigned(def)<<"\n";
    std::cerr<<"LIGHTSEQ_DICTIONARY_CONFIGS="<<dict.size()<<"\n";
    std::cerr<<"LIGHTSEQ_EVENTS="<<events<<"\n";
    std::cerr<<"LIGHTSEQ_DICT_HITS="<<dict_hits<<"\n";
    std::cerr<<"LIGHTSEQ_LITERAL_EVENTS="<<literal_events<<"\n";
    std::cerr<<"LIGHTSEQ_LITERAL_RAW_BYTES="<<literal_bytes<<"\n";
    std::cerr<<"LIGHTSEQ_TOKEN_BITS="<<tokbw.bits<<"\n";
    std::cerr<<"LIGHTSEQ_GAP_BITS="<<gapbw.bits<<"\n";
    std::cerr<<"LIGHTSEQ_COMPRESSED_CANDIDATE_BYTES="<<compsize<<"\n";
    std::cerr<<"LIGHTSEQ_ARTIFACT_BYTES="<<fsize(dst)<<"\n";
}

static void decode(const std::string& src,const std::string& dst){
    std::ifstream in(src,std::ios::binary);
    if(!in)throw std::runtime_error("open carrier");
    char m[4];in.read(m,4);
    if(in.gcount()!=4||std::string(m,4)!="LLS1")throw std::runtime_error("magic");
    if(in.get()!=1)throw std::runtime_error("version");
    int mode=in.get();if(mode<0)throw std::runtime_error("mode");
    uint64_t total=get_u64(in);
    std::ofstream out(dst,std::ios::binary);
    if(!out)throw std::runtime_error("open output");

    if(mode==0){
        std::vector<char> b(1<<20);uint64_t rem=total;
        while(rem){size_t want=(size_t)std::min<uint64_t>(b.size(),rem);in.read(b.data(),want);if((size_t)in.gcount()!=want)throw std::runtime_error("short raw");out.write(b.data(),want);rem-=want;}
        if(in.peek()!=EOF)throw std::runtime_error("trailing raw carrier");
        std::cerr<<"LIGHTSEQ_RECOVERED_BYTES="<<total<<"\n";return;
    }
    if(mode!=1)throw std::runtime_error("unknown branch");
    int dc=in.get();if(dc<0)throw std::runtime_error("default");uint8_t def=uint8_t(dc);
    uint32_t D=get_u32(in);
    std::vector<std::vector<uint8_t>> dict(D);
    for(uint32_t i=0;i<D;i++){uint64_t n=get_var(in);dict[i].resize(n);if(n){in.read((char*)dict[i].data(),n);if((uint64_t)in.gcount()!=n)throw std::runtime_error("short dictionary");}}
    uint32_t L=get_u32(in);
    if(L!=D+1)throw std::runtime_error("code alphabet mismatch");
    std::vector<uint8_t> lens(L);if(L){in.read((char*)lens.data(),L);if((uint32_t)in.gcount()!=L)throw std::runtime_error("short code lengths");}
    auto codes=canonical_codes(lens);auto tree=decode_tree(codes);
    uint64_t events=get_u64(in),leading=get_var(in),trailing=get_var(in);
    uint64_t tokbits=get_u64(in),tokbytes=get_u64(in);
    uint64_t gapbits=get_u64(in),gapbytes=get_u64(in);
    uint64_t litbytes=get_u64(in);
    uint64_t header_end=(uint64_t)in.tellg();
    uint64_t tokoff=header_end,gapoff=tokoff+tokbytes,litoff=gapoff+gapbytes;
    if(litoff+litbytes!=fsize(src))throw std::runtime_error("carrier section size mismatch");

    BitReader tokbr(src,tokoff,tokbits),gapbr(src,gapoff,gapbits);
    std::ifstream litr(src,std::ios::binary);litr.seekg((std::streamoff)litoff);if(!litr)throw std::runtime_error("seek literals");
    uint64_t written=0;
    auto emit_default=[&](uint64_t n){
        std::array<char,8192> b{};b.fill(char(def));
        while(n){size_t z=(size_t)std::min<uint64_t>(n,b.size());out.write(b.data(),z);written+=z;n-=z;}
    };
    emit_default(leading);
    for(uint64_t e=0;e<events;e++){
        int sym=decode_symbol(tokbr,tree);
        if(sym<(int)D){
            auto &d=dict[sym];if(!d.empty())out.write((char*)d.data(),d.size());written+=d.size();
        }else if(sym==(int)D){
            uint64_t n=get_var(litr);
            std::vector<char> b((size_t)n);
            if(n){litr.read(b.data(),n);if((uint64_t)litr.gcount()!=n)throw std::runtime_error("short literal");out.write(b.data(),n);}
            written+=n;
        }else throw std::runtime_error("bad symbol");
        if(e+1<events){
            int special=gapbr.bit();
            uint64_t gap=special?(gapbr.gamma()-1):1;
            emit_default(gap);
        }
    }
    emit_default(trailing);
    if(written!=total)throw std::runtime_error("decoded size mismatch");
    std::cerr<<"LIGHTSEQ_RECOVERED_BYTES="<<written<<"\n";
}

int main(int argc,char**argv){
    try{
        if(argc<4){
            std::cerr<<"usage: lccp_lightseq_expand encode input output.lls [segment_mode]\n"
                     <<"       segment_mode 0=whole non-default runs, 1=word/symbol architecture\n"
                     <<"       lccp_lightseq_expand decode input.lls output\n";
            return 2;
        }
        std::string a=argv[1];
        if(a=="encode"||a=="e"){
            if(argc>4) g_seg_mode=std::stoi(argv[4]);
            if(g_seg_mode<0||g_seg_mode>1) throw std::runtime_error("segment_mode");
            encode(argv[2],argv[3]);
        }else if(a=="decode"||a=="d")decode(argv[2],argv[3]);
        else throw std::runtime_error("unknown action");
    }catch(const std::exception&e){
        std::cerr<<"ERROR "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
