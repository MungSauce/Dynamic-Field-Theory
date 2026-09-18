#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

static void put_u64(std::ostream&o,uint64_t v){for(int i=0;i<8;i++)o.put(char((v>>(8*i))&255));}
static uint64_t get_u64(std::istream&i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}

struct BitOut{
    std::ostream& o; uint8_t cur=0; int used=0; uint64_t bits=0;
    explicit BitOut(std::ostream&x):o(x){}
    void bit(int b){cur|=uint8_t((b&1)<<(7-used));used++;bits++;if(used==8){o.put(char(cur));cur=0;used=0;}}
    void finish(){if(used)o.put(char(cur));o.flush();}
};
struct BitIn{
    std::istream& i; uint8_t cur=0; int used=8;
    explicit BitIn(std::istream&x):i(x){}
    int bit(){if(used==8){int c=i.get();cur=(c<0)?0:uint8_t(c);used=0;}return (cur>>(7-used++))&1;}
};

struct ArithEnc{
    static constexpr uint64_t TOP=0xffffffffULL, HALF=0x80000000ULL, Q1=0x40000000ULL, Q3=0xc0000000ULL;
    uint64_t lo=0,hi=TOP,pending=0; BitOut out;
    explicit ArithEnc(std::ostream&o):out(o){}
    void outbit(int b){out.bit(b);while(pending){out.bit(!b);--pending;}}
    void bit(int b,uint32_t f0,uint32_t f1){
        uint32_t total=f0+f1, cum=b?f0:0, freq=b?f1:f0;
        uint64_t range=hi-lo+1;
        hi=lo+(range*(cum+freq))/total-1;
        lo=lo+(range*cum)/total;
        for(;;){
            if(hi<HALF) outbit(0);
            else if(lo>=HALF){outbit(1);lo-=HALF;hi-=HALF;}
            else if(lo>=Q1&&hi<Q3){++pending;lo-=Q1;hi-=Q1;}
            else break;
            lo=(lo<<1)&TOP;hi=((hi<<1)&TOP)|1;
        }
    }
    void finish(){++pending;if(lo<Q1)outbit(0);else outbit(1);out.finish();}
};
struct ArithDec{
    static constexpr uint64_t TOP=0xffffffffULL, HALF=0x80000000ULL, Q1=0x40000000ULL, Q3=0xc0000000ULL;
    uint64_t lo=0,hi=TOP,code=0; BitIn in;
    explicit ArithDec(std::istream&i):in(i){for(int k=0;k<32;k++)code=(code<<1)|in.bit();}
    int bit(uint32_t f0,uint32_t f1){
        uint32_t total=f0+f1;
        uint64_t range=hi-lo+1;
        uint32_t target=uint32_t((((code-lo+1)*total)-1)/range);
        int b=target>=f0;
        uint32_t cum=b?f0:0,freq=b?f1:f0;
        hi=lo+(range*(cum+freq))/total-1;
        lo=lo+(range*cum)/total;
        for(;;){
            if(hi<HALF){}
            else if(lo>=HALF){lo-=HALF;hi-=HALF;code-=HALF;}
            else if(lo>=Q1&&hi<Q3){lo-=Q1;hi-=Q1;code-=Q1;}
            else break;
            lo=(lo<<1)&TOP;hi=((hi<<1)&TOP)|1;code=((code<<1)&TOP)|in.bit();
        }
        return b;
    }
};

struct BinCtx{
    uint16_t n0=1,n1=1;
    void update(int b){
        if(b)n1++;else n0++;
        if(uint32_t(n0)+n1>=8192){n0=uint16_t((n0+1)>>1);n1=uint16_t((n1+1)>>1);if(!n0)n0=1;if(!n1)n1=1;}
    }
};
struct HEntry{uint32_t tag=0;BinCtx c;};
struct MEntry{uint64_t key=0;uint32_t pos=0;};

static inline uint64_t mix64(uint64_t x){
    x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;x^=x>>31;return x;
}
static inline uint32_t h32(uint64_t x){return uint32_t(mix64(x));}

struct Due{uint64_t pos;uint16_t sym;uint32_t ver;bool operator<(const Due&o)const{return pos>o.pos;}};

