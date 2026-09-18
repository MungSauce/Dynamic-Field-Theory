#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
static constexpr uint64_t BUDGET=50000000ULL,HEADER=512ULL,PAGE=1000000ULL;
static constexpr uint32_t N=(uint32_t)(BUDGET-HEADER),M=206; static constexpr uint8_t U=255;
static uint64_t mix(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;x^=x>>31;return x;}
static uint64_t comb(uint64_t a,uint64_t b){return mix(a^(mix(b+0x9e3779b97f4a7c15ULL)+0x517cc1b727220a95ULL));}
static uint64_t rq(uint64_t q){uint64_t s=0x243f6a8885a308d3ULL;for(int i=0;i<16;i++){s=comb(s,(q>>i)&1);s=comb(s,i);}return s;}
static uint32_t inv0(uint32_t a,uint32_t mod){int64_t t=0,nt=1,r=mod,nr=a;while(nr){int64_t q=r/nr,z=t-q*nt;t=nt;nt=z;z=r-q*nr;r=nr;nr=z;}if(r!=1)return 0;if(t<0)t+=mod;return(uint32_t)t;}
static std::array<uint8_t,206>I206=[](){std::array<uint8_t,206>a{};for(int i=1;i<206;i++)a[i]=(uint8_t)inv0(i,206);return a;}();
static std::array<uint8_t,103>I103=[](){std::array<uint8_t,103>a{};for(int i=1;i<103;i++)a[i]=(uint8_t)inv0(i,103);return a;}();
static uint8_t unit(uint64_t h){uint16_t x=(uint16_t)(2*(h%102)+1);if(x==103)x=205;return(uint8_t)x;}
struct F{uint32_t r;uint8_t m,a;};
struct DSU{std::vector<uint32_t>p;std::vector<uint8_t>rank,mul,add,par,r103;DSU():p(N),rank(N),mul(N,1),add(N),par(N,U),r103(N,U){for(uint32_t i=0;i<N;i++)p[i]=i;}
F find(uint32_t x){if(p[x]==x)return{x,1,0};uint32_t pp=p[x];uint8_t m0=mul[x],a0=add[x];F f=find(pp);uint8_t nm=(uint8_t)((m0*f.m)%M),na=(uint8_t)((m0*f.a+a0)%M);p[x]=f.r;mul[x]=nm;add[x]=na;return{f.r,nm,na};}
bool impose(uint32_t r,uint16_t c,uint16_t rhs){if((c&1)==0){if(rhs&1)return false;}else{uint8_t v=rhs&1;if(par[r]!=U&&par[r]!=v)return false;par[r]=v;}uint16_t c3=c%103,r3=rhs%103;if(c3==0){if(r3)return false;}else{uint8_t v=(uint8_t)((r3*I103[c3])%103);if(r103[r]!=U&&r103[r]!=v)return false;r103[r]=v;}return true;}
bool inherit(uint32_t c,uint32_t p0,uint8_t u,uint8_t v){if(par[c]!=U){uint8_t need=(uint8_t)((par[c]+2-(v&1))&1);if(par[p0]!=U&&par[p0]!=need)return false;par[p0]=need;}if(r103[c]!=U){uint8_t um=u%103,vm=v%103,need=(uint8_t)(((r103[c]+103-vm)%103*I103[um])%103);if(r103[p0]!=U&&r103[p0]!=need)return false;r103[p0]=need;}return true;}
bool link(uint32_t a,uint32_t b,uint8_t rm,uint8_t rc){F A=find(a),B=find(b);if(A.r==B.r){uint16_t c=(A.m+M-(rm*B.m)%M)%M,rhs=((rm*B.a)%M+rc+M-A.a)%M;return impose(A.r,c,rhs);}if(rank[A.r]<rank[B.r]){uint8_t ia=I206[A.m],u=(uint8_t)((uint32_t)ia*rm%M*B.m%M),v=(uint8_t)((uint32_t)ia*(((rm*B.a)%M+rc+M-A.a)%M)%M);if(!inherit(A.r,B.r,u,v))return false;p[A.r]=B.r;mul[A.r]=u;add[A.r]=v;}else{uint8_t den=(uint8_t)((rm*B.m)%M),id=I206[den],u=(uint8_t)((uint32_t)id*A.m%M);uint16_t num=(A.a+M-(rm*B.a)%M+M-rc)%M;uint8_t v=(uint8_t)((uint32_t)id*num%M);if(!inherit(B.r,A.r,u,v))return false;p[B.r]=A.r;mul[B.r]=u;add[B.r]=v;if(rank[A.r]==rank[B.r])rank[A.r]++;}return true;}};
struct Rel{uint32_t a,b;uint8_t ca,cb;uint16_t salt;};
static Rel rel(int law,uint64_t t,uint64_t ctx){uint64_t q=t/PAGE,k=t%PAGE,R=rq(q),ha,hb,hs;if(law==0){ha=mix(t^0xa0761d6478bd642fULL);hb=mix(t^0xe7037ed1a0b428dbULL);hs=mix(t^0x8ebc6af09c88c6e3ULL);}else if(law==1){ha=comb(mix(k),R);hb=comb(mix(k^0x589965cc75374cc3ULL),comb(R,q));hs=comb(R,comb(k,q));}else{ha=comb(ctx,comb(R,k));hb=comb(mix(ctx^0xd1b54a32d192ed03ULL),comb(k,R));hs=comb(ctx,comb(R,comb(k,q)));}Rel z{(uint32_t)(ha%N),(uint32_t)(hb%N),unit(comb(ha,t^0x11)),unit(comb(hb,t^0x22)),(uint16_t)(hs%M)};if(z.a==z.b)z.b=(z.b+1)%N;return z;}
static const char*name(int x){return x==0?"count_affine":x==1?"page_affine":"context_affine";}
int main(int argc,char**argv){if(argc!=3)return 64;std::string s=argv[2];int law=s=="count_affine"?0:s=="page_affine"?1:s=="context_affine"?2:-1;if(law<0)return 64;std::ifstream in(argv[1],std::ios::binary);if(!in)return 3;DSU u;std::array<int16_t,256>map;map.fill(-1);uint16_t terms=0;uint64_t t=0,ctx=0x6a09e667f3bcc909ULL;char ch;std::cout<<"ACT206_V2 BUDGET_BYTES="<<BUDGET<<" CARRIER_BYTES="<<N<<" WIDTH_BYTES_PER_CARRIER=1 TERMINAL_STATES=206 ONE_BUTTON_IMPRINT=true\n";while(in.get(ch)){uint8_t b=(uint8_t)ch;int code=map[b];if(code<0){if(terms>=206){std::cout<<"LAW="<<name(law)<<" status=TERMINAL_OVERFLOW t="<<t<<"\n";return 2;}map[b]=code=terms++;}Rel z=rel(law,t,ctx);uint16_t w=(code+M-z.salt)%M;uint8_t ia=I206[z.ca],rm=(uint8_t)((uint32_t)ia*z.cb%M),rc=(uint8_t)((uint32_t)ia*w%M);if(!u.link(z.a,z.b,rm,rc)){std::cout<<"LAW="<<name(law)<<" status=RELATIONAL_CONTRADICTION bytes_imprinted="<<t<<" page="<<t/PAGE<<" key="<<t%PAGE<<" terminals_seen="<<terms<<"\n";return 2;}ctx=comb(ctx,(uint64_t)b+1);++t;if(t%10000000ULL==0)std::cout<<"LAW="<<name(law)<<" progress="<<t<<" terminals_seen="<<terms<<"\n";}std::cout<<"LAW="<<name(law)<<" status=SURVIVED_FULL_SOURCE bytes_imprinted="<<t<<" terminals_seen="<<terms<<" freeze_replay_required=true\n";return 0;}
