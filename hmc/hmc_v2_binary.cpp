#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static void put_u16(std::ostream&o,uint16_t v){o.put(char(v&255));o.put(char(v>>8));}
static uint16_t get_u16(std::istream&i){int a=i.get(),b=i.get();if(a<0||b<0)throw std::runtime_error("short u16");return uint16_t(a|(b<<8));}
static void put_u64(std::ostream&o,uint64_t v){for(int k=0;k<8;k++)o.put(char((v>>(8*k))&255));}
static uint64_t get_u64(std::istream&i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}
static uint64_t fsize(const std::string&p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("size open");return uint64_t(f.tellg());}

struct BitWriter{
    std::ofstream out; uint8_t cur=0;int used=0;uint64_t bits=0;
    explicit BitWriter(const std::string&p):out(p,std::ios::binary|std::ios::trunc){if(!out)throw std::runtime_error("open bitwriter");}
    void bit(int b){cur|=uint8_t((b&1)<<(7-used));used++;bits++;if(used==8){out.put(char(cur));cur=0;used=0;}}
    void finish(){if(used)out.put(char(cur));out.flush();if(!out)throw std::runtime_error("bitwriter flush");}
};
struct BitReader{
    std::ifstream in;uint8_t cur=0;int used=8;uint64_t remaining;
    BitReader(const std::string&p,uint64_t off,uint64_t bytes):in(p,std::ios::binary),remaining(bytes){if(!in)throw std::runtime_error("open bitreader");in.seekg((std::streamoff)off);if(!in)throw std::runtime_error("seek");}
    int bit(){if(used==8){if(!remaining)throw std::runtime_error("bitreader exhausted");int c=in.get();if(c<0)throw std::runtime_error("short bitreader");cur=uint8_t(c);used=0;remaining--;}return (cur>>(7-used++))&1;}
};
struct Meta{uint8_t sym;uint64_t count,bits,bytes,off;};

static std::string tmp(const std::string&d,int i){return d+".p"+std::to_string(i)+".tmp";}

static std::vector<int> make_order(const std::array<uint64_t,256>&cnt,const std::string&mode){
    std::vector<int> s;for(int i=0;i<256;i++)if(cnt[i])s.push_back(i);
    if(mode=="freq-desc")std::sort(s.begin(),s.end(),[&](int a,int b){return cnt[a]!=cnt[b]?cnt[a]>cnt[b]:a<b;});
    else if(mode=="freq-asc")std::sort(s.begin(),s.end(),[&](int a,int b){return cnt[a]!=cnt[b]?cnt[a]<cnt[b]:a<b;});
    else if(mode=="byte")std::sort(s.begin(),s.end());
    else throw std::runtime_error("order must be freq-desc|freq-asc|byte");
    return s;
}

