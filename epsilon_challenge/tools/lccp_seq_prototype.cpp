#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static void put_u16(std::ostream& o,uint16_t v){o.put(char(v&255));o.put(char(v>>8));}
static uint16_t get_u16(std::istream& i){int a=i.get(),b=i.get();if(a<0||b<0)throw std::runtime_error("short u16");return uint16_t(a|(b<<8));}
static void put_i16(std::ostream& o,int16_t v){put_u16(o,uint16_t(v));}
static int16_t get_i16(std::istream& i){return int16_t(get_u16(i));}
static void put_u32(std::ostream& o,uint32_t v){for(int k=0;k<4;k++)o.put(char((v>>(8*k))&255));}
static uint32_t get_u32(std::istream& i){uint32_t v=0;for(int k=0;k<4;k++){int c=i.get();if(c<0)throw std::runtime_error("short u32");v|=uint32_t(c)<<(8*k);}return v;}
static void put_u64(std::ostream& o,uint64_t v){for(int k=0;k<8;k++)o.put(char((v>>(8*k))&255));}
static uint64_t get_u64(std::istream& i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}
static void put_var(std::ostream& o,uint64_t v){while(v>=128){o.put(char((v&127)|128));v>>=7;}o.put(char(v));}
static uint64_t get_var(std::istream& i){uint64_t v=0;int sh=0;for(;;){int c=i.get();if(c<0)throw std::runtime_error("short varint");v|=uint64_t(c&127)<<sh;if(!(c&128))return v;sh+=7;if(sh>63)throw std::runtime_error("varint overflow");}}
static uint64_t var_size(uint64_t v){uint64_t n=1;while(v>=128){v>>=7;++n;}return n;}

enum : uint8_t {
    F_RAW=0,
    F_SHIFT=1,
    F_CONTINUE=2
};

struct Patch {
    uint32_t pos;
    uint8_t value;
};

static std::vector<uint8_t> read_all(const std::string& p){
    std::ifstream in(p,std::ios::binary|std::ios::ate);
    if(!in)throw std::runtime_error("open input");
    auto sz=in.tellg(); if(sz<0)throw std::runtime_error("size");
    in.seekg(0);
    std::vector<uint8_t> d((size_t)sz);
    if(sz){in.read((char*)d.data(),sz);if(in.gcount()!=sz)throw std::runtime_error("short input");}
    return d;
}

static std::vector<uint8_t> shifted(const std::vector<uint8_t>& prev,uint32_t w,uint32_t h,int dx,int dy){
    std::vector<uint8_t> out(prev.size(),0);
    for(uint32_t y=0;y<h;y++){
        int sy=int(y)-dy;
        if(sy<0||sy>=int(h)) continue;
        for(uint32_t x=0;x<w;x++){
            int sx=int(x)-dx;
            if(sx<0||sx>=int(w)) continue;
            out[size_t(y)*w+x]=prev[size_t(sy)*w+sx];
        }
    }
    return out;
}

static std::vector<Patch> diff_patches(const std::vector<uint8_t>& pred,const std::vector<uint8_t>& cur){
    std::vector<Patch> p;
    for(uint32_t i=0;i<cur.size();i++) if(pred[i]!=cur[i]) p.push_back({i,cur[i]});
    return p;
}

static uint64_t patch_cost(const std::vector<Patch>& p){
    uint64_t n=var_size(p.size());
    uint64_t prev=0;
    bool first=true;
    for(auto &x:p){
        uint64_t gap=first?x.pos:uint64_t(x.pos)-prev;
        n+=var_size(gap)+1;
        prev=x.pos;
        first=false;
    }
    return n;
}

static void write_patches(std::ostream& out,const std::vector<Patch>& p){
    put_var(out,p.size());
    uint64_t prev=0;
    bool first=true;
    for(auto &x:p){
        uint64_t gap=first?x.pos:uint64_t(x.pos)-prev;
        put_var(out,gap);
        out.put(char(x.value));
        prev=x.pos;
        first=false;
    }
}

