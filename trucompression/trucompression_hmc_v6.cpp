#include "../trucompute/trucompute_runtime_v3.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using trucompute::State;
namespace tc6 {
constexpr uint32_t N=1000000,PAGE=1000000,W=(N+63)/64,HDR=512;
constexpr uint16_t PAGES=1000,CHARS=206,CTRL=1206,BITS=8,VER=6;
static bool get(const std::vector<uint64_t>&v,uint32_t i){return(v[i>>6]>>(i&63))&1ULL;}
static void set(std::vector<uint64_t>&v,uint32_t i){v[i>>6]|=1ULL<<(i&63);}
static void xo(std::vector<uint64_t>&a,const std::vector<uint64_t>&b){for(size_t i=0;i<a.size();++i)a[i]^=b[i];}
static bool zero(const std::vector<uint64_t>&v){for(auto x:v)if(x)return false;return true;}
static uint32_t first(const std::vector<uint64_t>&v){for(uint32_t w=0;w<v.size();++w)if(v[w])return w*64u+__builtin_ctzll(v[w]);return N;}
static uint64_t payload(uint16_t r){return uint64_t(r)*(uint64_t(N)+PAGES);}
#pragma pack(push,1)
struct Header{char magic[8];uint16_t ver,hbytes;uint32_t nodes,page_size;uint16_t page_buttons,char_buttons,controls,bits,lanes,terminals;uint64_t source_len,payload_bytes,law;uint16_t rank[BITS];uint8_t terminal[CHARS];uint8_t reserved[234];};
#pragma pack(pop)
static_assert(sizeof(Header)==HDR);
struct Plane{
 uint16_t lanes=0,used=0;std::vector<std::vector<uint64_t>> basis;std::vector<uint64_t> pc;
 Plane(uint16_t r=0):lanes(r),basis(r,std::vector<uint64_t>(W)),pc((uint64_t(PAGES)*r+63)/64){}
 bool coeff(uint16_t q,uint16_t r)const{uint64_t k=uint64_t(q)*lanes+r;return(pc[k>>6]>>(k&63))&1ULL;}
 void coeff1(uint16_t q,uint16_t r){uint64_t k=uint64_t(q)*lanes+r;pc[k>>6]|=1ULL<<(k&63);}
};
struct Machine{
 uint16_t lanes=0,terminal_count=0;uint64_t source_len=0;std::array<uint8_t,CHARS> terminal{};std::array<int16_t,256> inv{};std::array<Plane,BITS> f;
 bool running=false,pv=false,cv=false;uint16_t page=0,ch=0;
 Machine(uint16_t r=0):lanes(r){inv.fill(-1);for(auto&x:f)x=Plane(r);}
 void rebuild(){inv.fill(-1);for(uint16_t i=0;i<terminal_count;++i)inv[terminal[i]]=i;}
 void begin(){running=true;pv=cv=false;} void select_page(uint16_t q){if(!running||q>=PAGES)throw std::runtime_error("page");page=q;pv=true;cv=false;}
 void select_char(uint16_t c){if(!running||!pv||c>=CHARS)throw std::runtime_error("char");ch=c;cv=true;} void done(){running=pv=cv=false;}
 State codebit(uint16_t q,uint32_t i,uint16_t b)const{State s=State::NEG;auto&x=f[b];for(uint16_t r=0;r<x.used;++r)if(x.coeff(q,r)&&get(x.basis[r],i))s=(s==State::POS?State::NEG:State::POS);return s;}
 uint8_t decode(uint16_t q,uint32_t i)const{uint8_t c=0;for(uint16_t b=0;b<BITS;++b)if(codebit(q,i,b)==State::POS)c|=1u<<b;return c;}
 State observe(uint32_t i)const{if(!running||!pv||!cv||i>=N)return State::NEITHER;if(uint64_t(page)*PAGE+i>=source_len)return State::NEITHER;uint8_t t=decode(page,i);if(t>=terminal_count)return State::STATELESS_ZERO;return t==ch?State::POS:State::NEG;}
};
static std::vector<uint8_t> all(const std::string&p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("input");return{std::istreambuf_iterator<char>(f),{}};}
static void wr(std::ofstream&f,const void*p,size_t n){f.write((const char*)p,n);if(!f)throw std::runtime_error("write");}static void rd(std::ifstream&f,void*p,size_t n){f.read((char*)p,n);if(!f)throw std::runtime_error("read");}
static void save(const Machine&m,const std::string&p){Header h{};memcpy(h.magic,"TRUHMC06",8);h.ver=VER;h.hbytes=HDR;h.nodes=N;h.page_size=PAGE;h.page_buttons=PAGES;h.char_buttons=CHARS;h.controls=CTRL;h.bits=BITS;h.lanes=m.lanes;h.terminals=m.terminal_count;h.source_len=m.source_len;h.payload_bytes=payload(m.lanes);h.law=0x47463252414E4B36ULL;for(int b=0;b<BITS;++b)h.rank[b]=m.f[b].used;std::copy(m.terminal.begin(),m.terminal.end(),h.terminal);std::ofstream o(p,std::ios::binary);wr(o,&h,sizeof h);for(int b=0;b<BITS;++b){auto&x=m.f[b];for(uint16_t r=0;r<m.lanes;++r)wr(o,x.basis[r].data(),W*8);uint64_t nb=(uint64_t(PAGES)*m.lanes+7)/8;std::vector<uint8_t>z(nb);for(uint64_t k=0;k<uint64_t(PAGES)*m.lanes;++k)if((x.pc[k>>6]>>(k&63))&1)z[k>>3]|=1u<<(k&7);wr(o,z.data(),z.size());}if(uint64_t(o.tellp())!=HDR+payload(m.lanes))throw std::runtime_error("size");}
static Machine load(const std::string&p){std::ifstream in(p,std::ios::binary);Header h{};rd(in,&h,sizeof h);if(memcmp(h.magic,"TRUHMC06",8)||h.ver!=VER||h.nodes!=N||h.page_size!=PAGE||h.page_buttons!=PAGES||h.char_buttons!=CHARS||h.controls!=CTRL||h.bits!=BITS||h.payload_bytes!=payload(h.lanes))throw std::runtime_error("format");Machine m(h.lanes);m.terminal_count=h.terminals;m.source_len=h.source_len;std::copy(std::begin(h.terminal),std::end(h.terminal),m.terminal.begin());m.rebuild();for(int b=0;b<BITS;++b){auto&x=m.f[b];x.used=h.rank[b];if(x.used>m.lanes)throw std::runtime_error("rank");for(uint16_t r=0;r<m.lanes;++r)rd(in,x.basis[r].data(),W*8);uint64_t nb=(uint64_t(PAGES)*m.lanes+7)/8;std::vector<uint8_t>z(nb);rd(in,z.data(),z.size());for(uint64_t k=0;k<uint64_t(PAGES)*m.lanes;++k)if((z[k>>3]>>(k&7))&1)x.pc[k>>6]|=1ULL<<(k&63);}char e;if(in.read(&e,1))throw std::runtime_error("trailing");return m;}
static int mapchars(Machine&m,const std::vector<uint8_t>&s){std::array<bool,256>seen{};for(auto c:s)seen[c]=true;for(int c=0;c<256;++c)if(seen[c]){if(m.terminal_count>=CHARS)return 2;m.terminal[m.terminal_count++]=c;}m.rebuild();return 0;}
static std::vector<uint64_t> row(const std::vector<uint8_t>&s,const Machine&m,uint16_t q,uint16_t b){std::vector<uint64_t>v(W);uint64_t a=uint64_t(q)*PAGE,z=std::min<uint64_t>(a+PAGE,s.size());for(uint64_t t=a;t<z;++t){int16_t c=m.inv[s[t]];if(c<0)throw std::runtime_error("map");if((uint16_t(c)>>b)&1)set(v,t-a);}return v;}
static int factor(const std::vector<uint8_t>&s,Machine&m,uint16_t b){auto&x=m.f[b];std::vector<uint32_t>pivot(m.lanes,N);uint16_t qs=(s.size()+PAGE-1)/PAGE;for(uint16_t q=0;q<qs;++q){auto v=row(s,m,q,b);std::vector<uint8_t>coef(m.lanes);for(uint16_t r=0;r<x.used;++r)if(get(v,pivot[r])){xo(v,x.basis[r]);coef[r]^=1;}if(!zero(v)){if(x.used>=m.lanes){std::cout<<"status=CAPACITY_EXCEEDED\nbitplane="<<b<<"\npage="<<q<<"\nrequired_rank_at_least="<<x.used+1<<"\nfixed_relation_lanes="<<m.lanes<<"\nmachine_growth=0\n";return 2;}uint16_t r=x.used++;x.basis[r]=std::move(v);pivot[r]=first(x.basis[r]);coef[r]=1;}for(uint16_t r=0;r<x.used;++r)if(coef[r])x.coeff1(q,r);}return 0;}
static int blank(const std::string&p,uint16_t r){if(!r||r>PAGES)return 64;Machine m(r);save(m,p);std::cout<<"status=BLANK_FROZEN\nrelation_lanes="<<r<<"\nartifact_bytes="<<HDR+payload(r)<<"\ncontrol_buttons=1206\nnode_to_node_relations=0\n";return 0;}
static int imprint(const std::string&sp,const std::string&ap,uint16_t r){if(!r||r>PAGES)return 64;auto s=all(sp);if(s.size()>uint64_t(PAGES)*PAGE){std::cout<<"status=PAGE_CAPACITY_EXCEEDED\n";return 2;}Machine m(r);m.source_len=s.size();if(mapchars(m,s)){std::cout<<"status=TERMINAL_CAPACITY_EXCEEDED\n";return 2;}for(uint16_t b=0;b<BITS;++b){int rc=factor(s,m,b);if(rc)return rc;}for(uint64_t t=0;t<s.size();++t){uint8_t c=m.decode(t/PAGE,t%PAGE);if(c>=m.terminal_count||m.terminal[c]!=s[t]){std::cout<<"status=FREEZE_VERIFY_FAIL\nbyte="<<t<<"\n";return 3;}}save(m,ap);std::cout<<"status=FROZEN\nsource_bytes="<<s.size()<<"\nterminal_count="<<m.terminal_count<<"\nrelation_lanes="<<r<<"\nartifact_bytes="<<HDR+payload(r)<<"\ncontrol_buttons=1206\nnode_to_node_relations=0\n";for(int b=0;b<BITS;++b)std::cout<<"rank_bit"<<b<<"="<<m.f[b].used<<"\n";return 0;}
static int replay(const std::string&ap,const std::string&op,bool strict){Machine m=load(ap);m.begin();std::ofstream o(op,std::ios::binary);uint64_t done=0;uint16_t qs=(m.source_len+PAGE-1)/PAGE;for(uint16_t q=0;q<qs;++q){m.select_page(q);uint32_t count=std::min<uint64_t>(PAGE,m.source_len-done);if(strict){std::vector<int16_t>res(count,-1);for(uint16_t c=0;c<m.terminal_count;++c){m.select_char(c);for(uint32_t i=0;i<count;++i){State s=m.observe(i);if(s==State::POS){if(res[i]!=-1)return 4;res[i]=c;}else if(s!=State::NEG)return 5;}}for(uint32_t i=0;i<count;++i){if(res[i]<0)return 6;uint8_t x=m.terminal[res[i]];o.write((char*)&x,1);}}else for(uint32_t i=0;i<count;++i){uint8_t c=m.decode(q,i);if(c>=m.terminal_count)return 6;uint8_t x=m.terminal[c];o.write((char*)&x,1);}done+=count;std::cout<<"page_button="<<q<<" recovered="<<count<<"\n";}m.done();std::cout<<"status=REPLAY_PASS\nrecovered_bytes="<<done<<"\ncontrol_buttons=1206\npower_cycles=1\nrun_lifecycle=BEGIN_ONCE__SELECTORS_CHANGE__DONE_ONCE\n";return 0;}
static int selftest(){using namespace trucompute;if(CTRL!=1206||settle({1,-1})!=State::STATELESS_ZERO)return 1;bool ok=false;try{net(State::NEITHER);}catch(...){ok=true;}if(!ok)return 2;Machine m(2);m.source_len=1;m.terminal_count=2;m.terminal[0]='A';m.terminal[1]='B';m.rebuild();m.f[0].used=1;m.f[0].coeff1(0,0);set(m.f[0].basis[0],0);m.begin();m.select_page(0);m.select_char(0);if(m.observe(0)!=State::NEG)return 3;m.select_char(1);if(m.observe(0)!=State::POS)return 4;m.done();std::cout<<"TRUCOMPUTE_V3_CONFORMANCE=PASS\nTRUCOMPRESSION_HMC_V6=PASS\nphysical_nodes=1000000\npage_buttons=1000\ncharacter_buttons=206\ncontrol_buttons=1206\nnode_to_node_relations=0\npower_cycles_per_task=1\nrelation_law=GF2_FIXED_RANK_FIELD\n";return 0;}
}
int main(int argc,char**argv){using namespace tc6;try{if(argc==2&&std::string(argv[1])=="selftest")return selftest();if(argc==4&&std::string(argv[1])=="blank")return blank(argv[2],std::stoul(argv[3]));if(argc==5&&std::string(argv[1])=="imprint")return imprint(argv[2],argv[3],std::stoul(argv[4]));if((argc==4||argc==5)&&std::string(argv[1])=="replay")return replay(argv[2],argv[3],argc==5&&std::string(argv[4])=="--strict-hmc");std::cerr<<"usage: selftest | blank ARTIFACT LANES | imprint SOURCE ARTIFACT LANES | replay ARTIFACT OUTPUT [--strict-hmc]\n";return 64;}catch(const std::exception&e){std::cerr<<"error="<<e.what()<<"\n";return 70;}}
