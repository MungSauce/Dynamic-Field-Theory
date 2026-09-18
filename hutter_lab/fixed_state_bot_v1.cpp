#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static void put_u64(std::ostream&o,uint64_t v){for(int i=0;i<8;i++)o.put(char((v>>(8*i))&255));}
static uint64_t get_u64(std::istream&i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}

struct BitWriter{
    std::ofstream &out; uint8_t cur=0; int used=0; uint64_t bits=0;
    explicit BitWriter(std::ofstream&o):out(o){}
    void bit(int b){cur|=uint8_t((b&1)<<(7-used));++used;++bits;if(used==8){out.put(char(cur));cur=0;used=0;}}
    void finish(){if(used){out.put(char(cur));cur=0;used=0;}}
};
struct BitReader{
    std::ifstream &in; uint8_t cur=0; int used=8; uint64_t bits=0;
    explicit BitReader(std::ifstream&i):in(i){}
    int bit(){if(used==8){int c=in.get();cur=c<0?0:uint8_t(c);used=0;}++bits;return (cur>>(7-used++))&1;}
};
struct ArithEnc{
    static constexpr uint64_t TOP=0xffffffffULL, HALF=0x80000000ULL, Q1=0x40000000ULL, Q3=0xc0000000ULL;
    uint64_t lo=0,hi=TOP,pending=0; BitWriter &w;
    explicit ArithEnc(BitWriter&x):w(x){}
    void outbit(int b){w.bit(b);while(pending){w.bit(!b);--pending;}}
    void bit(int b,uint32_t p1){
        p1=std::clamp<uint32_t>(p1,1,4095);uint32_t p0=4096-p1;
        uint32_t cum=b?p0:0, freq=b?p1:p0, total=4096;
        uint64_t range=hi-lo+1;
        hi=lo+(range*(cum+freq))/total-1; lo=lo+(range*cum)/total;
        for(;;){
            if(hi<HALF)outbit(0);
            else if(lo>=HALF){outbit(1);lo-=HALF;hi-=HALF;}
            else if(lo>=Q1&&hi<Q3){++pending;lo-=Q1;hi-=Q1;}
            else break;
            lo=(lo<<1)&TOP;hi=((hi<<1)&TOP)|1;
        }
    }
    void finish(){++pending;if(lo<Q1)outbit(0);else outbit(1);w.finish();}
};
struct ArithDec{
    static constexpr uint64_t TOP=0xffffffffULL, HALF=0x80000000ULL, Q1=0x40000000ULL, Q3=0xc0000000ULL;
    uint64_t lo=0,hi=TOP,code=0; BitReader&r;
    explicit ArithDec(BitReader&x):r(x){for(int i=0;i<32;i++)code=(code<<1)|r.bit();}
    int bit(uint32_t p1){
        p1=std::clamp<uint32_t>(p1,1,4095);uint32_t p0=4096-p1;
        uint64_t range=hi-lo+1;
        uint64_t split=lo+(range*p0)/4096;
        int b;
        if(code<split){b=0;hi=split-1;}
        else{b=1;lo=split;}
        for(;;){
            if(hi<HALF){}
            else if(lo>=HALF){lo-=HALF;hi-=HALF;code-=HALF;}
            else if(lo>=Q1&&hi<Q3){lo-=Q1;hi-=Q1;code-=Q1;}
            else break;
            lo=(lo<<1)&TOP;hi=((hi<<1)&TOP)|1;code=((code<<1)&TOP)|r.bit();
        }
        return b;
    }
};

static uint64_t mix64(uint64_t x){
    x+=0x9e3779b97f4a7c15ULL;
    x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
    x=(x^(x>>27))*0x94d049bb133111ebULL;
    return x^(x>>31);
}

