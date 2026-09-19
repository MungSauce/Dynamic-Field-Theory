#include "../trucompute/trucompute_runtime_v3.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using trucompute::State;

namespace trucompression_direct {

static constexpr uint32_t NODE_COUNT = 1'000'000;
static constexpr uint32_t PAGE_SIZE = 1'000'000;
static constexpr uint16_t PAGE_BUTTONS = 1000;
static constexpr uint16_t CHAR_BUTTONS = 206;
static constexpr uint16_t CONTROL_BUTTONS = PAGE_BUTTONS + CHAR_BUTTONS;
static constexpr uint16_t VERSION = 4;
static constexpr size_t MACHINE_BYTES = NODE_COUNT / 8;

static uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// A node has NO relation to another node.
// Its response is a direct transfer from the displayed selector condition:
//     (page button, character button) -> node response.
//
// The one retained bit chooses between two fixed, source-independent
// transfer branches. This is intentionally the minimum-width baseline.
static uint8_t candidate(uint32_t node, uint16_t page, uint8_t branch) {
    uint64_t x =
        uint64_t(node) * 0xD6E8FEB86659FD93ULL
        ^ uint64_t(page + 1) * 0xA5A35625AA5A3563ULL
        ^ uint64_t(branch + 1) * 0x9E3779B185EBCA87ULL;
    uint8_t a = uint8_t(mix64(x) % CHAR_BUTTONS);
    if (branch == 0) return a;

    uint8_t b = uint8_t(mix64(x ^ 0x243F6A8885A308D3ULL) % CHAR_BUTTONS);
    if (b == candidate(node, page, 0))
        b = uint8_t((b + 1) % CHAR_BUTTONS);
    return b;
}

struct Machine {
    std::vector<uint8_t> program_bits;
    uint64_t source_length = 0;

    Machine() : program_bits(MACHINE_BYTES, 0) {}

    uint8_t program(uint32_t node) const {
        return uint8_t((program_bits[node >> 3] >> (node & 7)) & 1u);
    }

    void set_program(uint32_t node, uint8_t value) {
        uint8_t &x = program_bits[node >> 3];
        const uint8_t mask = uint8_t(1u << (node & 7));
        if (value) x |= mask;
        else x &= uint8_t(~mask);
    }

    State press(uint16_t page, uint16_t character, uint32_t node) const {
        if (page >= PAGE_BUTTONS || character >= CHAR_BUTTONS || node >= NODE_COUNT)
            return State::NEITHER;

        const uint8_t expressed = candidate(node, page, program(node));
        return expressed == character ? State::POS : State::NEG;
    }