class Model{
    static constexpr uint32_t HSZ=1u<<20, HMASK=HSZ-1, MAXG=4095;
    std::array<BinCtx,256> global_{};
    std::vector<BinCtx> o1_;
    std::vector<HEntry> o2_,o3_;
    std::vector<MEntry> match_;
    std::vector<uint8_t> hist_;
    std::array<int64_t,256> last_;
    std::array<uint16_t,256> best_gap_{};
    std::array<uint16_t,256> best_count_{};
    std::vector<uint16_t> gapfreq_;
    std::array<uint32_t,256> duever_{};
    std::priority_queue<Due> due_;
    uint64_t pos_=0;
    uint8_t prev1_=0,prev2_=0,prev3_=0;
    bool have1_=false,have2_=false,have3_=false;
    int match_pred_=-1;
    int64_t match_source_=-1;
    std::vector<uint8_t> due_syms_;

    HEntry& get(std::vector<HEntry>&tab,uint64_t key){
        uint32_t tag=h32(key)|1u, idx=tag&HMASK;
        HEntry &e=tab[idx];
        if(e.tag!=tag){e.tag=tag;e.c=BinCtx();}
        return e;
    }
    uint64_t context8_key() const{
        if(hist_.size()<8)return 0;
        uint64_t x=0;std::memcpy(&x,hist_.data()+hist_.size()-8,8);
        return mix64(x);
    }
    void prepare_special(){
        due_syms_.clear();
        while(!due_.empty()){
            Due d=due_.top();
            if(d.pos<pos_ || d.ver!=duever_[d.sym]){due_.pop();continue;}
            if(d.pos>pos_)break;
            due_.pop();due_syms_.push_back(uint8_t(d.sym));
        }
        match_pred_=-1;match_source_=-1;
        if(hist_.size()>=8){
            uint64_t key=context8_key();uint32_t idx=uint32_t(key)&HMASK;
            const MEntry&e=match_[idx];
            if(e.key==key && e.pos<hist_.size()){match_source_=e.pos;match_pred_=hist_[e.pos];}
        }
    }
public:
    Model():o1_(256*256),o2_(HSZ),o3_(HSZ),match_(HSZ),gapfreq_(256*(MAXG+1),0){
        last_.fill(-1);
        hist_.reserve(1<<20);
    }
    void reserve(size_t n){hist_.reserve(n);}
    void start_byte(){prepare_special();}
    int64_t match_source() const{return match_source_;}
    size_t history_size() const{return hist_.size();}
    uint8_t history_at(size_t i) const{if(i>=hist_.size())throw std::runtime_error("history range");return hist_[i];}
    void probs(uint16_t node,uint32_t &f0,uint32_t &f1){
        auto add=[&](const BinCtx&c,uint32_t w){f0+=w*c.n0;f1+=w*c.n1;};
        f0=f1=1;
        add(global_[node],1);
        if(have1_)add(o1_[uint32_t(prev1_)*256+node],4);
        if(have2_){
            uint64_t k=(uint64_t(prev2_)<<24)|(uint64_t(prev1_)<<16)|node;
            add(get(o2_,k).c,7);
        }
        if(have3_){
            uint64_t k=(uint64_t(prev3_)<<32)|(uint64_t(prev2_)<<24)|(uint64_t(prev1_)<<16)|node;
            add(get(o3_,k).c,7);
        }
        int bit_index=0;for(uint16_t n=node;n>1;n>>=1)bit_index++;
        int shift=7-bit_index;
        if(match_pred_>=0){
            int b=(match_pred_>>shift)&1;
            if(b)f1+=192;else f0+=192;
        }
        for(uint8_t s:due_syms_){
            int b=(s>>shift)&1;
            if(b)f1+=18;else f0+=18;
        }
        uint32_t total=f0+f1;
        if(total>60000){
            f0=std::max<uint32_t>(1,(f0*60000ull)/total);
            f1=std::max<uint32_t>(1,(f1*60000ull)/total);
        }
    }
    void update_bit(uint16_t node,int b){
        global_[node].update(b);
        if(have1_)o1_[uint32_t(prev1_)*256+node].update(b);
        if(have2_){
            uint64_t k=(uint64_t(prev2_)<<24)|(uint64_t(prev1_)<<16)|node;
            get(o2_,k).c.update(b);
        }
        if(have3_){
            uint64_t k=(uint64_t(prev3_)<<32)|(uint64_t(prev2_)<<24)|(uint64_t(prev1_)<<16)|node;
            get(o3_,k).c.update(b);
        }
    }
    void observe_byte(uint8_t c){
        uint16_t node=1;
        for(int sh=7;sh>=0;--sh){int b=(c>>sh)&1;update_bit(node,b);node=uint16_t((node<<1)|b);}
        finish_byte(c);
    }
    void finish_byte(uint8_t c){
        if(hist_.size()>=8){
            uint64_t key=context8_key();uint32_t idx=uint32_t(key)&HMASK;
            match_[idx].key=key;match_[idx].pos=uint32_t(hist_.size());
        }
        int64_t lp=last_[c];
        if(lp>=0){
            uint64_t gap=pos_-uint64_t(lp);
            if(gap<=MAXG){
                uint16_t &gf=gapfreq_[uint32_t(c)*(MAXG+1)+uint32_t(gap)];
                if(gf<65535)gf++;
                if(gf>best_count_[c]){best_count_[c]=gf;best_gap_[c]=uint16_t(gap);}
            }
        }
        last_[c]=int64_t(pos_);
        duever_[c]++;
        if(best_gap_[c])due_.push(Due{pos_+best_gap_[c],c,duever_[c]});
        hist_.push_back(c);
        prev3_=prev2_;prev2_=prev1_;prev1_=c;
        if(have2_)have3_=true;
        if(have1_)have2_=true;
        have1_=true;pos_++;
    }
};

