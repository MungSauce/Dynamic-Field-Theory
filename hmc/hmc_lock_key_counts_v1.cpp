#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

static void put_u64(std::ostream& o,uint64_t v){for(int i=0;i<8;i++)o.put(char((v>>(8*i))&255));}
static uint64_t get_u64(std::istream& i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}
static void put_var(std::ostream& o,uint64_t v){while(v>=128){o.put(char((v&127)|128));v>>=7;}o.put(char(v));}
static uint64_t get_var(std::istream& i){uint64_t v=0;int s=0;for(;;){int c=i.get();if(c<0)throw std::runtime_error("short varint");v|=uint64_t(c&127)<<s;if(!(c&128))return v;s+=7;if(s>63)throw std::runtime_error("varint overflow");}}
static uint64_t fsize(const std::string&p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("size "+p);return uint64_t(f.tellg());}
static std::vector<uint8_t> read_all(const std::string&p){uint64_t n=fsize(p);std::vector<uint8_t>v((size_t)n);std::ifstream f(p,std::ios::binary);if(n){f.read((char*)v.data(),(std::streamsize)n);if((uint64_t)f.gcount()!=n)throw std::runtime_error("short read");}return v;}

static uint64_t mask_for(int k){return k==7?0x00ffffffffffffffULL:((1ULL<<(8*k))-1);}
static uint64_t seed_ctx(const std::vector<uint8_t>&d,int k){uint64_t c=0;for(int i=0;i<k;i++)c=(c<<8)|d[i];return c;}
static uint64_t step_ctx(uint64_t ctx,uint8_t c,uint64_t mask){return ((ctx<<8)|c)&mask;}
static uint64_t pk(uint64_t ctx,uint8_t c){return (ctx<<8)|c;}

struct LockState{
    int k=0; uint64_t n=0,mask=0; std::vector<uint8_t> seed;
    std::unordered_map<uint64_t,uint32_t> cnt;
    std::unordered_map<uint64_t,uint16_t> active;
    std::unordered_map<uint64_t,uint8_t> single;
};

static void init_active(LockState&s){
    s.active.reserve(s.cnt.size()/2+1);
    for(auto &kv:s.cnt) if(kv.second) s.active[kv.first>>8]++;
    s.single.reserve(s.active.size()/4+1);
    for(auto &kv:s.active) if(kv.second==1){
        uint64_t ctx=kv.first;
        for(int c=0;c<256;c++){auto it=s.cnt.find(pk(ctx,(uint8_t)c));if(it!=s.cnt.end()&&it->second){s.single[ctx]=(uint8_t)c;break;}}
    }
}
static void consume(LockState&s,uint64_t ctx,uint8_t c){
    auto it=s.cnt.find(pk(ctx,c));
    if(it==s.cnt.end()||it->second==0)throw std::runtime_error("invalid transition");
    if(--it->second==0){
        auto ai=s.active.find(ctx);if(ai==s.active.end()||ai->second==0)throw std::runtime_error("active underflow");
        ai->second--;
        if(ai->second==1){
            for(int x=0;x<256;x++){auto jt=s.cnt.find(pk(ctx,(uint8_t)x));if(jt!=s.cnt.end()&&jt->second){s.single[ctx]=(uint8_t)x;break;}}
        }else if(ai->second==0)s.single.erase(ctx);
    }
}

static void save_lock(const std::string&path,const LockState&s){
    std::ofstream o(path,std::ios::binary|std::ios::trunc);if(!o)throw std::runtime_error("lock out");
    o.write("LKH1",4);o.put(char(s.k));put_u64(o,s.n);o.write((const char*)s.seed.data(),s.seed.size());
    std::vector<std::pair<uint64_t,uint32_t>> e;e.reserve(s.cnt.size());
    for(auto &kv:s.cnt)e.push_back(kv);
    std::sort(e.begin(),e.end(),[](auto&a,auto&b){return a.first<b.first;});
    put_u64(o,e.size());uint64_t prev=0;
    for(auto &kv:e){put_var(o,kv.first-prev);put_var(o,kv.second);prev=kv.first;}
}
static LockState load_lock(const std::string&path){
    std::ifstream i(path,std::ios::binary);if(!i)throw std::runtime_error("lock in");
    char m[4];i.read(m,4);if(i.gcount()!=4||std::string(m,4)!="LKH1")throw std::runtime_error("magic");
    LockState s;s.k=i.get();if(s.k<1||s.k>7)throw std::runtime_error("k");s.n=get_u64(i);s.mask=mask_for(s.k);
    s.seed.resize(s.k);i.read((char*)s.seed.data(),s.k);if(i.gcount()!=s.k)throw std::runtime_error("seed");
    uint64_t ne=get_u64(i);s.cnt.reserve((size_t)(ne*1.3)+1);uint64_t cur=0;
    for(uint64_t j=0;j<ne;j++){cur+=get_var(i);uint64_t v=get_var(i);if(!v||v>0xffffffffULL)throw std::runtime_error("count");s.cnt[cur]=(uint32_t)v;}
    init_active(s);return s;
}

