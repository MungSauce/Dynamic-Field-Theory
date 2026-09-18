#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

static uint64_t fsize(const std::string&p){
    std::ifstream i(p,std::ios::binary|std::ios::ate);
    if(!i) throw std::runtime_error("size open");
    return (uint64_t)i.tellg();
}
static void put_u64(std::ostream&o,uint64_t v){ for(int i=0;i<8;i++) o.put(char((v>>(8*i))&255)); }
static uint64_t get_u64(std::istream&i){
    uint64_t v=0; for(int k=0;k<8;k++){ int c=i.get(); if(c<0) throw std::runtime_error("short u64"); v|=uint64_t(c)<<(8*k); } return v;
}
static void put_u32(std::ostream&o,uint32_t v){ for(int i=0;i<4;i++) o.put(char((v>>(8*i))&255)); }
static uint32_t get_u32(std::istream&i){
    uint32_t v=0; for(int k=0;k<4;k++){ int c=i.get(); if(c<0) throw std::runtime_error("short u32"); v|=uint32_t(c)<<(8*k); } return v;
}

enum Mode : uint8_t { TRANS_DELTA=1, PERIOD_DELTA=2, PERIOD_XOR=3 };

static void encode(const std::string&src,const std::string&dst,Mode mode,uint32_t lag){
    if(mode==TRANS_DELTA) lag=1;
    if(!lag) throw std::runtime_error("lag zero");
    const uint64_t total=fsize(src);
    std::ifstream in(src,std::ios::binary); if(!in) throw std::runtime_error("open input");
    std::ofstream out(dst,std::ios::binary|std::ios::trunc); if(!out) throw std::runtime_error("open output");
    out.write("ERP1",4); out.put(1); out.put(char(mode)); put_u32(out,lag); put_u64(out,total);

    const size_t CHUNK=1<<20;
    std::vector<uint8_t> buf(CHUNK), hist(std::min<uint64_t>(lag,total));
    uint64_t pos=0;
    while(pos<total){
        size_t n=(size_t)std::min<uint64_t>(CHUNK,total-pos);
        in.read((char*)buf.data(),n); if((size_t)in.gcount()!=n) throw std::runtime_error("short input");
        for(size_t j=0;j<n;j++,pos++){
            uint8_t x=buf[j], y;
            if(pos < lag){
                y=x;
            }else{
                uint8_t prev=hist[pos % lag];
                if(mode==PERIOD_XOR) y=uint8_t(x ^ prev);
                else y=uint8_t(x - prev);
            }
            out.put(char(y));
            if(pos < hist.size()) hist[pos]=x;
            else hist[pos % lag]=x;
        }
    }
    if(in.peek()!=EOF) throw std::runtime_error("trailing source");
}
static void decode(const std::string&src,const std::string&dst){
    std::ifstream in(src,std::ios::binary); if(!in) throw std::runtime_error("open carrier");
    char m[4]; in.read(m,4); if(in.gcount()!=4 || std::string(m,4)!="ERP1") throw std::runtime_error("magic");
    if(in.get()!=1) throw std::runtime_error("version");
    int mm=in.get(); if(mm<1||mm>3) throw std::runtime_error("mode");
    Mode mode=(Mode)mm; uint32_t lag=get_u32(in); uint64_t total=get_u64(in);
    if(mode==TRANS_DELTA && lag!=1) throw std::runtime_error("transition lag");
    if(!lag) throw std::runtime_error("lag zero");
    std::ofstream out(dst,std::ios::binary|std::ios::trunc); if(!out) throw std::runtime_error("open output");

    std::vector<uint8_t> hist(std::min<uint64_t>(lag,total));
    for(uint64_t pos=0;pos<total;pos++){
        int cc=in.get(); if(cc<0) throw std::runtime_error("short carrier");
        uint8_t y=(uint8_t)cc, x;
        if(pos<lag) x=y;
        else{
            uint8_t prev=hist[pos%lag];
            if(mode==PERIOD_XOR) x=uint8_t(y ^ prev);
            else x=uint8_t(y + prev);
        }
        out.put(char(x));
        if(pos<hist.size()) hist[pos]=x;
        else hist[pos%lag]=x;
    }
    if(in.peek()!=EOF) throw std::runtime_error("trailing carrier");
}
int main(int argc,char**argv){
    try{
        if(argc<4){
            std::cerr<<"usage:\n"
                     <<"  erpb_transform encode-transition in out\n"
                     <<"  erpb_transform encode-period-delta in out lag\n"
                     <<"  erpb_transform encode-period-xor in out lag\n"
                     <<"  erpb_transform decode in out\n";
            return 2;
        }
        std::string op=argv[1];
        if(op=="encode-transition") encode(argv[2],argv[3],TRANS_DELTA,1);
        else if(op=="encode-period-delta"){
            if(argc<5) throw std::runtime_error("need lag");
            encode(argv[2],argv[3],PERIOD_DELTA,(uint32_t)std::stoul(argv[4]));
        } else if(op=="encode-period-xor"){
            if(argc<5) throw std::runtime_error("need lag");
            encode(argv[2],argv[3],PERIOD_XOR,(uint32_t)std::stoul(argv[4]));
        } else if(op=="decode") decode(argv[2],argv[3]);
        else throw std::runtime_error("unknown op");
    }catch(const std::exception&e){ std::cerr<<"ERPB_ERROR "<<e.what()<<"\n"; return 1; }
    return 0;
}