static void read_patches(std::istream& in,std::vector<uint8_t>& frame){
    uint64_t n=get_var(in);
    uint64_t pos=0;
    for(uint64_t k=0;k<n;k++){
        uint64_t gap=get_var(in);
        pos = (k==0)?gap:pos+gap;
        int v=in.get();
        if(v<0||pos>=frame.size())throw std::runtime_error("bad patch");
        frame[(size_t)pos]=uint8_t(v);
    }
}

struct Choice{
    uint8_t mode=F_RAW;
    int16_t dx=0,dy=0;
    uint64_t cost=std::numeric_limits<uint64_t>::max();
    std::vector<Patch> patches;
};

static Choice choose_frame(const std::vector<uint8_t>& prev,
                           const std::vector<uint8_t>& cur,
                           uint32_t w,uint32_t h,
                           bool have_prev_shift,int16_t prev_dx,int16_t prev_dy){
    Choice best;
    best.mode=F_RAW;
    best.cost=1+cur.size();

    static const std::array<std::pair<int,int>,13> cand{{
        {0,0},{1,0},{-1,0},{0,1},{0,-1},
        {1,1},{1,-1},{-1,1},{-1,-1},
        {2,0},{-2,0},{0,2},{0,-2}
    }};

    for(auto [dx,dy]:cand){
        auto pred=shifted(prev,w,h,dx,dy);
        auto p=diff_patches(pred,cur);
        uint8_t mode=(have_prev_shift && dx==prev_dx && dy==prev_dy)?F_CONTINUE:F_SHIFT;
        uint64_t meta=(mode==F_CONTINUE)?1:5;
        uint64_t c=meta+patch_cost(p);
        if(c<best.cost){
            best.mode=mode;
            best.dx=int16_t(dx);best.dy=int16_t(dy);
            best.cost=c;
            best.patches=std::move(p);
        }
    }
    return best;
}

static void encode(const std::string& inpath,const std::string& outpath,uint32_t w,uint32_t h){
    if(!w||!h)throw std::runtime_error("zero dimensions");
    uint64_t cells=uint64_t(w)*h;
    if(cells>std::numeric_limits<uint32_t>::max())throw std::runtime_error("frame too large for prototype");

    auto src=read_all(inpath);
    uint64_t frames=(src.size()+cells-1)/cells;

    std::ofstream out(outpath,std::ios::binary);
    if(!out)throw std::runtime_error("open output");
    out.write("LCS1",4);
    out.put(1);
    put_u32(out,w);put_u32(out,h);
    put_u64(out,src.size());
    put_u64(out,frames);

    std::vector<uint8_t> prev(cells,0),cur(cells,0);
    bool have_prev=false,have_shift=false;
    int16_t last_dx=0,last_dy=0;
    uint64_t raw_frames=0,moved_frames=0,continued_frames=0,total_patches=0;

    for(uint64_t fi=0;fi<frames;fi++){
        std::fill(cur.begin(),cur.end(),0);
        size_t off=(size_t)(fi*cells);
        size_t n=std::min<uint64_t>(cells,src.size()-off);
        std::copy(src.begin()+off,src.begin()+off+n,cur.begin());

        if(!have_prev){
            out.put(char(F_RAW));
            out.write((char*)cur.data(),cur.size());
            raw_frames++;
            have_prev=true;
            have_shift=false;
        }else{
            Choice ch=choose_frame(prev,cur,w,h,have_shift,last_dx,last_dy);
            out.put(char(ch.mode));
            if(ch.mode==F_RAW){
                out.write((char*)cur.data(),cur.size());
                raw_frames++;
                have_shift=false;
            }else{
                if(ch.mode==F_SHIFT){
                    put_i16(out,ch.dx);put_i16(out,ch.dy);
                    last_dx=ch.dx;last_dy=ch.dy;
                    have_shift=true;
                    moved_frames++;
                }else{
                    continued_frames++;
                }
                write_patches(out,ch.patches);
                total_patches+=ch.patches.size();
            }
        }
        prev.swap(cur);
    }

    out.flush();
    std::ifstream chk(outpath,std::ios::binary|std::ios::ate);
    uint64_t actual=chk?(uint64_t)chk.tellg():0;
    std::cerr<<"SEQ_SOURCE_BYTES="<<src.size()<<"\n";
    std::cerr<<"SEQ_FRAMES="<<frames<<"\n";
    std::cerr<<"SEQ_RAW_FRAMES="<<raw_frames<<"\n";
    std::cerr<<"SEQ_MOVED_FRAMES="<<moved_frames<<"\n";
    std::cerr<<"SEQ_CONTINUED_FRAMES="<<continued_frames<<"\n";
    std::cerr<<"SEQ_PATCHES="<<total_patches<<"\n";
    std::cerr<<"SEQ_ARTIFACT_BYTES="<<actual<<"\n";
}