static uint64_t fsize(const std::string&p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("size");return uint64_t(f.tellg());}

static void gamma_enc(ArithEnc&ac,uint64_t x){
    if(!x)throw std::runtime_error("gamma zero");
    int k=63-__builtin_clzll(x);
    for(int i=0;i<k;i++)ac.bit(0,1,1);
    ac.bit(1,1,1);
    for(int i=k-1;i>=0;i--)ac.bit((x>>i)&1,1,1);
}
static uint64_t gamma_dec(ArithDec&ac){
    int k=0;while(ac.bit(1,1)==0){if(++k>30)throw std::runtime_error("gamma runaway");}
    uint64_t x=1;
    for(int i=0;i<k;i++)x=(x<<1)|ac.bit(1,1);
    return x;
}
static void rescale_flag(BinCtx&c,int b){c.update(b);}

static std::vector<uint8_t> read_all(const std::string&p){
    uint64_t n=fsize(p);std::vector<uint8_t>v((size_t)n);
    std::ifstream in(p,std::ios::binary);if(!in)throw std::runtime_error("open source");
    if(n){in.read((char*)v.data(),(std::streamsize)n);if((uint64_t)in.gcount()!=n)throw std::runtime_error("short source");}
    return v;
}

static void encode(const std::string&src,const std::string&dst,uint8_t minmatch){
    auto data=read_all(src);uint64_t n=data.size();
    std::ofstream out(dst,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("open output");
    out.write("HCD1",4);out.put(1);out.put(char(minmatch));put_u64(out,n);
    ArithEnc ac(out);Model m;m.reserve((size_t)std::min<uint64_t>(n,1000000000ull));BinCtx flag;
    uint64_t pos=0,matches=0,match_bytes=0,literals=0;
    constexpr uint64_t MAX_MATCH=1u<<20;
    while(pos<n){
        m.start_byte();int64_t ms=m.match_source();bool has=ms>=0;uint64_t L=0;
        if(has){
            uint64_t s=(uint64_t)ms;
            uint64_t lim=std::min<uint64_t>({MAX_MATCH,n-pos,n-s});
            // Once source catches current position, overlap is allowed and source bytes are the already-known periodic continuation.
            lim=std::min<uint64_t>(MAX_MATCH,n-pos);
            while(L<lim && data[s+L]==data[pos+L]){
                L++;
                if(s+L>=n)break;
            }
        }
        bool use=has && L>=minmatch;
        if(has){
            uint32_t f0=flag.n0,f1=flag.n1;ac.bit(use?1:0,f0,f1);rescale_flag(flag,use?1:0);
        }
        if(use){
            gamma_enc(ac,L-minmatch+1);matches++;match_bytes+=L;
            for(uint64_t i=0;i<L;i++)m.observe_byte(data[pos+i]);
            pos+=L;
        }else{
            uint8_t ch=data[pos];uint16_t node=1;
            for(int sh=7;sh>=0;--sh){
                uint32_t f0,f1;m.probs(node,f0,f1);int b=(ch>>sh)&1;ac.bit(b,f0,f1);m.update_bit(node,b);node=uint16_t((node<<1)|b);
            }
            m.finish_byte(ch);pos++;literals++;
        }
    }
    ac.finish();
    std::cerr<<"HCD_SOURCE_BYTES="<<n<<"\nHCD_CARRIER_BYTES="<<fsize(dst)<<"\nHCD_MATCHES="<<matches<<"\nHCD_MATCH_BYTES="<<match_bytes<<"\nHCD_LITERALS="<<literals<<"\n";
}
static void decode(const std::string&src,const std::string&dst){
    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open carrier");
    char magic[4];in.read(magic,4);if(in.gcount()!=4||std::string(magic,4)!="HCD1")throw std::runtime_error("magic");
    if(in.get()!=1)throw std::runtime_error("version");int mm=in.get();if(mm<2||mm>64)throw std::runtime_error("minmatch");uint8_t minmatch=uint8_t(mm);uint64_t n=get_u64(in);
    std::ofstream out(dst,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("open recovered");
    ArithDec ac(in);Model m;m.reserve((size_t)std::min<uint64_t>(n,1000000000ull));BinCtx flag;
    uint64_t pos=0,matches=0,match_bytes=0,literals=0;
    while(pos<n){
        m.start_byte();int64_t ms=m.match_source();bool has=ms>=0,use=false;
        if(has){uint32_t f0=flag.n0,f1=flag.n1;use=ac.bit(f0,f1);rescale_flag(flag,use?1:0);}
        if(use){
            uint64_t L=gamma_dec(ac)+minmatch-1;if(!L||pos+L>n)throw std::runtime_error("match length");
            uint64_t s=(uint64_t)ms;matches++;match_bytes+=L;
            for(uint64_t i=0;i<L;i++){
                if(s+i>=m.history_size())throw std::runtime_error("match source unavailable");
                uint8_t ch=m.history_at((size_t)(s+i));out.put(char(ch));m.observe_byte(ch);
            }
            pos+=L;
        }else{
            uint16_t node=1;uint8_t ch=0;
            for(int sh=7;sh>=0;--sh){
                uint32_t f0,f1;m.probs(node,f0,f1);int b=ac.bit(f0,f1);m.update_bit(node,b);node=uint16_t((node<<1)|b);ch|=uint8_t(b<<sh);
            }
            out.put(char(ch));m.finish_byte(ch);pos++;literals++;
        }
    }
    out.flush();std::cerr<<"HCD_RECOVERED_BYTES="<<n<<"\nHCD_MATCHES="<<matches<<"\nHCD_MATCH_BYTES="<<match_bytes<<"\nHCD_LITERALS="<<literals<<"\n";
}
int main(int argc,char**argv){
    try{
        if(argc<4||argc>5){std::cerr<<"usage: hmc_domino_v2 e input output.hcd [minmatch=8] | d input.hcd output\n";return 2;}
        std::string op=argv[1];
        if(op=="e"||op=="encode"){uint8_t mm=argc==5?(uint8_t)std::stoul(argv[4]):8;if(mm<2||mm>64)throw std::runtime_error("minmatch");encode(argv[2],argv[3],mm);}
        else if(op=="d"||op=="decode")decode(argv[2],argv[3]);
        else throw std::runtime_error("op");
    }catch(const std::exception&e){std::cerr<<"HCD_ERROR "<<e.what()<<"\n";return 1;}
    return 0;
}
