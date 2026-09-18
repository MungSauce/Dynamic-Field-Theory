#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <array>
static constexpr uint64_t BUDGET=50000000ULL,HEADER=512ULL,PAGE=1000000ULL;
static constexpr uint32_t N=(uint32_t)(BUDGET-HEADER),MOD=206;
static uint64_t mix(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;x^=x>>31;return x;}
static uint64_t comb(uint64_t a,uint64_t b){return mix(a^(mix(b+0x9e3779b97f4a7c15ULL)+0x517cc1b727220a95ULL));}
static uint64_t rq(uint64_t q){uint64_t s=0x243f6a8885a308d3ULL;for(int i=0;i<16;i++){s=comb(s,(q>>i)&1);s=comb(s,i);}return s;}
struct DSU{std::vector<uint32_t>p;std::vector<uint8_t>r,d;DSU():p(N),r(N),d(N){for(uint32_t i=0;i<N;i++)p[i]=i;}
std::pair<uint32_t,uint16_t>find(uint32_t x){uint32_t z=x;uint16_t a=0;while(p[z]!=z){a=(a+d[z])%MOD;z=p[z];}uint32_t c=x;uint16_t pref=0;while(p[c]!=c){uint32_t par=p[c];uint8_t e=d[c];p[c]=z;d[c]=(uint8_t)((a+MOD-pref)%MOD);pref=(pref+e)%MOD;c=par;}return{z,a};}
bool add(uint32_t a,uint32_t b,uint16_t w){auto A=find(a),B=find(b);if(A.first==B.first)return((A.second+MOD-B.second)%MOD)==w;if(r[A.first]<r[B.first]){p[A.first]=B.first;d[A.first]=(uint8_t)((w+MOD-A.second+B.second)%MOD);}else{p[B.first]=A.first;d[B.first]=(uint8_t)((MOD-w+A.second+MOD-B.second)%MOD);if(r[A.first]==r[B.first])r[A.first]++;}return true;}};
struct Rel{uint32_t a,b;uint16_t s;};
static Rel rel(int law,uint64_t t,uint64_t ctx){uint64_t q=t/PAGE,k=t%PAGE,R=rq(q),a,b,s;if(law==0){a=mix(t^0xa0761d6478bd642fULL);b=mix(t^0xe7037ed1a0b428dbULL);s=mix(t^0x8ebc6af09c88c6e3ULL);}else if(law==1){a=comb(mix(k),R);b=comb(mix(k^0x589965cc75374cc3ULL),comb(R,q));s=comb(R,comb(k,q));}else{a=comb(ctx,comb(R,k));b=comb(mix(ctx^0xd1b54a32d192ed03ULL),comb(k,R));s=comb(ctx,comb(R,comb(k,q)));}Rel z{(uint32_t)(a%N),(uint32_t)(b%N),(uint16_t)(s%MOD)};if(z.a==z.b)z.b=(z.b+1)%N;return z;}
static const char*name(int x){return x==0?"count":x==1?"page_recursive":"context_recursive";}
int main(int argc,char**argv){if(argc!=3)return 64;std::string s=argv[2];int law=s=="count"?0:s=="page_recursive"?1:s=="context_recursive"?2:-1;if(law<0)return 64;std::ifstream in(argv[1],std::ios::binary);if(!in)return 3;DSU u;std::array<int16_t,256>map;map.fill(-1);uint16_t terms=0;uint64_t t=0,ctx=0x6a09e667f3bcc909ULL;char ch;std::cout<<"BUDGET_BYTES="<<BUDGET<<" CARRIER_BYTES="<<N<<" TERMINAL_STATES=206 ONE_BUTTON_IMPRINT=true PAGE_BANK_BYTES=0 RESIDUAL_BYTES=0 CORRECTION_BYTES=0\n";while(in.get(ch)){uint8_t b=(uint8_t)ch;int code=map[b];if(code<0){if(terms>=206){std::cout<<"LAW="<<name(law)<<" status=TERMINAL_OVERFLOW t="<<t<<"\n";return 2;}map[b]=code=terms++;}Rel z=rel(law,t,ctx);uint16_t w=(code+MOD-z.s)%MOD;if(!u.add(z.a,z.b,w)){std::cout<<"LAW="<<name(law)<<" status=RELATIONAL_CONTRADICTION bytes_imprinted="<<t<<" page="<<t/PAGE<<" key="<<t%PAGE<<" terminals_seen="<<terms<<"\n";return 2;}ctx=comb(ctx,(uint64_t)b+1);++t;if(t%10000000ULL==0)std::cout<<"LAW="<<name(law)<<" progress="<<t<<" terminals_seen="<<terms<<"\n";}std::cout<<"LAW="<<name(law)<<" status=SURVIVED_FULL_SOURCE bytes_imprinted="<<t<<" terminals_seen="<<terms<<" freeze_replay_required=true\n";return 0;}

// CANONICAL REPLICATION NOTICE (2026-09-18): predecessor/lineage artifact. Current protocol: compression/EIS_K32_FORMAL_REPLICATION_V1.md