struct CEntry{
    uint64_t key=0;
    uint16_t n0=1,n1=1;
    uint8_t used=0;
};
struct ContextBank{
    std::vector<CEntry> t; uint64_t mask;
    explicit ContextBank(unsigned bits):t(size_t(1)<<bits),mask((uint64_t(1)<<bits)-1){}
    CEntry& get(uint64_t key){
        CEntry &e=t[mix64(key)&mask];
        if(!e.used||e.key!=key){e.key=key;e.n0=1;e.n1=1;e.used=1;}
        return e;
    }
};
struct MEntry{
    uint64_t key=0,last=std::numeric_limits<uint64_t>::max(),prev=std::numeric_limits<uint64_t>::max();
    uint8_t used=0;
};

struct Bot{
    static constexpr int NM=5;
    std::array<int,NM> orders{{0,1,2,4,8}};
    std::vector<ContextBank> banks;
    std::vector<MEntry> mt;
    uint64_t mtmask;
    std::vector<uint8_t> ring;
    uint64_t ringmask;
    uint64_t hist=0,pos=0;
    uint8_t prefix=0; int bitpos=0;
    int match_weight;
    bool match_valid=false; uint8_t match_byte=0;

    Bot(unsigned tb,unsigned wb,int mw):mt(size_t(1)<<tb),mtmask((uint64_t(1)<<tb)-1),
      ring(size_t(1)<<wb),ringmask((uint64_t(1)<<wb)-1),match_weight(mw){
        for(int i=0;i<NM;i++)banks.emplace_back(tb);
    }
    uint64_t ctxkey(int ord,int model)const{
        uint64_t h=hist;
        if(ord<8){
            uint64_t mask=ord==0?0:((uint64_t(1)<<(ord*8))-1);
            h&=mask;
        }
        uint64_t x=h ^ (uint64_t(prefix)<<48) ^ (uint64_t(bitpos)<<56) ^ (uint64_t(model)<<60);
        return mix64(x);
    }
    void begin_byte(){
        prefix=0;bitpos=0;match_valid=false;
        uint64_t key=mix64(hist^0x6d617463685f7631ULL);
        MEntry &e=mt[key&mtmask];
        if(e.used&&e.key==key&&e.prev!=std::numeric_limits<uint64_t>::max()){
            uint64_t p=e.prev+1;
            if(p<pos && pos-p<=ring.size()){match_byte=ring[p&ringmask];match_valid=true;}
        }
    }
    uint32_t predict(){
        uint64_t ps=0,ws=0;
        for(int m=0;m<NM;m++){
            CEntry &e=banks[m].get(ctxkey(orders[m],m));
            uint32_t den=uint32_t(e.n0)+e.n1;
            uint32_t p=(uint32_t(e.n1)*4096u + den/2)/den;
            uint32_t w=4+std::min<uint32_t>(96,den/4);
            ps+=uint64_t(p)*w;ws+=w;
        }
        if(match_valid){
            int mb=(match_byte>>(7-bitpos))&1;
            uint32_t p=mb?4032:64;
            ps+=uint64_t(p)*uint32_t(match_weight);ws+=uint32_t(match_weight);
        }
        uint32_t out=uint32_t((ps+ws/2)/ws);
        return std::clamp<uint32_t>(out,1,4095);
    }
    void update(int b){
        for(int m=0;m<NM;m++){
            CEntry &e=banks[m].get(ctxkey(orders[m],m));
            uint32_t n=uint32_t(e.n0)+e.n1;
            if(n>=60000){e.n0=uint16_t((e.n0+1)/2);e.n1=uint16_t((e.n1+1)/2);}
            if(b){if(e.n1<65535)e.n1++;}else{if(e.n0<65535)e.n0++;}
        }
        prefix=uint8_t((prefix<<1)|b);bitpos++;
    }
    void end_byte(uint8_t byte){
        ring[pos&ringmask]=byte;
        hist=(hist<<8)|byte;
        uint64_t key=mix64(hist^0x6d617463685f7631ULL);
        MEntry &e=mt[key&mtmask];
        if(e.used&&e.key==key){e.prev=e.last;e.last=pos;}
        else{e.key=key;e.prev=std::numeric_limits<uint64_t>::max();e.last=pos;e.used=1;}
        pos++;
    }
    uint64_t approx_state_bytes()const{
        uint64_t s=ring.size()+mt.size()*sizeof(MEntry);
        for(auto &b:banks)s+=b.t.size()*sizeof(CEntry);
        return s;
    }
};