    uint8_t decoded_terminal(uint16_t page, uint32_t node) const {
        return candidate(node, page, program(node));
    }
};

#pragma pack(push, 1)
struct Header {
    char magic[8];
    uint16_t version;
    uint16_t header_bytes;
    uint32_t node_count;
    uint32_t page_size;
    uint16_t page_buttons;
    uint16_t char_buttons;
    uint16_t control_buttons;
    uint16_t program_bits_per_node;
    uint64_t source_length;
    uint64_t machine_bytes;
    uint64_t transfer_id;
    uint8_t reserved[12];
};
#pragma pack(pop)
static_assert(sizeof(Header) == 64);

static std::vector<uint8_t> read_all(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("open input failed");
    return {std::istreambuf_iterator<char>(f), {}};
}

static void save(const Machine &m, const std::string &path) {
    Header h{};
    std::memcpy(h.magic, "TRUDIR04", 8);
    h.version = VERSION;
    h.header_bytes = sizeof(Header);
    h.node_count = NODE_COUNT;
    h.page_size = PAGE_SIZE;
    h.page_buttons = PAGE_BUTTONS;
    h.char_buttons = CHAR_BUTTONS;
    h.control_buttons = CONTROL_BUTTONS;
    h.program_bits_per_node = 1;
    h.source_length = m.source_length;
    h.machine_bytes = m.program_bits.size();
    h.transfer_id = 0x5452554449523034ULL;

    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("open artifact failed");
    f.write(reinterpret_cast<const char*>(&h), sizeof(h));
    f.write(reinterpret_cast<const char*>(m.program_bits.data()),
            std::streamsize(m.program_bits.size()));
}

static Machine load(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("open artifact failed");

    Header h{};
    f.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (std::memcmp(h.magic, "TRUDIR04", 8)
        || h.version != VERSION
        || h.node_count != NODE_COUNT
        || h.page_size != PAGE_SIZE
        || h.page_buttons != PAGE_BUTTONS
        || h.char_buttons != CHAR_BUTTONS
        || h.control_buttons != CONTROL_BUTTONS
        || h.program_bits_per_node != 1
        || h.machine_bytes != MACHINE_BYTES)
        throw std::runtime_error("artifact incompatible");

    Machine m;
    m.source_length = h.source_length;
    f.read(reinterpret_cast<char*>(m.program_bits.data()),
           std::streamsize(m.program_bits.size()));
    if (!f) throw std::runtime_error("artifact truncated");

    char extra;
    if (f.read(&extra, 1))
        throw std::runtime_error("artifact has trailing data");
    return m;
}

static int blank(const std::string &path) {
    Machine m;
    save(m, path);
    std::cout
        << "status=BLANK_FROZEN\n"
        << "physical_nodes=" << NODE_COUNT << "\n"
        << "page_buttons=" << PAGE_BUTTONS << "\n"
        << "character_buttons=" << CHAR_BUTTONS << "\n"
        << "control_buttons=" << CONTROL_BUTTONS << "\n"
        << "program_bits_per_node=1\n"
        << "machine_payload_bytes=" << MACHINE_BYTES << "\n"
        << "artifact_bytes=" << (sizeof(Header) + MACHINE_BYTES) << "\n";
    return 0;
}

static int imprint(const std::string &source_path, const std::string &artifact_path) {
    const auto source = read_all(source_path);
    if (source.size() > uint64_t(PAGE_BUTTONS) * PAGE_SIZE) {
        std::cout << "status=PAGE_CAPACITY_EXCEEDED\n";
        return 2;
    }

    // Two-bit temporary possibility mask per node, write-time only:
    // bit0 = transfer branch 0 remains possible
    // bit1 = transfer branch 1 remains possible
    std::vector<uint8_t> allowed(NODE_COUNT, 0b11);

    for (uint64_t t = 0; t < source.size(); ++t) {
        const uint8_t terminal = source[t];
        if (terminal >= CHAR_BUTTONS) {
            std::cout << "status=TERMINAL_OUT_OF_RANGE\nbyte=" << t
                      << "\nterminal=" << unsigned(terminal) << "\n";
            return 2;
        }

        const uint16_t page = uint16_t(t / PAGE_SIZE);
        const uint32_t node = uint32_t(t % PAGE_SIZE);

        uint8_t matches = 0;
        if (candidate(node, page, 0) == terminal) matches |= 0b01;
        if (candidate(node, page, 1) == terminal) matches |= 0b10;

        allowed[node] &= matches;
        if (allowed[node] == 0) {
            std::cout
                << "status=IMPRINT_CONTRADICTION\n"
                << "byte=" << t << "\n"
                << "page=" << page << "\n"
                << "node=" << node << "\n"
                << "terminal=" << unsigned(terminal) << "\n"
                << "candidate0=" << unsigned(candidate(node,page,0)) << "\n"
                << "candidate1=" << unsigned(candidate(node,page,1)) << "\n"
                << "node_to_node_relations=0\n"
                << "machine_growth=0\n";
            return 2;
        }
    }

    Machine m;
    m.source_length = source.size();
    for (uint32_t node=0; node<NODE_COUNT; ++node) {
        if (allowed[node] & 0b01) m.set_program(node,0);
        else if (allowed[node] & 0b10) m.set_program(node,1);
        // Nodes never touched by the source remain canonical branch 0.
    }

    for (uint64_t t=0; t<source.size(); ++t) {
        const uint16_t page=uint16_t(t/PAGE_SIZE);
        const uint32_t node=uint32_t(t%PAGE_SIZE);
        if (m.decoded_terminal(page,node) != source[t]) {
            std::cout << "status=FREEZE_VERIFY_FAIL\nbyte=" << t << "\n";
            return 3;
        }
    }

    save(m,artifact_path);
    std::cout
        << "status=FROZEN\n"
        << "source_bytes=" << source.size() << "\n"
        << "physical_nodes=" << NODE_COUNT << "\n"
        << "page_buttons=" << PAGE_BUTTONS << "\n"
        << "character_buttons=" << CHAR_BUTTONS << "\n"
        << "control_buttons=" << CONTROL_BUTTONS << "\n"
        << "node_to_node_relations=0\n"
        << "program_bits_per_node=1\n"
        << "artifact_bytes=" << (sizeof(Header)+MACHINE_BYTES) << "\n";
    return 0;
}

static int replay(const std::string &artifact_path,
                  const std::string &output_path,
                  bool strict_buttons) {
    const Machine m=load(artifact_path);
    std::ofstream out(output_path,std::ios::binary);
    if(!out) throw std::runtime_error("open output failed");

    uint64_t written=0;
    const uint64_t pages=(m.source_length+PAGE_SIZE-1)/PAGE_SIZE;

    for(uint16_t page=0; page<pages; ++page){
        const uint32_t count=uint32_t(std::min<uint64_t>(
            PAGE_SIZE,m.source_length-written));

        if(strict_buttons){
            std::vector<int16_t> resolved(count,-1);

            // Exactly one page selector is displayed; character selectors
            // are then pressed in their fixed 0..205 order.
            for(uint16_t character=0; character<CHAR_BUTTONS; ++character){
                for(uint32_t node=0; node<count; ++node){
                    const State s=m.press(page,character,node);
                    if(s==State::POS){
                        if(resolved[node]!=-1){
                            std::cout<<"status=AMBIGUOUS\npage="<<page
                                     <<"\nnode="<<node<<"\n";
                            return 4;
                        }
                        resolved[node]=int16_t(character);
                    } else if(s!=State::NEG){
                        std::cout<<"status=NONBINARY_PAYLOAD_RESPONSE\npage="<<page
                                 <<"\nnode="<<node<<"\n";
                        return 5;
                    }
                }
            }

            for(uint32_t node=0; node<count; ++node){
                if(resolved[node]<0){
                    std::cout<<"status=UNRESOLVED\npage="<<page
                             <<"\nnode="<<node<<"\n";
                    return 6;
                }
                const uint8_t terminal=uint8_t(resolved[node]);
                out.write(reinterpret_cast<const char*>(&terminal),1);
            }
        } else {
            for(uint32_t node=0; node<count; ++node){
                const uint8_t terminal=m.decoded_terminal(page,node);
                out.write(reinterpret_cast<const char*>(&terminal),1);
            }
        }

        written+=count;
        std::cout<<"page_button="<<page
                 <<" character_sweep=0..205"
                 <<" recovered="<<count<<"\n";
    }

    std::cout<<"status=REPLAY_PASS\nrecovered_bytes="<<written
             <<"\ncontrol_buttons="<<CONTROL_BUTTONS<<"\n";
    return 0;
}

// Generates a corpus that is exactly representable by this one-bit baseline.
// This is a mechanics test, not evidence that arbitrary documents fit.
static int synth(uint64_t length,const std::string &out_path){
    if(length>uint64_t(PAGE_BUTTONS)*PAGE_SIZE) return 2;
    std::ofstream out(out_path,std::ios::binary);
    if(!out) return 3;

    for(uint64_t t=0;t<length;++t){
        const uint16_t page=uint16_t(t/PAGE_SIZE);
        const uint32_t node=uint32_t(t%PAGE_SIZE);
        const uint8_t branch=uint8_t(node&1u);
        const uint8_t terminal=candidate(node,page,branch);
        out.write(reinterpret_cast<const char*>(&terminal),1);
    }
    std::cout<<"status=SYNTH_PASS\nbytes="<<length<<"\n";
    return 0;
}

static int selftest(){
    using namespace trucompute;

    if(CONTROL_BUTTONS!=1206) return 1;
    if(PAGE_BUTTONS!=1000) return 2;
    if(CHAR_BUTTONS!=206) return 3;

    // TruCompute translation remains unchanged.
    if(settle({+1,-1})!=State::STATELESS_ZERO) return 4;
    if(!stateless(settle({+1,-1}))) return 5;

    bool rejected=false;
    try { (void)net(State::NEITHER); }
    catch(const std::runtime_error&){ rejected=true; }
    if(!rejected) return 6;

    Machine m;
    // One displayed page+character selector broadcasts to every node.
    for(uint32_t node=0;node<100;++node){
        int positives=0;
        for(uint16_t c=0;c<CHAR_BUTTONS;++c)
            if(m.press(0,c,node)==State::POS) ++positives;
        if(positives!=1) return 7;
    }

    std::cout
        <<"TRUCOMPUTE_V3_CONFORMANCE=PASS\n"
        <<"TRUCOMPRESSION_DIRECT_SELECTOR=PASS\n"
        <<"physical_nodes="<<NODE_COUNT<<"\n"
        <<"page_buttons="<<PAGE_BUTTONS<<"\n"
        <<"character_buttons="<<CHAR_BUTTONS<<"\n"
        <<"control_buttons="<<CONTROL_BUTTONS<<"\n"
        <<"node_to_node_relations=0\n"
        <<"program_bits_per_node=1\n";
    return 0;
}

} // namespace trucompression_direct

int main(int argc,char**argv){
    using namespace trucompression_direct;
    try{
        if(argc==2 && std::string(argv[1])=="selftest") return selftest();
        if(argc==3 && std::string(argv[1])=="blank") return blank(argv[2]);
        if(argc==4 && std::string(argv[1])=="imprint") return imprint(argv[2],argv[3]);
        if((argc==4||argc==5) && std::string(argv[1])=="replay")
            return replay(argv[2],argv[3],argc==5 && std::string(argv[4])=="--strict-buttons");
        if(argc==4 && std::string(argv[1])=="synth")
            return synth(std::stoull(argv[2]),argv[3]);

        std::cerr
            <<"usage:\n"
            <<"  trucompression_direct_v4 selftest\n"
            <<"  trucompression_direct_v4 blank ARTIFACT\n"
            <<"  trucompression_direct_v4 imprint SOURCE ARTIFACT\n"
            <<"  trucompression_direct_v4 replay ARTIFACT OUTPUT [--strict-buttons]\n"
            <<"  trucompression_direct_v4 synth LENGTH OUTPUT\n";
        return 64;
    }catch(const std::exception&e){
        std::cerr<<"error="<<e.what()<<"\n";
        return 70;
    }
}
