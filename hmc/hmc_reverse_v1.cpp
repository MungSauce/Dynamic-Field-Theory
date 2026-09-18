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
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static void put_u16(std::ostream&o,uint16_t v){o.put(char(v&255));o.put(char(v>>8));}
static uint16_t get_u16(std::istream&i){int a=i.get(),b=i.get();if(a<0||b<0)throw std::runtime_error("short u16");return uint16_t(a|(b<<8));}
static void put_u64(std::ostream&o,uint64_t v){for(int k=0;k<8;k++)o.put(char((v>>(8*k))&255));}
static uint64_t get_u64(std::istream&i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}
static uint64_t fsize(const std::string&p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("size open");return uint64_t(f.tellg());}

struct BW{
    std::ofstream o; uint8_t cur=0; int used=0; uint64_t bits=0;
    BW(const std::string&p):o(p,std::ios::binary|std::ios::trunc){if(!o)throw std::runtime_error("bw open");}
    void bit(int b){cur|=uint8_t((b&1)<<(7-used));used++;bits++;if(used==8){o.put(char(cur));cur=0;used=0;}}
    void bits_msb(uint64_t v,int n){for(int i=n-1;i>=0;i--)bit((v>>i)&1);}
    void rice(uint64_t x,int k){uint64_t q=x>>k;for(uint64_t i=0;i<q;i++)bit(0);bit(1);if(k)bits_msb(x&((1ull<<k)-1),k);}
    void finish(){if(used)o.put(char(cur));o.flush();if(!o)throw std::runtime_error("bw flush");}
};
struct BR{
    std::ifstream i; uint8_t cur=0; int used=8; uint64_t rem;
    BR(const std::string&p,uint64_t off,uint64_t bytes):i(p,std::ios::binary),rem(bytes){if(!i)throw std::runtime_error("br open");i.seekg((std::streamoff)off);}
    int bit(){if(used==8){if(!rem)throw std::runtime_error("br exhausted");int c=i.get();if(c<0)throw std::runtime_error("br short");cur=uint8_t(c);used=0;rem--;}return (cur>>(7-used++))&1;}
    uint64_t bits_msb(int n){uint64_t v=0;for(int j=0;j<n;j++)v=(v<<1)|bit();return v;}
    uint64_t rice(int k){uint64_t q=0;while(bit()==0){if(++q>(1ull<<40))throw std::runtime_error("unary runaway");}uint64_t r=k?bits_msb(k):0;return (q<<k)|r;}
};
static int choose_k(long double mean){
    int k=0; long double p=1;
    while(k<31 && p*2<=mean+1){p*=2;k++;}
    return k;
}
static std::string tp(const std::string&d,int s){return d+".r"+std::to_string(s)+".tmp";}

struct Meta{uint8_t sym;uint64_t count,runs;uint8_t kg,kr;uint64_t bits,bytes,off;};

