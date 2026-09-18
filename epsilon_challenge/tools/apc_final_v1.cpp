#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void put_u32(std::ostream&o,uint32_t v){for(int i=0;i<4;i++)o.put(char((v>>(8*i))&255));}
static uint32_t get_u32(std::istream&i){uint32_t v=0;for(int k=0;k<4;k++){int c=i.get();if(c<0)throw std::runtime_error("short u32");v|=uint32_t(c)<<(8*k);}return v;}
static void put_u64(std::ostream&o,uint64_t v){for(int i=0;i<8;i++)o.put(char((v>>(8*i))&255));}
static uint64_t get_u64(std::istream&i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}
static void put_var(std::ostream&o,uint64_t v){while(v>=128){o.put(char((v&127)|128));v>>=7;}o.put(char(v));}
static uint64_t get_var(std::istream&i){uint64_t v=0;int sh=0;for(;;){int c=i.get();if(c<0)throw std::runtime_error("short var");v|=uint64_t(c&127)<<sh;if(!(c&128))return v;sh+=7;if(sh>63)throw std::runtime_error("var overflow");}}

struct BitWriter{
    std::vector<uint8_t>b; uint8_t cur=0; int used=0;
    void bit(int x){cur|=uint8_t((x&1)<<(7-used));if(++used==8){b.push_back(cur);cur=0;used=0;}}
    void finish(){if(used){b.push_back(cur);cur=0;used=0;}}
};
struct BitReader{
    const std::vector<uint8_t>&b; size_t p=0; int used=8; uint8_t cur=0;
    explicit BitReader(const std::vector<uint8_t>&x):b(x){}
    int bit(){if(used==8){cur=p<b.size()?b[p++]:0;used=0;}return (cur>>(7-used++))&1;}
};

struct ArithEnc{
    static constexpr uint64_t TOP=0xffffffffULL, HALF=0x80000000ULL, Q1=0x40000000ULL, Q3=0xc0000000ULL;
    uint64_t lo=0,hi=TOP,pending=0; BitWriter w;
    void outbit(int b){w.bit(b);while(pending){w.bit(!b);--pending;}}
    void sym(uint32_t cum,uint32_t freq,uint32_t total){
        if(!freq||!total||cum+freq>total)throw std::runtime_error("bad arithmetic frequency");
        uint64_t range=hi-lo+1;
        hi=lo+(range*(cum+freq))/total-1;
        lo=lo+(range*cum)/total;
        for(;;){
            if(hi<HALF){outbit(0);}
            else if(lo>=HALF){outbit(1);lo-=HALF;hi-=HALF;}
            else if(lo>=Q1&&hi<Q3){++pending;lo-=Q1;hi-=Q1;}
            else break;
            lo=(lo<<1)&TOP; hi=((hi<<1)&TOP)|1;
        }
    }
    std::vector<uint8_t> finish(){
        ++pending;
        if(lo<Q1)outbit(0); else outbit(1);
        w.finish(); return std::move(w.b);
    }
};
struct ArithDec{
    static constexpr uint64_t TOP=0xffffffffULL, HALF=0x80000000ULL, Q1=0x40000000ULL, Q3=0xc0000000ULL;
    uint64_t lo=0,hi=TOP,code=0; BitReader r;
    explicit ArithDec(const std::vector<uint8_t>&b):r(b){for(int i=0;i<32;i++)code=(code<<1)|r.bit();}
    uint32_t target(uint32_t total){
        uint64_t range=hi-lo+1;
        return uint32_t((((code-lo+1)*total)-1)/range);
    }
    void sym(uint32_t cum,uint32_t freq,uint32_t total){
        uint64_t range=hi-lo+1;
        hi=lo+(range*(cum+freq))/total-1;
        lo=lo+(range*cum)/total;
        for(;;){
            if(hi<HALF){}
            else if(lo>=HALF){lo-=HALF;hi-=HALF;code-=HALF;}
            else if(lo>=Q1&&hi<Q3){lo-=Q1;hi-=Q1;code-=Q1;}
            else break;
            lo=(lo<<1)&TOP; hi=((hi<<1)&TOP)|1; code=((code<<1)&TOP)|r.bit();
        }
    }
};

struct Fenwick256{
    std::array<uint32_t,257> f{};
    void clear(){f.fill(0);}
    void add(int idx,int delta){
        for(int i=idx+1;i<=256;i+=i&-i) f[i]=uint32_t(int64_t(f[i])+delta);
    }
    uint32_t prefix(int idx)const{uint32_t s=0;for(int i=idx;i>0;i-=i&-i)s+=f[i];return s;} // [0,idx)
    uint32_t total()const{return prefix(256);}
    int kth(uint32_t k)const{ // zero-based cumulative target
        int idx=0; uint32_t acc=0;
        for(int bit=256;bit;bit>>=1){int nx=idx+bit;if(nx<=256&&acc+f[nx]<=k){idx=nx;acc+=f[nx];}}
        if(idx>=256)throw std::runtime_error("kth range");
        return idx;
    }
};

