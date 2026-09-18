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
#include <sys/stat.h>
#include <unistd.h>

static void put_u16(std::ostream&o,uint16_t v){o.put(char(v&255));o.put(char(v>>8));}
static uint16_t get_u16(std::istream&i){int a=i.get(),b=i.get();if(a<0||b<0)throw std::runtime_error("short u16");return uint16_t(a|(b<<8));}
static void put_u64(std::ostream&o,uint64_t v){for(int k=0;k<8;k++)o.put(char((v>>(8*k))&255));}
static uint64_t get_u64(std::istream&i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}
static uint64_t fsize(const std::string&p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("size open");return uint64_t(f.tellg());}

struct BitWriter{
    std::ofstream out; uint64_t buf=0; int used=0; uint64_t bits=0;
    explicit BitWriter(const std::string&p):out(p,std::ios::binary|std::ios::trunc){if(!out)throw std::runtime_error("open bitwriter");}
    void bit(unsigned b){buf=(buf<<1)|(b&1);used++;bits++;if(used==64)flush64();}
    void bits_msb(uint64_t v,int n){for(int i=n-1;i>=0;i--)bit((v>>i)&1);}
    void unary(uint64_t q){for(uint64_t i=0;i<q;i++)bit(0);bit(1);}
    void rice(uint64_t x,int k){uint64_t q=x>>k; unary(q); if(k)bits_msb(x&((uint64_t(1)<<k)-1),k);}
    void flush64(){for(int i=7;i>=0;i--)out.put(char((buf>>(8*i))&255));buf=0;used=0;}
    void finish(){
        if(used){buf <<= (64-used);int bytes=(used+7)/8;for(int i=7;i>=8-bytes;i--)out.put(char((buf>>(8*i))&255));}
        out.flush(); if(!out)throw std::runtime_error("bitwriter flush");
    }
};

struct BitReader{
    std::ifstream in; uint8_t cur=0; int used=8; uint64_t remaining;
    BitReader(const std::string&p,uint64_t offset,uint64_t bytes):in(p,std::ios::binary),remaining(bytes){
        if(!in)throw std::runtime_error("open bitreader");
        in.seekg((std::streamoff)offset);
        if(!in)throw std::runtime_error("bitreader seek");
    }
    int bit(){
        if(used==8){if(!remaining)throw std::runtime_error("bitreader exhausted");int c=in.get();if(c<0)throw std::runtime_error("short bitreader");cur=uint8_t(c);used=0;remaining--;}
        return (cur>>(7-used++))&1;
    }
    uint64_t bits_msb(int n){uint64_t v=0;for(int i=0;i<n;i++)v=(v<<1)|bit();return v;}
    uint64_t rice(int k){
        uint64_t q=0;while(bit()==0){q++;if(q>(1ull<<40))throw std::runtime_error("rice unary runaway");}
        uint64_t r=k?bits_msb(k):0; return (q<<k)|r;
    }
};
static int choose_k(uint64_t total,uint64_t count){
    if(!count || count>=total)return 0;
    long double mean=(long double)(total-count)/(long double)count;
    int k=0; long double p=1.0L;
    while(k<31 && p*2.0L<=mean+1.0L){p*=2.0L;k++;}
    return k;
}

static std::string tmpname(const std::string&dst,int sym){return dst+".p"+std::to_string(sym)+".tmp";}