static void decode(const std::string& inpath,const std::string& outpath){
    std::ifstream in(inpath,std::ios::binary);
    if(!in)throw std::runtime_error("open carrier");
    char m[4];in.read(m,4);
    if(in.gcount()!=4||std::string(m,4)!="LCS1")throw std::runtime_error("magic");
    if(in.get()!=1)throw std::runtime_error("version");
    uint32_t w=get_u32(in),h=get_u32(in);
    uint64_t total=get_u64(in),frames=get_u64(in);
    uint64_t cells=uint64_t(w)*h;
    if(!cells||cells>std::numeric_limits<uint32_t>::max())throw std::runtime_error("bad dimensions");

    std::ofstream out(outpath,std::ios::binary);
    if(!out)throw std::runtime_error("open decoded");

    std::vector<uint8_t> prev(cells,0),cur(cells,0);
    bool have_prev=false,have_shift=false;
    int16_t last_dx=0,last_dy=0;
    uint64_t written=0;

    for(uint64_t fi=0;fi<frames;fi++){
        int mode=in.get();
        if(mode<0)throw std::runtime_error("short frame mode");
        if(mode==F_RAW){
            in.read((char*)cur.data(),cur.size());
            if((size_t)in.gcount()!=cur.size())throw std::runtime_error("short raw frame");
            have_shift=false;
        }else if(mode==F_SHIFT || mode==F_CONTINUE){
            if(!have_prev)throw std::runtime_error("motion before first frame");
            if(mode==F_SHIFT){
                last_dx=get_i16(in);last_dy=get_i16(in);have_shift=true;
            }else if(!have_shift){
                throw std::runtime_error("continue without shift");
            }
            cur=shifted(prev,w,h,last_dx,last_dy);
            read_patches(in,cur);
        }else throw std::runtime_error("bad frame mode");

        uint64_t remaining=total-written;
        size_t n=(size_t)std::min<uint64_t>(cells,remaining);
        if(n)out.write((char*)cur.data(),n);
        written+=n;
        prev=cur;
        have_prev=true;
    }

    if(written!=total)throw std::runtime_error("size mismatch");
    if(in.peek()!=EOF)throw std::runtime_error("trailing carrier data");
    std::cerr<<"SEQ_RECOVERED_BYTES="<<written<<"\n";
}

int main(int argc,char**argv){
    try{
        if(argc<4){
            std::cerr<<"usage: lccp_seq encode input output.lcs [width height]\n"
                     <<"       lccp_seq decode input.lcs output\n";
            return 2;
        }
        std::string a=argv[1];
        if(a=="encode"||a=="e"){
            uint32_t w=argc>4?uint32_t(std::stoul(argv[4])):256;
            uint32_t h=argc>5?uint32_t(std::stoul(argv[5])):256;
            encode(argv[2],argv[3],w,h);
        }else if(a=="decode"||a=="d"){
            decode(argv[2],argv[3]);
        }else throw std::runtime_error("unknown action");
    }catch(const std::exception&e){
        std::cerr<<"ERROR "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