static void encode(const std::string&src,const std::string&dst,const std::string&order_mode){
    uint64_t total=fsize(src);
    std::array<uint64_t,256> cnt{};
    {std::ifstream in(src,std::ios::binary);std::vector<uint8_t>b(8<<20);while(in){in.read((char*)b.data(),b.size());size_t n=(size_t)in.gcount();for(size_t i=0;i<n;i++)cnt[b[i]]++;}}
    auto order=make_order(cnt,order_mode);if(order.size()<2)throw std::runtime_error("need >=2 symbols");
    std::array<int,256> rank{};rank.fill(-1);for(size_t i=0;i<order.size();i++)rank[order[i]]=(int)i;
    const size_t stored=order.size()-1; // last page is all remaining vacancies.
    std::vector<std::unique_ptr<BitWriter>> w(stored);
    for(size_t p=0;p<stored;p++)w[p]=std::make_unique<BitWriter>(tmp(dst,(int)p));

    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open source");
    std::vector<uint8_t>b(4<<20);uint64_t pos=0;
    while(in){
        in.read((char*)b.data(),b.size());size_t n=(size_t)in.gcount();
        for(size_t j=0;j<n;j++,pos++){
            int r=rank[b[j]];if(r<0)throw std::runtime_error("rank");
            size_t upto=std::min<size_t>((size_t)r,stored-1);
            for(size_t p=0;p<=upto;p++)w[p]->bit((int)p==r);
        }
    }
    if(pos!=total)throw std::runtime_error("length");
    std::vector<uint64_t> bits(stored),bytes(stored);
    for(size_t p=0;p<stored;p++){w[p]->finish();bits[p]=w[p]->bits;w[p].reset();bytes[p]=fsize(tmp(dst,(int)p));}

    std::ofstream out(dst,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("open carrier");
    out.write("HMC2",4);out.put(1);put_u64(out,total);put_u16(out,(uint16_t)order.size());
    uint8_t om=order_mode=="freq-desc"?1:order_mode=="freq-asc"?2:3;out.put(char(om));
    for(int s:order){out.put(char(s));put_u64(out,cnt[s]);}
    for(size_t p=0;p<stored;p++){put_u64(out,bits[p]);put_u64(out,bytes[p]);}
    std::vector<char>buf(4<<20);
    for(size_t p=0;p<stored;p++){std::ifstream q(tmp(dst,(int)p),std::ios::binary);while(q){q.read(buf.data(),buf.size());auto n=q.gcount();if(n>0)out.write(buf.data(),n);}q.close();std::remove(tmp(dst,(int)p).c_str());}
    out.flush();
    uint64_t total_bits=0;for(auto x:bits)total_bits+=x;
    std::cerr<<"HMC2_SOURCE_BYTES="<<total<<"\nHMC2_PAGES="<<order.size()<<"\nHMC2_STORED_BINARY_DECISIONS="<<total_bits<<"\nHMC2_CARRIER_BYTES="<<fsize(dst)<<"\n";
}

static void decode(const std::string&src,const std::string&dst){
    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open carrier");
    char m[4];in.read(m,4);if(in.gcount()!=4||std::string(m,4)!="HMC2")throw std::runtime_error("magic");if(in.get()!=1)throw std::runtime_error("version");
    uint64_t total=get_u64(in);uint16_t np=get_u16(in);int om=in.get();if(np<2||np>256||om<1||om>3)throw std::runtime_error("header");
    std::vector<int> order(np);std::vector<uint64_t> cnt(np);
    for(size_t i=0;i<np;i++){int s=in.get();if(s<0)throw std::runtime_error("sym");order[i]=s;cnt[i]=get_u64(in);}
    if([&](){uint64_t s=0;for(auto x:cnt)s+=x;return s;}()!=total)throw std::runtime_error("counts");
    size_t stored=np-1;std::vector<Meta> meta(stored);
    for(size_t p=0;p<stored;p++){meta[p].sym=(uint8_t)order[p];meta[p].count=cnt[p];meta[p].bits=get_u64(in);meta[p].bytes=get_u64(in);}
    uint64_t off=(uint64_t)in.tellg(),sum=off;for(auto &x:meta){x.off=sum;sum+=x.bytes;}if(sum!=fsize(src))throw std::runtime_error("size mismatch");in.close();

    if(total>0xffffffffull)throw std::runtime_error("v2 decoder supports <=2^32-1 slots");
    std::vector<uint32_t> vacancies((size_t)total);for(uint64_t i=0;i<total;i++)vacancies[(size_t)i]=(uint32_t)i;
    std::vector<uint8_t> out((size_t)total);
    for(size_t p=0;p<stored;p++){
        if(meta[p].bits!=vacancies.size())throw std::runtime_error("binary page length mismatch");
        BitReader br(src,meta[p].off,meta[p].bytes);
        std::vector<uint32_t> next;next.reserve(vacancies.size()-meta[p].count);uint64_t ones=0;
        for(uint32_t pos:vacancies){
            if(br.bit()){out[pos]=meta[p].sym;ones++;}
            else next.push_back(pos);
        }
        if(ones!=meta[p].count)throw std::runtime_error("page count mismatch");
        vacancies.swap(next);
    }
    if(vacancies.size()!=cnt.back())throw std::runtime_error("final page count");
    for(uint32_t pos:vacancies)out[pos]=(uint8_t)order.back();
    std::ofstream o(dst,std::ios::binary|std::ios::trunc);o.write((char*)out.data(),out.size());o.flush();
    std::cerr<<"HMC2_RECOVERED_BYTES="<<out.size()<<"\n";
}
int main(int argc,char**argv){
    try{
        if(argc<4){std::cerr<<"usage: hmc_v2 e input output [freq-desc|freq-asc|byte] | d input output\n";return 2;}
        std::string op=argv[1];if(op=="e"||op=="encode")encode(argv[2],argv[3],argc>4?argv[4]:"freq-desc");else if(op=="d"||op=="decode")decode(argv[2],argv[3]);else throw std::runtime_error("op");
    }catch(const std::exception&e){std::cerr<<"HMC2_ERROR "<<e.what()<<"\n";return 1;}return 0;
}