static void encode(const std::string&src,const std::string&dst){
    uint64_t total=fsize(src);
    std::array<uint64_t,256> counts{};
    {
        std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open source");
        std::vector<unsigned char>b(8<<20);
        while(in){in.read((char*)b.data(),b.size());size_t n=(size_t)in.gcount();for(size_t i=0;i<n;i++)counts[b[i]]++;}
    }
    std::vector<int> syms;for(int s=0;s<256;s++)if(counts[s])syms.push_back(s);
    std::array<int,256> ks{};for(int s:syms)ks[s]=choose_k(total,counts[s]);

    std::array<uint64_t,256> last{}; last.fill(uint64_t(-1));
    std::array<std::unique_ptr<BitWriter>,256> bw;
    for(int s:syms)bw[s]=std::make_unique<BitWriter>(tmpname(dst,s));

    {
        std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open source pass2");
        std::vector<unsigned char>b(8<<20);uint64_t pos=0;
        while(in){
            in.read((char*)b.data(),b.size());size_t n=(size_t)in.gcount();
            for(size_t i=0;i<n;i++,pos++){
                int s=b[i];
                uint64_t gap=(last[s]==uint64_t(-1))?pos:(pos-last[s]-1);
                bw[s]->rice(gap,ks[s]); last[s]=pos;
            }
        }
        if(pos!=total)throw std::runtime_error("source length mismatch");
    }
    std::array<uint64_t,256> pbytes{},pbits{};
    for(int s:syms){bw[s]->finish();pbits[s]=bw[s]->bits;bw[s].reset();pbytes[s]=fsize(tmpname(dst,s));}

    std::ofstream out(dst,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("open carrier");
    out.write("HMC1",4);out.put(1);put_u64(out,total);put_u16(out,(uint16_t)syms.size());
    for(int s:syms){
        out.put(char(s));put_u64(out,counts[s]);out.put(char(ks[s]));put_u64(out,pbits[s]);put_u64(out,pbytes[s]);
    }
    std::vector<char>buf(8<<20);
    for(int s:syms){
        std::ifstream p(tmpname(dst,s),std::ios::binary);if(!p)throw std::runtime_error("open payload");
        while(p){p.read(buf.data(),buf.size());std::streamsize n=p.gcount();if(n>0)out.write(buf.data(),n);}
        p.close();std::remove(tmpname(dst,s).c_str());
    }
    out.flush();
    std::cerr<<"HMC_SOURCE_BYTES="<<total<<"\nHMC_SYMBOL_PAGES="<<syms.size()<<"\nHMC_CARRIER_BYTES="<<fsize(dst)<<"\n";
    for(int s:syms)std::cerr<<"HMC_PAGE sym="<<s<<" count="<<counts[s]<<" k="<<ks[s]<<" bits="<<pbits[s]<<" bytes="<<pbytes[s]<<"\n";
}

struct PageMeta{uint8_t sym;uint64_t count;uint8_t k;uint64_t bits,bytes,off;};

static void decode(const std::string&src,const std::string&dst){
    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open carrier");
    char m[4];in.read(m,4);if(in.gcount()!=4||std::string(m,4)!="HMC1")throw std::runtime_error("magic");
    if(in.get()!=1)throw std::runtime_error("version");
    uint64_t total=get_u64(in);uint16_t np=get_u16(in);
    if(!np||np>256)throw std::runtime_error("page count");
    std::vector<PageMeta> p(np);
    for(auto &x:p){
        int s=in.get(),k;if(s<0)throw std::runtime_error("short sym");x.sym=uint8_t(s);x.count=get_u64(in);
        k=in.get();if(k<0||k>31)throw std::runtime_error("bad k");x.k=uint8_t(k);x.bits=get_u64(in);x.bytes=get_u64(in);
    }
    uint64_t off=(uint64_t)in.tellg(),sum=off;
    for(auto &x:p){x.off=sum;sum+=x.bytes;}
    if(sum!=fsize(src))throw std::runtime_error("carrier size mismatch");
    in.close();

    int fd=::open(dst.c_str(),O_RDWR|O_CREAT|O_TRUNC,0600);
    if(fd<0)throw std::runtime_error("open mmap output");
    if(ftruncate(fd,(off_t)total)!=0){::close(fd);throw std::runtime_error("truncate output");}
    uint8_t* mem=nullptr;
    if(total){
        void* q=mmap(nullptr,(size_t)total,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
        if(q==MAP_FAILED){::close(fd);throw std::runtime_error("mmap output");}
        mem=(uint8_t*)q;
    }
    std::vector<uint8_t> seen((size_t)((total+7)/8),0);
    uint64_t assigned=0;
    try{
        for(const auto &x:p){
            BitReader br(src,x.off,x.bytes);uint64_t last=uint64_t(-1);
            for(uint64_t j=0;j<x.count;j++){
                uint64_t gap=br.rice(x.k);
                uint64_t pos=(last==uint64_t(-1))?gap:(last+1+gap);
                if(pos>=total)throw std::runtime_error("position range");
                size_t by=(size_t)(pos>>3);uint8_t mask=uint8_t(1u<<(pos&7));
                if(seen[by]&mask)throw std::runtime_error("page overlap");
                seen[by]|=mask;assigned++;
                mem[pos]=x.sym;last=pos;
            }
        }
        if(assigned!=total)throw std::runtime_error("unassigned slots");
        if(total && msync(mem,(size_t)total,MS_SYNC)!=0)throw std::runtime_error("msync");
    }catch(...){
        if(total)munmap(mem,(size_t)total);::close(fd);throw;
    }
    if(total)munmap(mem,(size_t)total);::close(fd);
    std::cerr<<"HMC_RECOVERED_BYTES="<<total<<"\nHMC_ASSIGNED_SLOTS="<<assigned<<"\n";
}

int main(int argc,char**argv){
    try{
        if(argc!=4){std::cerr<<"usage: hmc_v1 encode input output.hmc | decode input.hmc output\n";return 2;}
        std::string op=argv[1];
        if(op=="encode"||op=="e")encode(argv[2],argv[3]);
        else if(op=="decode"||op=="d")decode(argv[2],argv[3]);
        else throw std::runtime_error("unknown op");
    }catch(const std::exception&e){std::cerr<<"HMC_ERROR "<<e.what()<<"\n";return 1;}
    return 0;
}