static void append_var(std::vector<uint8_t>&v,uint64_t x){while(x>=128){v.push_back(uint8_t((x&127)|128));x>>=7;}v.push_back(uint8_t(x));}
static uint64_t read_var(const std::vector<uint8_t>&v,size_t&p){uint64_t x=0;int sh=0;for(;;){if(p>=v.size())throw std::runtime_error("short payload var");uint8_t c=v[p++];x|=uint64_t(c&127)<<sh;if(!(c&128))return x;sh+=7;if(sh>63)throw std::runtime_error("payload var overflow");}}

static std::vector<uint8_t> encode_o0(const std::vector<uint8_t>&s){
    std::array<uint32_t,256> c{};for(uint8_t x:s)c[x]++;
    std::vector<uint8_t> meta; uint32_t nz=0;for(auto x:c)if(x)nz++;append_var(meta,nz);
    Fenwick256 fw;fw.clear();
    for(int x=0;x<256;x++)if(c[x]){meta.push_back(uint8_t(x));append_var(meta,c[x]);fw.add(x,c[x]);}
    ArithEnc ae;
    for(uint8_t x:s){uint32_t tot=fw.total(),cum=fw.prefix(x),fr=c[x];ae.sym(cum,fr,tot);fw.add(x,-1);c[x]--;}
    auto bits=ae.finish();append_var(meta,bits.size());meta.insert(meta.end(),bits.begin(),bits.end());return meta;
}
static std::vector<uint8_t> decode_o0(const std::vector<uint8_t>&p,size_t n){
    size_t q=0;uint64_t nz=read_var(p,q);std::array<uint32_t,256> c{};Fenwick256 fw;fw.clear();
    for(uint64_t i=0;i<nz;i++){if(q>=p.size())throw std::runtime_error("o0 symbol");int x=p[q++];uint64_t z=read_var(p,q);if(!z||z>0xffffffffu)throw std::runtime_error("o0 count");c[x]=uint32_t(z);fw.add(x,c[x]);}
    uint64_t bl=read_var(p,q);if(q+bl!=p.size())throw std::runtime_error("o0 bit length");std::vector<uint8_t>bits(p.begin()+q,p.end());ArithDec ad(bits);
    std::vector<uint8_t>out;out.reserve(n);
    for(size_t i=0;i<n;i++){uint32_t tot=fw.total();if(!tot)throw std::runtime_error("o0 exhausted");uint32_t t=ad.target(tot);int x=fw.kth(t);uint32_t cum=fw.prefix(x),fr=c[x];ad.sym(cum,fr,tot);out.push_back(uint8_t(x));fw.add(x,-1);c[x]--;}
    return out;
}

static std::vector<uint8_t> encode_o1(const std::vector<uint8_t>&s){
    if(s.empty())return {};
    std::array<uint32_t,65536> c{};std::array<uint32_t,256> row{};
    for(size_t i=1;i<s.size();i++){uint32_t k=(uint32_t(s[i-1])<<8)|s[i];c[k]++;row[s[i-1]]++;}
    std::vector<uint8_t> meta;meta.push_back(s[0]);uint32_t contexts=0;for(auto x:row)if(x)contexts++;append_var(meta,contexts);
    std::array<Fenwick256,256> fw;
    for(int a=0;a<256;a++){fw[a].clear();if(!row[a])continue;meta.push_back(uint8_t(a));uint32_t deg=0;for(int b=0;b<256;b++)if(c[(a<<8)|b])deg++;append_var(meta,deg);for(int b=0;b<256;b++){uint32_t z=c[(a<<8)|b];if(z){meta.push_back(uint8_t(b));append_var(meta,z);fw[a].add(b,z);}}}
    ArithEnc ae;
    for(size_t i=1;i<s.size();i++){int a=s[i-1],b=s[i];uint32_t tot=fw[a].total(),fr=c[(a<<8)|b],cum=fw[a].prefix(b);ae.sym(cum,fr,tot);fw[a].add(b,-1);c[(a<<8)|b]--;}
    auto bits=ae.finish();append_var(meta,bits.size());meta.insert(meta.end(),bits.begin(),bits.end());return meta;
}
static std::vector<uint8_t> decode_o1(const std::vector<uint8_t>&p,size_t n){
    if(!n)return {};
    size_t q=0;if(q>=p.size())throw std::runtime_error("o1 first");uint8_t first=p[q++];uint64_t contexts=read_var(p,q);
    std::array<uint32_t,65536> c{};std::array<Fenwick256,256> fw;for(auto &x:fw)x.clear();
    for(uint64_t ci=0;ci<contexts;ci++){if(q>=p.size())throw std::runtime_error("o1 context");int a=p[q++];uint64_t deg=read_var(p,q);for(uint64_t j=0;j<deg;j++){if(q>=p.size())throw std::runtime_error("o1 edge");int b=p[q++];uint64_t z=read_var(p,q);if(!z||z>0xffffffffu)throw std::runtime_error("o1 count");c[(a<<8)|b]=uint32_t(z);fw[a].add(b,uint32_t(z));}}
    uint64_t bl=read_var(p,q);if(q+bl!=p.size())throw std::runtime_error("o1 bit length");std::vector<uint8_t>bits(p.begin()+q,p.end());ArithDec ad(bits);
    std::vector<uint8_t>out;out.reserve(n);out.push_back(first);
    for(size_t i=1;i<n;i++){int a=out.back();uint32_t tot=fw[a].total();if(!tot)throw std::runtime_error("o1 exhausted");uint32_t t=ad.target(tot);int b=fw[a].kth(t);uint32_t cum=fw[a].prefix(b),fr=c[(a<<8)|b];ad.sym(cum,fr,tot);out.push_back(uint8_t(b));fw[a].add(b,-1);c[(a<<8)|b]--;}
    return out;
}