static void encode(const std::string&src,const std::string&dst,unsigned tb,unsigned wb,int mw){
    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open source");
    in.seekg(0,std::ios::end);uint64_t n=uint64_t(in.tellg());in.seekg(0);
    std::ofstream out(dst,std::ios::binary);if(!out)throw std::runtime_error("open carrier");
    out.write("FSB1",4);out.put(1);out.put(char(tb));out.put(char(wb));put_u64(out,uint64_t(mw));put_u64(out,n);
    BitWriter bw(out);ArithEnc ae(bw);Bot bot(tb,wb,mw);
    for(uint64_t i=0;i<n;i++){
        int ch=in.get();if(ch<0)throw std::runtime_error("short source");
        uint8_t x=uint8_t(ch);bot.begin_byte();
        for(int bp=0;bp<8;bp++){int b=(x>>(7-bp))&1;uint32_t p=bot.predict();ae.bit(b,p);bot.update(b);}
        bot.end_byte(x);
        if((i+1)%1000000==0)std::cerr<<"FSB_ENCODE_BYTES="<<(i+1)<<"\n";
    }
    ae.finish();out.flush();
    std::cerr<<"FSB_SOURCE_BYTES="<<n<<"\n";
    std::cerr<<"FSB_STATE_BYTES="<<bot.approx_state_bytes()<<"\n";
}
static void decode(const std::string&src,const std::string&dst){
    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open carrier");
    char m[4];in.read(m,4);if(in.gcount()!=4||std::string(m,4)!="FSB1")throw std::runtime_error("magic");
    if(in.get()!=1)throw std::runtime_error("version");int tbc=in.get(),wbc=in.get();if(tbc<0||wbc<0)throw std::runtime_error("params");
    unsigned tb=unsigned(tbc),wb=unsigned(wbc);uint64_t mw64=get_u64(in),n=get_u64(in);if(mw64>1000000)throw std::runtime_error("weight");
    BitReader br(in);ArithDec ad(br);Bot bot(tb,wb,int(mw64));
    std::ofstream out(dst,std::ios::binary);if(!out)throw std::runtime_error("open output");
    for(uint64_t i=0;i<n;i++){
        bot.begin_byte();uint8_t x=0;
        for(int bp=0;bp<8;bp++){uint32_t p=bot.predict();int b=ad.bit(p);x=uint8_t((x<<1)|b);bot.update(b);}
        out.put(char(x));bot.end_byte(x);
        if((i+1)%1000000==0)std::cerr<<"FSB_DECODE_BYTES="<<(i+1)<<"\n";
    }
    std::cerr<<"FSB_RECOVERED_BYTES="<<n<<"\n";
    std::cerr<<"FSB_STATE_BYTES="<<bot.approx_state_bytes()<<"\n";
}
int main(int argc,char**argv){
    try{
        if(argc<4){std::cerr<<"usage: fixed_state_bot e input output [table_bits=20] [window_bits=24] [match_weight=96]\n       fixed_state_bot d input output\n";return 2;}
        std::string a=argv[1];
        if(a=="e"||a=="encode"){
            unsigned tb=argc>4?unsigned(std::stoul(argv[4])):20;
            unsigned wb=argc>5?unsigned(std::stoul(argv[5])):24;
            int mw=argc>6?std::stoi(argv[6]):96;
            if(tb<16||tb>23||wb<20||wb>26||mw<0||mw>4096)throw std::runtime_error("parameter range");
            encode(argv[2],argv[3],tb,wb,mw);
        }else if(a=="d"||a=="decode")decode(argv[2],argv[3]);
        else throw std::runtime_error("action");
    }catch(const std::exception&e){std::cerr<<"FSB_ERROR "<<e.what()<<"\n";return 1;}
    return 0;
}