static void build(const std::string&src,const std::string&lockp,const std::string&keyp,int k){
    auto d=read_all(src);if(d.size()<(size_t)k)throw std::runtime_error("source too short");
    LockState s;s.k=k;s.n=d.size();s.mask=mask_for(k);s.seed.assign(d.begin(),d.begin()+k);
    size_t transitions=d.size()-k;
    s.cnt.reserve((size_t)(transitions*0.45)+1024);
    uint64_t ctx=seed_ctx(d,k);
    for(size_t p=k;p<d.size();p++){uint8_t c=d[p];auto &v=s.cnt[pk(ctx,c)];if(v==0xffffffffu)throw std::runtime_error("count overflow");v++;ctx=step_ctx(ctx,c,s.mask);}
    save_lock(lockp,s);
    init_active(s);
    std::ofstream key(keyp,std::ios::binary|std::ios::trunc);if(!key)throw std::runtime_error("key out");
    ctx=seed_ctx(d,k);uint64_t forced=0,ambig=0;
    for(size_t p=k;p<d.size();p++){
        auto ai=s.active.find(ctx);if(ai==s.active.end()||ai->second==0)throw std::runtime_error("dead lock");
        uint8_t c=d[p];
        if(ai->second==1){
            auto si=s.single.find(ctx);if(si==s.single.end())throw std::runtime_error("missing single");
            if(si->second!=c)throw std::runtime_error("forced mismatch");
            forced++;
        }else{key.put(char(c));ambig++;}
        consume(s,ctx,c);ctx=step_ctx(ctx,c,s.mask);
    }
    key.flush();
    std::cerr<<"LK_SOURCE_BYTES="<<d.size()<<"\nLK_ORDER="<<k<<"\nLK_LOCK_BYTES="<<fsize(lockp)<<"\nLK_KEY_BYTES="<<fsize(keyp)<<"\nLK_FORCED="<<forced<<"\nLK_AMBIG="<<ambig<<"\nLK_FORCED_FRACTION="<<(double)forced/(double)(d.size()-k)<<"\n";
}
static void decode(const std::string&lockp,const std::string&keyp,const std::string&outp){
    LockState s=load_lock(lockp);std::ifstream key(keyp,std::ios::binary);if(!key)throw std::runtime_error("key in");
    std::ofstream out(outp,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("out");
    out.write((const char*)s.seed.data(),s.seed.size());uint64_t ctx=seed_ctx(s.seed,s.k),forced=0,ambig=0;
    for(uint64_t p=s.k;p<s.n;p++){
        auto ai=s.active.find(ctx);if(ai==s.active.end()||ai->second==0)throw std::runtime_error("dead decode lock");
        uint8_t c;
        if(ai->second==1){
            auto si=s.single.find(ctx);if(si==s.single.end())throw std::runtime_error("missing forced symbol");
            c=si->second;forced++;
        }else{
            int x=key.get();if(x<0)throw std::runtime_error("key exhausted");c=(uint8_t)x;ambig++;
        }
        consume(s,ctx,c);out.put(char(c));ctx=step_ctx(ctx,c,s.mask);
    }
    if(key.peek()!=EOF)throw std::runtime_error("unused key bytes");
    out.flush();std::cerr<<"LK_RECOVERED_BYTES="<<s.n<<"\nLK_FORCED="<<forced<<"\nLK_AMBIG="<<ambig<<"\n";
}
int main(int argc,char**argv){
    try{
        if(argc<2)throw std::runtime_error("usage");
        std::string op=argv[1];
        if(op=="build"){
            if(argc!=6)throw std::runtime_error("build source lock key order");
            build(argv[2],argv[3],argv[4],std::stoi(argv[5]));
        }else if(op=="decode"){
            if(argc!=5)throw std::runtime_error("decode lock key output");
            decode(argv[2],argv[3],argv[4]);
        }else throw std::runtime_error("bad op");
    }catch(const std::exception&e){std::cerr<<"LK_ERROR "<<e.what()<<"\n";return 1;}
    return 0;
}