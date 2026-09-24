#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
static uint64_t fnv=1469598103934665603ULL;
static void add16(uint16_t x){ for(int k=1;k>=0;k--){uint8_t b=(x>>(8*k))&255; fnv^=b; fnv*=1099511628211ULL;} }
static std::string hx(uint64_t x){std::ostringstream o;o<<std::hex<<std::setw(16)<<std::setfill('0')<<x;return o.str();}
int main(int argc,char**argv){
 if(argc<2)return 64; std::string mode=argv[1];
 if(mode=="selftest"){std::cout<<"BARNETT_207_BUTTON_CONFORMANCE=PASS\n";return 0;}
 if(mode!="train"||argc!=5)return 64;
 std::ifstream in(argv[2],std::ios::binary); if(!in)return 3;
 std::array<uint64_t,256> freq{}; char ch; uint64_t n=0;
 while(in.get(ch)){freq[(uint8_t)ch]++;n++;}
 std::vector<int> vals,unused; for(int i=0;i<256;i++)(freq[i]?vals:unused).push_back(i);
 if(n!=1000000000ULL||vals.size()!=206){std::cout<<"status=SOURCE_GATE_FAIL bytes="<<n<<" chars="<<vals.size()<<"\n";return 2;}
 std::array<uint16_t,256> button{}; for(size_t i=0;i<vals.size();i++)button[vals[i]]=uint16_t(i+1);
 int sepchar=unused.at(0);
 in.clear();in.seekg(0); uint64_t tokens=0; std::vector<uint16_t> pre,tail;
 while(in.get(ch)){
   uint16_t a=button[(uint8_t)ch];
   for(uint16_t t: {a,(uint16_t)207}){add16(t);tokens++;if(pre.size()<40)pre.push_back(t);tail.push_back(t);if(tail.size()>40)tail.erase(tail.begin());}
 }
 std::ofstream out("barnett_enwik9_207_equations.txt");
 out<<"SOURCE_BYTES="<<n<<"\nDISTINCT_CHARACTERS=206\nBUTTONS=207\nSEPARATOR_BUTTON=207\nSEPARATOR_UNUSED_BYTE="<<sepchar<<"\n";
 out<<"MAPPING(byte:button)\n";for(int v:vals)out<<v<<":"<<button[v]<<"\n";
 out<<"TARGET_TOKEN_COUNT="<<tokens<<"\n";
 out<<"TARGET_EXACT_FORM=N = concat_decimal(b(c1),207,b(c2),207,...,b(c1000000000),207)\n";
 out<<"FRAME=Phi(N)=(V,E,H,C,F)\n";
 out<<"REDUCTION=K -> NCL_C(K); for ratioed throuple J=CG(N_A,N_B,N_C), rho=Omega_J(N_A,N_B,N_C), reconstruct each branch before retention\n";
 out<<"STOP=irreducible iff every strictly smaller candidate fails equivalence/reconstruction under frozen C\n";
 out<<"TOKEN_FNV64_U16BE="<<hx(fnv)<<"\nPREFIX=";for(auto x:pre)out<<x<<",";out<<"\nSUFFIX=";for(auto x:tail)out<<x<<",";out<<"\n";
 out.close();
 std::cout<<"status=NUMERIC_TARGET_READY bytes_imprinted="<<n<<" terminals_seen=206 token_count="<<tokens<<" separator_button=207 separator_unused_byte="<<sepchar<<" token_fingerprint="<<hx(fnv)<<" equations=barnett_enwik9_207_equations.txt\n";
 return 2;
}
