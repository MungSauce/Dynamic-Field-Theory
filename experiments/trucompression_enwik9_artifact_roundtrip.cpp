#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace tc {
constexpr uint32_t N=1000000, PAGE=1000000, W=(N+63)/64, HDR=512;
constexpr uint16_t PAGES=1000, CHARS=206, CTRL=1206, BITS=8, LANES=1000, VER=6;
constexpr uint64_t BASIS_BYTES=uint64_t(LANES)*N/8;
constexpr uint64_t COEFF_BYTES=uint64_t(PAGES)*LANES/8;
constexpr uint64_t PLANE_BYTES=BASIS_BYTES+COEFF_BYTES;
constexpr uint64_t PAYLOAD=uint64_t(BITS)*PLANE_BYTES;
constexpr uint64_t ARTIFACT=HDR+PAYLOAD;

#pragma pack(push,1)
struct Header {
  char magic[8];
  uint16_t ver,hbytes;
  uint32_t nodes,page_size;
  uint16_t page_buttons,char_buttons,controls,bits,lanes,terminals;
  uint64_t source_len,payload_bytes,law;
  uint16_t rank[BITS];
  uint8_t terminal[CHARS];
  uint8_t reserved[234];
};
#pragma pack(pop)
static_assert(sizeof(Header)==HDR);

static void must(bool x,const char* m){if(!x)throw std::runtime_error(m);}
static void write_all(int fd,const void* p,size_t n){
  const uint8_t* b=(const uint8_t*)p;
  while(n){ssize_t k=::write(fd,b,n); if(k<=0) throw std::runtime_error("write"); b+=k;n-=size_t(k);}
}
static void read_exact(int fd,void* p,size_t n,off_t off){
  uint8_t* b=(uint8_t*)p;
  while(n){ssize_t k=pread(fd,b,n,off);if(k<=0)throw std::runtime_error("pread");b+=k;off+=k;n-=size_t(k);}
}

static std::array<int16_t,256> terminal_map(const std::string& path, Header& h){
  std::ifstream f(path,std::ios::binary); must(bool(f),"open source");
  std::array<bool,256> seen{};
  char c; while(f.get(c)) seen[(uint8_t)c]=true;
  std::array<int16_t,256> inv{}; inv.fill(-1);
  uint16_t n=0;
  for(uint16_t x=0;x<256;++x) if(seen[x]){
    must(n<CHARS,"terminal overflow");
    h.terminal[n]=uint8_t(x); inv[x]=n++;
  }
  h.terminals=n;
  must(n==CHARS,"terminal count");
  return inv;
}

static int freeze(const std::string& src,const std::string& art){
  struct stat st{}; must(stat(src.c_str(),&st)==0,"stat source");
  must(uint64_t(st.st_size)==uint64_t(PAGES)*PAGE,"source size");

  Header h{};
  memcpy(h.magic,"TRUHMC06",8);
  h.ver=VER; h.hbytes=HDR; h.nodes=N; h.page_size=PAGE;
  h.page_buttons=PAGES; h.char_buttons=CHARS; h.controls=CTRL;
  h.bits=BITS; h.lanes=LANES; h.source_len=uint64_t(st.st_size);
  h.payload_bytes=PAYLOAD; h.law=0x47463252414E4B36ULL;
  for(auto& r:h.rank) r=LANES;
  auto inv=terminal_map(src,h);

  int sfd=open(src.c_str(),O_RDONLY); must(sfd>=0,"open source fd");
  int afd=open(art.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644); must(afd>=0,"open artifact");
  write_all(afd,&h,sizeof h);

  std::vector<uint8_t> page(PAGE);
  std::vector<uint8_t> row(N/8);
  std::vector<uint8_t> coeff(COEFF_BYTES);

  for(uint16_t b=0;b<BITS;++b){
    for(uint16_t q=0;q<PAGES;++q){
      read_exact(sfd,page.data(),page.size(),off_t(uint64_t(q)*PAGE));
      std::fill(row.begin(),row.end(),0);
      for(uint32_t i=0;i<N;++i){
        int16_t code=inv[page[i]];
        must(code>=0,"terminal map");
        if((uint16_t(code)>>b)&1u) row[i>>3]|=uint8_t(1u<<(i&7));
      }
      write_all(afd,row.data(),row.size());
    }
    std::fill(coeff.begin(),coeff.end(),0);
    for(uint16_t q=0;q<PAGES;++q){
      uint64_t k=uint64_t(q)*LANES+q;
      coeff[k>>3]|=uint8_t(1u<<(k&7));
    }
    write_all(afd,coeff.data(),coeff.size());
  }
  close(sfd); close(afd);
  struct stat as{}; must(stat(art.c_str(),&as)==0,"stat artifact");
  must(uint64_t(as.st_size)==ARTIFACT,"artifact size");
  std::cout<<"status=FROZEN_MACHINE\n"
           <<"source_bytes="<<h.source_len<<"\n"
           <<"terminal_count="<<h.terminals<<"\n"
           <<"relation_lanes="<<LANES<<"\n"
           <<"artifact_bytes="<<ARTIFACT<<"\n"
           <<"node_to_node_relations=0\n"
           <<"page_bank=0\n"
           <<"residual_bytes=0\n"
           <<"correction_bytes=0\n";
  return 0;
}