static void encode(const std::string&src,const std::string&dst){
    uint64_t total=fsize(src);
    std::array<uint64_t,256> count{},runs{};
    {
        std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open");
        std::vector<uint8_t>b(8<<20);int prev=-1;uint64_t pos=0;
        while(in){in.read((char*)b.data(),b.size());size_t n=(size_t)in.gcount();for(size_t j=0;j<n;j++,pos++){int s=b[j];count[s]++;if(s!=prev)runs[s]++;prev=s;}}
        if(pos!=total)throw std::runtime_error("length");
    }
    std::vector<int> syms;for(int s=0;s<256;s++)if(count[s])syms.push_back(s);
    std::array<int,256>kg{},kr{};
    for(int s:syms){
        long double mg=(long double)(total-count[s])/(long double)runs[s];
        long double mr=(long double)(count[s]-runs[s])/(long double)runs[s]; // encode run_len-1
        kg[s]=choose_k(mg);kr[s]=choose_k(mr);
    }
    std::array<std::unique_ptr<BW>,256>w;
    for(int s:syms)w[s]=std::make_unique<BW>(tp(dst,s));
    std::array<uint64_t,256> cursor{}; // first not-yet-accounted slot for each symbol relation
    {
        std::ifstream in(src,std::ios::binary);std::vector<uint8_t>b(8<<20);
        uint64_t pos=0,run_start=0,run_len=0;int cur=-1;
        auto flush=[&](){
            if(cur<0)return;
            uint64_t gap=run_start-cursor[cur];
            w[cur]->rice(gap,kg[cur]);
            w[cur]->rice(run_len-1,kr[cur]);
            cursor[cur]=run_start+run_len;
        };
        while(in){
            in.read((char*)b.data(),b.size());size_t n=(size_t)in.gcount();
            for(size_t j=0;j<n;j++,pos++){
                int s=b[j];
                if(s==cur)run_len++;
                else{flush();cur=s;run_start=pos;run_len=1;}
            }
        }
        flush();
    }
    std::array<uint64_t,256>bits{},bytes{};
    for(int s:syms){w[s]->finish();bits[s]=w[s]->bits;w[s].reset();bytes[s]=fsize(tp(dst,s));}

    std::ofstream out(dst,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("out");
    out.write("HMR1",4);out.put(1);put_u64(out,total);put_u16(out,(uint16_t)syms.size());
    for(int s:syms){out.put(char(s));put_u64(out,count[s]);put_u64(out,runs[s]);out.put(char(kg[s]));out.put(char(kr[s]));put_u64(out,bits[s]);put_u64(out,bytes[s]);}
    std::vector<char>buf(8<<20);
    for(int s:syms){
        std::ifstream p(tp(dst,s),std::ios::binary);while(p){p.read(buf.data(),buf.size());auto n=p.gcount();if(n>0)out.write(buf.data(),n);}
        p.close();std::remove(tp(dst,s).c_str());
    }
    out.flush();
    std::cerr<<"HMR_SOURCE_BYTES="<<total<<"\nHMR_SYMBOL_PAGES="<<syms.size()<<"\nHMR_CARRIER_BYTES="<<fsize(dst)<<"\n";
}

static void decode(const std::string&src,const std::string&dst){
    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("carrier");
    char m[4];in.read(m,4);if(in.gcount()!=4||std::string(m,4)!="HMR1")throw std::runtime_error("magic");
    if(in.get()!=1)throw std::runtime_error("version");
    uint64_t total=get_u64(in);uint16_t np=get_u16(in);if(!np||np>256)throw std::runtime_error("pages");
    std::vector<Meta>v(np);
    for(auto&x:v){
        int s=in.get(),a,b;if(s<0)throw std::runtime_error("sym");x.sym=uint8_t(s);x.count=get_u64(in);x.runs=get_u64(in);
        a=in.get();b=in.get();if(a<0||a>31||b<0||b>31)throw std::runtime_error("k");x.kg=a;x.kr=b;x.bits=get_u64(in);x.bytes=get_u64(in);
    }
    uint64_t off=(uint64_t)in.tellg(),sum=off;for(auto&x:v){x.off=sum;sum+=x.bytes;}if(sum!=fsize(src))throw std::runtime_error("size");
    in.close();

    int fd=::open(dst.c_str(),O_RDWR|O_CREAT|O_TRUNC,0600);if(fd<0)throw std::runtime_error("dst");
    if(ftruncate(fd,(off_t)total)!=0){::close(fd);throw std::runtime_error("truncate");}
    uint8_t*mem=nullptr;if(total){void*q=mmap(nullptr,(size_t)total,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);if(q==MAP_FAILED){::close(fd);throw std::runtime_error("mmap");}mem=(uint8_t*)q;}
    std::vector<uint8_t>seen((size_t)((total+7)/8),0);uint64_t assigned=0;
    try{
        for(const auto&x:v){
            BR br(src,x.off,x.bytes);uint64_t cursor=0,placed=0;
            for(uint64_t r=0;r<x.runs;r++){
                uint64_t gap=br.rice(x.kg),len=br.rice(x.kr)+1,start=cursor+gap;
                if(start+len>total)throw std::runtime_error("range");
                for(uint64_t p=start;p<start+len;p++){
                    size_t by=(size_t)(p>>3);uint8_t mask=uint8_t(1u<<(p&7));
                    if(seen[by]&mask)throw std::runtime_error("overlap");
                    seen[by]|=mask;mem[p]=x.sym;assigned++;placed++;
                }
                cursor=start+len;
            }
            if(placed!=x.count)throw std::runtime_error("count mismatch");
        }
        if(assigned!=total)throw std::runtime_error("unassigned");
        if(total)msync(mem,(size_t)total,MS_SYNC);
    }catch(...){if(total)munmap(mem,(size_t)total);::close(fd);throw;}
    if(total)munmap(mem,(size_t)total);::close(fd);
    std::cerr<<"HMR_RECOVERED_BYTES="<<total<<"\n";
}
int main(int argc,char**argv){
    try{
        if(argc!=4){std::cerr<<"usage: hmc_reverse_v1 e in out | d in out\n";return 2;}
        std::string op=argv[1];
        if(op=="e"||op=="encode")encode(argv[2],argv[3]);
        else if(op=="d"||op=="decode")decode(argv[2],argv[3]);
        else throw std::runtime_error("op");
    }catch(const std::exception&e){std::cerr<<"HMR_ERROR "<<e.what()<<"\n";return 1;}
    return 0;
}
// CANONICAL REPLICATION NOTICE (2026-09-18): predecessor/lineage artifact. Current protocol: compression/EIS_K32_FORMAL_REPLICATION_V1.md