static uint64_t fsize(const std::string&p){std::ifstream i(p,std::ios::binary|std::ios::ate);if(!i)throw std::runtime_error("size open");return uint64_t(i.tellg());}

static void encode_file(const std::string&src,const std::string&dst,uint32_t page){
    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open source");
    uint64_t total=fsize(src),pages=(total+page-1)/page;
    std::ofstream out(dst,std::ios::binary);if(!out)throw std::runtime_error("open carrier");
    out.write("APC1",4);out.put(1);put_u64(out,total);put_u32(out,page);put_u32(out,uint32_t(pages));
    std::vector<uint8_t>s(page);uint64_t rawc=0,o0c=0,o1c=0;
    for(uint64_t pi=0;pi<pages;pi++){
        size_t n=size_t(std::min<uint64_t>(page,total-pi*page));in.read((char*)s.data(),n);if((size_t)in.gcount()!=n)throw std::runtime_error("short source");s.resize(n);
        auto a=encode_o0(s);auto b=encode_o1(s);
        uint8_t mode=0;const std::vector<uint8_t>*pay=nullptr;size_t best=n;
        if(a.size()<best){mode=1;pay=&a;best=a.size();}
        if(b.size()<best){mode=2;pay=&b;best=b.size();}
        out.put(char(mode));put_var(out,best);
        if(mode==0){out.write((char*)s.data(),n);rawc++;}
        else{out.write((char*)pay->data(),pay->size());if(mode==1)o0c++;else o1c++;}
        std::cerr<<"APC_PAGE="<<pi<<" MODE="<<int(mode)<<" SOURCE="<<n<<" PAYLOAD="<<best<<"\n";
        s.resize(page);
    }
    out.flush();std::cerr<<"APC_SOURCE_BYTES="<<total<<"\nAPC_CARRIER_BYTES="<<fsize(dst)<<"\nAPC_RAW_PAGES="<<rawc<<"\nAPC_O0_PAGES="<<o0c<<"\nAPC_O1_PAGES="<<o1c<<"\n";
}
static void decode_file(const std::string&src,const std::string&dst){
    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open carrier");char m[4];in.read(m,4);if(in.gcount()!=4||std::string(m,4)!="APC1")throw std::runtime_error("magic");if(in.get()!=1)throw std::runtime_error("version");
    uint64_t total=get_u64(in);uint32_t page=get_u32(in),pages=get_u32(in);std::ofstream out(dst,std::ios::binary);if(!out)throw std::runtime_error("open output");
    uint64_t written=0;
    for(uint32_t pi=0;pi<pages;pi++){
        int mode=in.get();if(mode<0||mode>2)throw std::runtime_error("mode");uint64_t pl=get_var(in);std::vector<uint8_t>p(pl);if(pl){in.read((char*)p.data(),pl);if((uint64_t)in.gcount()!=pl)throw std::runtime_error("short payload");}
        size_t n=size_t(std::min<uint64_t>(page,total-written));std::vector<uint8_t>s;
        if(mode==0){if(pl!=n)throw std::runtime_error("raw page length");s=std::move(p);}
        else if(mode==1)s=decode_o0(p,n);else s=decode_o1(p,n);
        if(s.size()!=n)throw std::runtime_error("page decode size");out.write((char*)s.data(),s.size());written+=s.size();
    }
    if(written!=total)throw std::runtime_error("total size");if(in.peek()!=EOF)throw std::runtime_error("trailing bytes");
    std::cerr<<"APC_RECOVERED_BYTES="<<written<<"\n";
}
int main(int argc,char**argv){
    try{
        if(argc<4){std::cerr<<"usage: apc_final e input output.apc [page_bytes=1000000]\n       apc_final d input.apc output\n";return 2;}
        std::string a=argv[1];if(a=="e"||a=="encode"){uint32_t p=argc>4?uint32_t(std::stoul(argv[4])):1000000u;if(!p||p>100000000u)throw std::runtime_error("page");encode_file(argv[2],argv[3],p);}
        else if(a=="d"||a=="decode")decode_file(argv[2],argv[3]);else throw std::runtime_error("action");
    }catch(const std::exception&e){std::cerr<<"APC_ERROR "<<e.what()<<"\n";return 1;}return 0;
}