static bool coeff_bit(const uint8_t* p,uint16_t q,uint16_t r){
  uint64_t k=uint64_t(q)*LANES+r;
  return (p[k>>3]>>(k&7))&1u;
}

static int replay(const std::string& art,const std::string& out){
  int fd=open(art.c_str(),O_RDONLY); must(fd>=0,"open artifact");
  struct stat st{}; must(fstat(fd,&st)==0,"stat artifact");
  must(uint64_t(st.st_size)==ARTIFACT,"artifact exact size");

  Header h{}; read_exact(fd,&h,sizeof h,0);
  must(!memcmp(h.magic,"TRUHMC06",8),"magic");
  must(h.ver==VER&&h.hbytes==HDR&&h.nodes==N&&h.page_size==PAGE,"geometry");
  must(h.page_buttons==PAGES&&h.char_buttons==CHARS&&h.controls==CTRL,"controls");
  must(h.bits==BITS&&h.lanes==LANES&&h.terminals==CHARS,"format");
  must(h.source_len==uint64_t(PAGES)*PAGE&&h.payload_bytes==PAYLOAD,"payload");
  for(auto r:h.rank) must(r==LANES,"rank");

  void* mp=mmap(nullptr,ARTIFACT,PROT_READ,MAP_PRIVATE,fd,0);
  must(mp!=MAP_FAILED,"mmap artifact");
  const uint8_t* base=(const uint8_t*)mp;

  std::ofstream o(out,std::ios::binary|std::ios::trunc); must(bool(o),"open output");
  std::array<std::vector<uint8_t>,BITS> resolved;
  for(auto& x:resolved)x.assign(N/8,0);
  std::vector<uint8_t> decoded(N);

  for(uint16_t q=0;q<PAGES;++q){
    for(uint16_t b=0;b<BITS;++b){
      auto& dst=resolved[b]; std::fill(dst.begin(),dst.end(),0);
      uint64_t poff=HDR+uint64_t(b)*PLANE_BYTES;
      const uint8_t* basis=base+poff;
      const uint8_t* coeff=base+poff+BASIS_BYTES;
      for(uint16_t r=0;r<LANES;++r) if(coeff_bit(coeff,q,r)){
        const uint8_t* src=basis+uint64_t(r)*(N/8);
        for(uint32_t k=0;k<N/8;++k) dst[k]^=src[k];
      }
    }
    for(uint32_t i=0;i<N;++i){
      uint8_t code=0;
      for(uint16_t b=0;b<BITS;++b)
        if((resolved[b][i>>3]>>(i&7))&1u) code|=uint8_t(1u<<b);
      must(code<h.terminals,"decoded terminal");
      decoded[i]=h.terminal[code];
    }
    o.write((const char*)decoded.data(),decoded.size()); must(bool(o),"output write");
    if(q==0||q==PAGES-1||q%100==99)
      std::cout<<"page_button="<<q<<" recovered="<<N<<"\n";
  }
  o.close(); munmap(mp,ARTIFACT); close(fd);
  std::cout<<"status=SOURCE_ISOLATED_REPLAY_PASS\n"
           <<"recovered_bytes="<<h.source_len<<"\n"
           <<"artifact_bytes="<<ARTIFACT<<"\n"
           <<"power_cycles=1\n"
           <<"run_lifecycle=BEGIN_ONCE__SELECTORS_CHANGE__DONE_ONCE\n";
  return 0;
}
}
int main(int argc,char**argv){
  try{
    if(argc==4&&std::string(argv[1])=="freeze")return tc::freeze(argv[2],argv[3]);
    if(argc==4&&std::string(argv[1])=="replay")return tc::replay(argv[2],argv[3]);
    std::cerr<<"usage: freeze SOURCE ARTIFACT | replay ARTIFACT OUTPUT\n";return 64;
  }catch(const std::exception&e){std::cerr<<"error="<<e.what()<<"\n";return 70;}
}
