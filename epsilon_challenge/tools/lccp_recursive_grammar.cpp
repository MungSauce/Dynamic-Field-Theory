#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

static void put_u32(std::ostream& o,uint32_t v){for(int k=0;k<4;k++)o.put(char((v>>(8*k))&255));}
static uint32_t get_u32(std::istream& i){uint32_t v=0;for(int k=0;k<4;k++){int c=i.get();if(c<0)throw std::runtime_error("short u32");v|=uint32_t(c)<<(8*k);}return v;}
static void put_u64(std::ostream& o,uint64_t v){for(int k=0;k<8;k++)o.put(char((v>>(8*k))&255));}
static uint64_t get_u64(std::istream& i){uint64_t v=0;for(int k=0;k<8;k++){int c=i.get();if(c<0)throw std::runtime_error("short u64");v|=uint64_t(c)<<(8*k);}return v;}
static void put_var(std::ostream& o,uint64_t v){while(v>=128){o.put(char((v&127)|128));v>>=7;}o.put(char(v));}
static uint64_t get_var(std::istream& i){uint64_t v=0;int sh=0;for(;;){int c=i.get();if(c<0)throw std::runtime_error("short varint");v|=uint64_t(c&127)<<sh;if(!(c&128))return v;sh+=7;if(sh>63)throw std::runtime_error("varint overflow");}}
static uint64_t fsize(const std::string& p){std::ifstream in(p,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("size open");return uint64_t(in.tellg());}
static void copy_file_to(std::ostream& out,const std::string& p){std::ifstream in(p,std::ios::binary);if(!in)throw std::runtime_error("open copy source");std::vector<char>b(1<<20);while(in){in.read(b.data(),b.size());size_t n=(size_t)in.gcount();if(n)out.write(b.data(),n);}}

struct BitWriter{
    std::ofstream out; uint8_t cur=0; int used=0; uint64_t bits=0,bytes=0;
    explicit BitWriter(const std::string&p):out(p,std::ios::binary){if(!out)throw std::runtime_error("open bit output");}
    void bit(int x){cur|=uint8_t((x&1)<<(7-used));used++;bits++;if(used==8){out.put(char(cur));bytes++;cur=0;used=0;}}
    void bits_msb(uint64_t v,int n){for(int k=n-1;k>=0;k--)bit((v>>k)&1);}
    void finish(){if(used){out.put(char(cur));bytes++;cur=0;used=0;}out.flush();}
};
struct BitReader{
    std::ifstream in; uint8_t cur=0; int used=8; uint64_t bits_left=0;
    BitReader(const std::string&p,uint64_t off,uint64_t bits):in(p,std::ios::binary),bits_left(bits){if(!in)throw std::runtime_error("open bit input");in.seekg((std::streamoff)off);if(!in)throw std::runtime_error("seek bits");}
    int bit(){if(!bits_left)throw std::runtime_error("bitstream exhausted");if(used==8){int c=in.get();if(c<0)throw std::runtime_error("short bits");cur=uint8_t(c);used=0;}int x=(cur>>(7-used))&1;used++;bits_left--;return x;}
};

struct HNode{uint64_t f=0;int l=-1,r=-1,s=-1;};
struct HQ{uint64_t f;int serial;int idx;};
struct HCmp{bool operator()(const HQ&a,const HQ&b)const{return a.f!=b.f?a.f>b.f:a.serial>b.serial;}};
static std::vector<uint8_t> huffman_lengths(const std::vector<uint64_t>& freq){
    std::vector<uint8_t> len(freq.size(),0);std::priority_queue<HQ,std::vector<HQ>,HCmp>q;std::vector<HNode>nodes;
    for(int s=0;s<(int)freq.size();s++)if(freq[s]){int i=(int)nodes.size();nodes.push_back({freq[s],-1,-1,s});q.push({freq[s],s,i});}
    if(q.empty())return len;if(q.size()==1){len[nodes[q.top().idx].s]=1;return len;}
    int serial=(int)freq.size();
    while(q.size()>1){auto a=q.top();q.pop();auto b=q.top();q.pop();int i=(int)nodes.size();nodes.push_back({a.f+b.f,a.idx,b.idx,-1});q.push({a.f+b.f,serial++,i});}
    std::vector<std::pair<int,int>>st{{q.top().idx,0}};
    while(!st.empty()){auto [i,d]=st.back();st.pop_back();if(nodes[i].s>=0){if(d<=0||d>63)throw std::runtime_error("Huffman code exceeds 63 bits");len[nodes[i].s]=uint8_t(d);}else{st.push_back({nodes[i].r,d+1});st.push_back({nodes[i].l,d+1});}}
    return len;
}
struct Code{uint64_t code=0;uint8_t len=0;};
static std::vector<Code> canonical_codes(const std::vector<uint8_t>& lens){
    std::vector<std::pair<int,int>>ord;for(int s=0;s<(int)lens.size();s++)if(lens[s])ord.push_back({lens[s],s});std::sort(ord.begin(),ord.end());
    std::vector<Code>out(lens.size());uint64_t code=0;int prev=0;for(auto [l,s]:ord){if(l>prev)code<<=(l-prev);out[s]={code,uint8_t(l)};code++;prev=l;}return out;
}
struct DNode{int ch[2]={-1,-1};int sym=-1;};
static std::vector<DNode> decode_tree(const std::vector<Code>& codes){
    std::vector<DNode>t(1);for(int s=0;s<(int)codes.size();s++)if(codes[s].len){int n=0;for(int k=codes[s].len-1;k>=0;k--){int b=(codes[s].code>>k)&1;if(t[n].ch[b]<0){t[n].ch[b]=(int)t.size();t.push_back({});}n=t[n].ch[b];}if(t[n].sym>=0)throw std::runtime_error("duplicate code");t[n].sym=s;}return t;
}
static int decode_symbol(BitReader& br,const std::vector<DNode>&t){int n=0;while(t[n].sym<0){int b=br.bit();n=t[n].ch[b];if(n<0)throw std::runtime_error("invalid huffman stream");}return t[n].sym;}

struct Rule{uint32_t a=0,b=0;};
static uint64_t pair_key(uint32_t a,uint32_t b){return (uint64_t(a)<<32)|b;}

static std::vector<uint8_t> read_all(const std::string&p){
    std::ifstream in(p,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("open source");uint64_t n=(uint64_t)in.tellg();in.seekg(0);std::vector<uint8_t>b((size_t)n);if(n){in.read((char*)b.data(),n);if((uint64_t)in.gcount()!=n)throw std::runtime_error("short source");}return b;
}

static void write_raw(const std::string&src,const std::string&dst,uint64_t n){
    std::ofstream out(dst,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("open raw output");out.write("RGD1",4);out.put(1);out.put(0);put_u64(out,n);copy_file_to(out,src);
}

static void encode(const std::string&src,const std::string&dst,uint32_t max_rules,uint32_t batch_pairs,uint32_t max_rounds){
    auto bytes=read_all(src);uint64_t total=bytes.size();
    std::vector<uint32_t>seq;seq.reserve(bytes.size());for(uint8_t b:bytes)seq.push_back(b);bytes.clear();bytes.shrink_to_fit();
    std::vector<Rule>rules;rules.reserve(max_rules);
    uint64_t rounds=0,total_replacements=0;

    for(uint32_t round=0;round<max_rounds && rules.size()<max_rules && seq.size()>1;round++){
        std::unordered_map<uint64_t,uint32_t>cnt;
        size_t reserve_n=std::min<size_t>(seq.size()/2+1,2'000'000);cnt.reserve(reserve_n);
        for(size_t i=0;i+1<seq.size();i++){
            uint64_t k=pair_key(seq[i],seq[i+1]);auto it=cnt.find(k);if(it==cnt.end())cnt.emplace(k,1);else if(it->second!=0xffffffffu)it->second++;
        }
        struct Cand{uint64_t key;uint32_t count;};std::vector<Cand>cand;cand.reserve(std::min<size_t>(cnt.size(),batch_pairs*8ull));
        for(auto&kv:cnt)if(kv.second>=8)cand.push_back({kv.first,kv.second});
        if(cand.empty())break;
        size_t want=std::min<size_t>(batch_pairs,std::min<size_t>(cand.size(),max_rules-rules.size()));
        if(cand.size()>want){std::nth_element(cand.begin(),cand.begin()+want,cand.end(),[](const Cand&a,const Cand&b){return a.count>b.count;});cand.resize(want);}
        std::sort(cand.begin(),cand.end(),[](const Cand&a,const Cand&b){return a.count!=b.count?a.count>b.count:a.key<b.key;});

        std::unordered_map<uint64_t,uint32_t>selected;selected.reserve(cand.size()*2+1);
        uint32_t base=256+(uint32_t)rules.size();
        for(uint32_t i=0;i<cand.size();i++)selected[cand[i].key]=base+i;

        std::vector<uint32_t>next;next.reserve(seq.size());std::vector<uint32_t>uses(cand.size(),0);
        for(size_t i=0;i<seq.size();){
            if(i+1<seq.size()){
                auto it=selected.find(pair_key(seq[i],seq[i+1]));
                if(it!=selected.end()){
                    uint32_t provisional=it->second;uses[provisional-base]++;next.push_back(provisional);i+=2;continue;
                }
            }
            next.push_back(seq[i++]);
        }

        std::vector<uint32_t>remap(cand.size(),std::numeric_limits<uint32_t>::max());
        uint32_t kept=0;
        for(uint32_t i=0;i<cand.size();i++)if(uses[i]>=2)remap[i]=256+(uint32_t)rules.size()+kept++;
        if(!kept)break;

        next.clear();next.reserve(seq.size());uint64_t replacements=0;
        for(size_t i=0;i<seq.size();){
            bool done=false;
            if(i+1<seq.size()){
                auto it=selected.find(pair_key(seq[i],seq[i+1]));
                if(it!=selected.end()){
                    uint32_t old=it->second-base;
                    if(remap[old]!=std::numeric_limits<uint32_t>::max()){
                        next.push_back(remap[old]);i+=2;replacements++;done=true;
                    }
                }
            }
            if(!done)next.push_back(seq[i++]);
        }
        for(uint32_t i=0;i<cand.size();i++)if(remap[i]!=std::numeric_limits<uint32_t>::max()){
            rules.push_back({uint32_t(cand[i].key>>32),uint32_t(cand[i].key)});
        }
        if(replacements<2)break;
        total_replacements+=replacements;rounds++;seq.swap(next);
    }

    uint32_t alphabet=256+(uint32_t)rules.size();
    std::vector<uint64_t>freq(alphabet,0);for(uint32_t s:seq){if(s>=alphabet)throw std::runtime_error("symbol range");freq[s]++;}
    std::vector<uint8_t>lens;std::vector<Code>codes;bool huff_ok=true;
    try{lens=huffman_lengths(freq);codes=canonical_codes(lens);}catch(const std::exception&){huff_ok=false;}

    std::string comp=dst+".cmp.tmp",bitp=dst+".bits.tmp";
    uint64_t comp_size=std::numeric_limits<uint64_t>::max(),stream_bits=0;
    if(huff_ok){
        BitWriter bw(bitp);for(uint32_t s:seq){auto c=codes[s];if(!c.len)throw std::runtime_error("missing code");bw.bits_msb(c.code,c.len);}bw.finish();stream_bits=bw.bits;
        std::ofstream out(comp,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("open grammar output");
        out.write("RGD1",4);out.put(1);out.put(1);put_u64(out,total);put_u32(out,(uint32_t)rules.size());
        for(auto&r:rules){put_var(out,r.a);put_var(out,r.b);}put_u32(out,alphabet);
        uint32_t used=0;for(uint8_t l:lens)if(l)used++;put_u32(out,used);for(uint32_t s=0;s<lens.size();s++)if(lens[s]){put_var(out,s);out.put(char(lens[s]));}
        put_u64(out,seq.size());put_u64(out,stream_bits);put_u64(out,bw.bytes);copy_file_to(out,bitp);out.flush();comp_size=fsize(comp);
    }
    uint64_t raw_size=4+1+1+8+total;
    if(huff_ok && comp_size<raw_size){std::remove(dst.c_str());if(std::rename(comp.c_str(),dst.c_str())!=0)throw std::runtime_error("rename grammar");std::cerr<<"RGD_BRANCH=GRAMMAR\n";}
    else{write_raw(src,dst,total);std::remove(comp.c_str());std::cerr<<"RGD_BRANCH=RAW\n";}
    std::remove(bitp.c_str());

    std::cerr<<"RGD_SOURCE_BYTES="<<total<<"\n";
    std::cerr<<"RGD_RULES="<<rules.size()<<"\n";
    std::cerr<<"RGD_ROUNDS="<<rounds<<"\n";
    std::cerr<<"RGD_TOPLEVEL_SYMBOLS="<<seq.size()<<"\n";
    std::cerr<<"RGD_REPLACEMENTS="<<total_replacements<<"\n";
    std::cerr<<"RGD_STREAM_BITS="<<stream_bits<<"\n";
    if(huff_ok)std::cerr<<"RGD_GRAMMAR_CANDIDATE_BYTES="<<comp_size<<"\n";
    std::cerr<<"RGD_ARTIFACT_BYTES="<<fsize(dst)<<"\n";
}

static void emit_symbol(uint32_t root,const std::vector<Rule>&rules,std::ofstream&out,uint64_t&written,std::vector<uint32_t>&stack){
    stack.clear();stack.push_back(root);
    while(!stack.empty()){
        uint32_t s=stack.back();stack.pop_back();
        if(s<256){out.put(char(uint8_t(s)));written++;continue;}
        uint32_t ri=s-256;if(ri>=rules.size())throw std::runtime_error("rule reference range");const Rule&r=rules[ri];
        stack.push_back(r.b);stack.push_back(r.a);
        if(stack.size()>50'000'000)throw std::runtime_error("expansion stack runaway");
    }
}

static void decode(const std::string&src,const std::string&dst){
    std::ifstream in(src,std::ios::binary);if(!in)throw std::runtime_error("open carrier");char m[4];in.read(m,4);if(in.gcount()!=4||std::string(m,4)!="RGD1")throw std::runtime_error("magic");
    if(in.get()!=1)throw std::runtime_error("version");int mode=in.get();if(mode<0)throw std::runtime_error("mode");uint64_t total=get_u64(in);
    std::ofstream out(dst,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("open decoded output");
    if(mode==0){std::vector<char>b(1<<20);uint64_t rem=total;while(rem){size_t n=(size_t)std::min<uint64_t>(rem,b.size());in.read(b.data(),n);if((size_t)in.gcount()!=n)throw std::runtime_error("short raw");out.write(b.data(),n);rem-=n;}if(in.peek()!=EOF)throw std::runtime_error("trailing raw");std::cerr<<"RGD_RECOVERED_BYTES="<<total<<"\n";return;}
    if(mode!=1)throw std::runtime_error("unknown mode");
    uint32_t R=get_u32(in);std::vector<Rule>rules(R);for(uint32_t i=0;i<R;i++){rules[i].a=(uint32_t)get_var(in);rules[i].b=(uint32_t)get_var(in);uint32_t lim=256+i;if(rules[i].a>=lim||rules[i].b>=lim)throw std::runtime_error("non-causal grammar rule");}
    uint32_t alphabet=get_u32(in);if(alphabet!=256+R)throw std::runtime_error("alphabet mismatch");uint32_t used=get_u32(in);std::vector<uint8_t>lens(alphabet,0);
    for(uint32_t i=0;i<used;i++){uint64_t s=get_var(in);int l=in.get();if(s>=alphabet||l<=0||l>63||lens[s])throw std::runtime_error("bad code table");lens[s]=uint8_t(l);}
    auto codes=canonical_codes(lens);auto tree=decode_tree(codes);uint64_t nsyms=get_u64(in),bits=get_u64(in),bytes=get_u64(in);uint64_t off=(uint64_t)in.tellg();if(off+bytes!=fsize(src))throw std::runtime_error("carrier size mismatch");
    BitReader br(src,off,bits);uint64_t written=0;std::vector<uint32_t>stack;stack.reserve(1024);
    for(uint64_t i=0;i<nsyms;i++){int s=decode_symbol(br,tree);emit_symbol((uint32_t)s,rules,out,written,stack);if(written>total)throw std::runtime_error("decoded beyond source size");}
    if(written!=total)throw std::runtime_error("decoded size mismatch");std::cerr<<"RGD_RECOVERED_BYTES="<<written<<"\n";
}

int main(int argc,char**argv){
    try{
        if(argc<4){std::cerr<<"usage: lccp_recursive_grammar encode input output.rgd [max_rules=32768] [batch_pairs=256] [rounds=32]\n       lccp_recursive_grammar decode input.rgd output\n";return 2;}
        std::string a=argv[1];if(a=="encode"||a=="e"){
            uint32_t mr=argc>4?(uint32_t)std::stoul(argv[4]):32768;uint32_t bp=argc>5?(uint32_t)std::stoul(argv[5]):256;uint32_t rd=argc>6?(uint32_t)std::stoul(argv[6]):32;
            if(!mr||mr>1'000'000||!bp||bp>16384||!rd||rd>256)throw std::runtime_error("parameter range");encode(argv[2],argv[3],mr,bp,rd);
        }else if(a=="decode"||a=="d")decode(argv[2],argv[3]);else throw std::runtime_error("unknown action");
    }catch(const std::exception&e){std::cerr<<"ERROR "<<e.what()<<"\n";return 1;}return 0;
}
