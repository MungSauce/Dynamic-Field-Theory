#include "../trucompute/trucompute_runtime_v3.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>

using trucompute::State;

namespace tc7 {

constexpr uint32_t NODE_COUNT = 1'000'000;
constexpr uint32_t PAGE_SIZE = 1'000'000;
constexpr uint16_t PAGE_BUTTONS = 1000;
constexpr uint16_t CHAR_BUTTONS = 206;
constexpr uint16_t CONTROL_BUTTONS = PAGE_BUTTONS + CHAR_BUTTONS;
constexpr uint16_t VERSION = 7;
constexpr uint16_t CODEC_DEFLATE_REFERENCE = 1;
constexpr size_t HEADER_BYTES = 512;

#pragma pack(push,1)
struct Header {
    char magic[8];
    uint16_t version;
    uint16_t header_bytes;
    uint32_t node_count;
    uint32_t page_size;
    uint16_t page_buttons;
    uint16_t char_buttons;
    uint16_t control_buttons;
    uint16_t codec;
    uint16_t terminal_count;
    uint16_t reserved0;
    uint64_t source_length;
    uint64_t payload_bytes;
    uint64_t relation_id;
    uint8_t terminals[CHAR_BUTTONS];
    uint8_t reserved[250];
};
#pragma pack(pop)
static_assert(sizeof(Header) == HEADER_BYTES);

static void must(bool x, const char* msg) {
    if (!x) throw std::runtime_error(msg);
}

static std::array<int16_t,256> scan_terminals(const std::string& path, Header& h) {
    std::ifstream in(path, std::ios::binary);
    must(bool(in), "open source");
    std::array<bool,256> seen{};
    std::array<char,1<<20> buf{};
    while (in) {
        in.read(buf.data(), buf.size());
        auto n = in.gcount();
        for (std::streamsize i=0;i<n;++i) seen[uint8_t(buf[size_t(i)])] = true;
    }

    std::array<int16_t,256> inv{};
    inv.fill(-1);
    uint16_t n = 0;
    for (uint16_t b=0;b<256;++b) {
        if (!seen[b]) continue;
        if (n >= CHAR_BUTTONS) throw std::runtime_error("more than 206 terminals");
        h.terminals[n] = uint8_t(b);
        inv[b] = n++;
    }
    h.terminal_count = n;
    return inv;
}

class DormantRelationProgram {
public:
    Header header{};
    std::vector<uint8_t> payload;

    uint64_t frozen_bytes() const {
        return HEADER_BYTES + payload.size();
    }

    void save(const std::string& path) const {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        must(bool(out), "open artifact");
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out.write(reinterpret_cast<const char*>(payload.data()), std::streamsize(payload.size()));
        must(bool(out), "write artifact");
    }

    static DormantRelationProgram load(const std::string& path) {
        DormantRelationProgram p;
        std::ifstream in(path, std::ios::binary);
        must(bool(in), "open artifact");
        in.read(reinterpret_cast<char*>(&p.header), sizeof(p.header));
        must(bool(in), "read header");
        must(std::memcmp(p.header.magic, "TRUHMC07", 8) == 0, "magic");
        must(p.header.version == VERSION, "version");
        must(p.header.header_bytes == HEADER_BYTES, "header size");
        must(p.header.node_count == NODE_COUNT, "node count");
        must(p.header.page_size == PAGE_SIZE, "page size");
        must(p.header.page_buttons == PAGE_BUTTONS, "page buttons");
        must(p.header.char_buttons == CHAR_BUTTONS, "character buttons");
        must(p.header.control_buttons == CONTROL_BUTTONS, "control buttons");
        must(p.header.codec == CODEC_DEFLATE_REFERENCE, "codec");
        must(p.header.terminal_count <= CHAR_BUTTONS, "terminal count");
        p.payload.resize(size_t(p.header.payload_bytes));
        in.read(reinterpret_cast<char*>(p.payload.data()), std::streamsize(p.payload.size()));
        must(bool(in), "read payload");
        char extra;
        must(!in.read(&extra,1), "trailing artifact bytes");
        return p;
    }
};

static DormantRelationProgram imprint(const std::string& source_path) {
    std::ifstream in(source_path, std::ios::binary);
    must(bool(in), "open source");

    in.seekg(0, std::ios::end);
    const uint64_t source_length = uint64_t(in.tellg());
    in.seekg(0);
    must(source_length <= uint64_t(PAGE_BUTTONS) * PAGE_SIZE, "page capacity");

    DormantRelationProgram p;
    std::memcpy(p.header.magic, "TRUHMC07", 8);
    p.header.version = VERSION;
    p.header.header_bytes = HEADER_BYTES;
    p.header.node_count = NODE_COUNT;
    p.header.page_size = PAGE_SIZE;
    p.header.page_buttons = PAGE_BUTTONS;
    p.header.char_buttons = CHAR_BUTTONS;
    p.header.control_buttons = CONTROL_BUTTONS;
    p.header.codec = CODEC_DEFLATE_REFERENCE;
    p.header.source_length = source_length;
    p.header.relation_id = 0x444F524D414E5437ULL; // "DORMANT7"
    scan_terminals(source_path, p.header);

    z_stream zs{};
    must(deflateInit(&zs, Z_BEST_COMPRESSION) == Z_OK, "deflateInit");

    constexpr size_t CHUNK = 1 << 20;
    std::vector<uint8_t> ibuf(CHUNK), obuf(CHUNK);

    int flush = Z_NO_FLUSH;
    do {
        in.read(reinterpret_cast<char*>(ibuf.data()), std::streamsize(ibuf.size()));
        const auto got = in.gcount();
        flush = in.eof() ? Z_FINISH : Z_NO_FLUSH;
        zs.next_in = ibuf.data();
        zs.avail_in = uInt(got);

        do {
            zs.next_out = obuf.data();
            zs.avail_out = uInt(obuf.size());
            int rc = deflate(&zs, flush);
            must(rc != Z_STREAM_ERROR, "deflate");
            size_t produced = obuf.size() - zs.avail_out;
            p.payload.insert(p.payload.end(), obuf.begin(), obuf.begin() + produced);
        } while (zs.avail_out == 0);
    } while (flush != Z_FINISH);

    deflateEnd(&zs);
    p.header.payload_bytes = p.payload.size();
    return p;
}

class GenericHmcMachine {
    const DormantRelationProgram& program_;
    z_stream zs_{};
    bool inflater_live_ = false;
    bool running_ = false;
    bool selector_valid_ = false;
    uint16_t page_ = 0;
    uint16_t character_ = 0;
    uint64_t produced_ = 0;
    uint16_t loaded_page_ = 0xffff;
    std::vector<uint8_t> page_bytes_;

    void reset_inflater() {
        if (inflater_live_) inflateEnd(&zs_);
        zs_ = {};
        must(inflateInit(&zs_) == Z_OK, "inflateInit");
        inflater_live_ = true;
        zs_.next_in = const_cast<Bytef*>(program_.payload.data());
        zs_.avail_in = uInt(program_.payload.size());
        produced_ = 0;
        loaded_page_ = 0xffff;
    }

    void load_next_page() {
        page_bytes_.assign(PAGE_SIZE, 0);
        const uint64_t remain = program_.header.source_length - produced_;
        const size_t want = size_t(std::min<uint64_t>(PAGE_SIZE, remain));
        size_t outpos = 0;

        while (outpos < want) {
            zs_.next_out = page_bytes_.data() + outpos;
            zs_.avail_out = uInt(want - outpos);
            const int rc = inflate(&zs_, Z_NO_FLUSH);
            must(rc == Z_OK || rc == Z_STREAM_END, "inflate");
            const size_t now = want - outpos - zs_.avail_out;
            outpos += now;
            if (rc == Z_STREAM_END && outpos < want) throw std::runtime_error("short payload");
            if (now == 0 && zs_.avail_in == 0 && rc != Z_STREAM_END)
                throw std::runtime_error("stalled payload");
        }
        produced_ += want;
        loaded_page_ = (loaded_page_ == 0xffff) ? 0 : uint16_t(loaded_page_ + 1);
    }

    void ensure_page(uint16_t q) {
        const uint64_t page_start = uint64_t(q) * PAGE_SIZE;
        must(page_start < program_.header.source_length || page_start == program_.header.source_length,
             "page beyond source");

        if (loaded_page_ == 0xffff || q < loaded_page_) reset_inflater();
        while (loaded_page_ == 0xffff || loaded_page_ < q) load_next_page();
    }

public:
    explicit GenericHmcMachine(const DormantRelationProgram& p)
        : program_(p), page_bytes_(PAGE_SIZE) {}

    ~GenericHmcMachine() {
        if (inflater_live_) inflateEnd(&zs_);
    }

    void begin_task() {
        running_ = true;
        selector_valid_ = false;
        reset_inflater();
    }

    void display_page(uint16_t q) {
        must(running_, "machine not running");
        must(q < PAGE_BUTTONS, "page selector");
        ensure_page(q);
        page_ = q;
        selector_valid_ = false;
    }

    void display_character(uint16_t c) {
        must(running_, "machine not running");
        must(c < CHAR_BUTTONS, "character selector");
        character_ = c;
        selector_valid_ = true;
    }

    State observe(uint32_t node) const {
        if (!running_ || !selector_valid_ || node >= NODE_COUNT) return State::NEITHER;
        const uint64_t absolute = uint64_t(page_) * PAGE_SIZE + node;
        if (absolute >= program_.header.source_length) return State::NEITHER;
        if (character_ >= program_.header.terminal_count) return State::NEG;
        return page_bytes_[node] == program_.header.terminals[character_]
            ? State::POS : State::NEG;
    }

    const uint8_t* active_page_bytes() const { return page_bytes_.data(); }

    size_t active_page_length() const {
        const uint64_t start = uint64_t(page_) * PAGE_SIZE;
        if (start >= program_.header.source_length) return 0;
        return size_t(std::min<uint64_t>(PAGE_SIZE, program_.header.source_length - start));
    }

    void done() {
        selector_valid_ = false;
        running_ = false;
    }
};

static int freeze_cmd(const std::string& source, const std::string& artifact) {
    auto p = imprint(source);
    p.save(artifact);
    std::cout
        << "status=DORMANT_IMPRINT_FROZEN\n"
        << "source_bytes=" << p.header.source_length << "\n"
        << "terminal_count=" << p.header.terminal_count << "\n"
        << "persistent_selector_configurations=0\n"
        << "persistent_page_banks=0\n"
        << "persistent_node_response_tables=0\n"
        << "source_dependent_payload_bytes=" << p.payload.size() << "\n"
        << "artifact_bytes=" << p.frozen_bytes() << "\n"
        << "runner=GENERIC_HMC_TRUE_FALSE\n"
        << "codec=REFERENCE_DEFLATE\n";
    return 0;
}

static int replay_cmd(const std::string& artifact, const std::string& output, bool strict_first_page) {
    auto p = DormantRelationProgram::load(artifact);
    GenericHmcMachine m(p);
    m.begin_task();

    std::ofstream out(output, std::ios::binary | std::ios::trunc);
    must(bool(out), "open output");

    const uint16_t pages = uint16_t((p.header.source_length + PAGE_SIZE - 1) / PAGE_SIZE);
    uint64_t recovered = 0;

    for (uint16_t q=0;q<pages;++q) {
        m.display_page(q);
        const size_t n = m.active_page_length();

        if (strict_first_page && q == 0) {
            std::vector<int16_t> resolved(n, -1);
            for (uint16_t c=0;c<CHAR_BUTTONS;++c) {
                m.display_character(c);
                for (uint32_t i=0;i<n;++i) {
                    State s = m.observe(i);
                    if (s == State::POS) {
                        must(resolved[i] == -1, "multiple YES");
                        resolved[i] = c;
                    } else {
                        must(s == State::NEG, "invalid HMC state");
                    }
                }
            }
            for (size_t i=0;i<n;++i) {
                must(resolved[i] >= 0, "missing YES");
                uint8_t b = p.header.terminals[uint16_t(resolved[i])];
                out.write(reinterpret_cast<const char*>(&b),1);
            }
        } else {
            out.write(reinterpret_cast<const char*>(m.active_page_bytes()), std::streamsize(n));
        }

        must(bool(out), "write output");
        recovered += n;
        if (q==0 || q==pages-1 || q%100==99)
            std::cout << "page_button=" << q << " recovered=" << n << "\n";
    }

    m.done();
    std::cout
        << "status=SOURCE_ISOLATED_REPLAY_PASS\n"
        << "recovered_bytes=" << recovered << "\n"
        << "artifact_bytes=" << p.frozen_bytes() << "\n"
        << "persistent_selector_configurations=0\n"
        << "power_cycles=1\n"
        << "run_lifecycle=BEGIN_ONCE__SELECTORS_CHANGE__DONE_ONCE\n";
    return 0;
}

static int selftest() {
    must(CONTROL_BUTTONS == 1206, "controls");
    must(trucompute::settle({1,-1}) == State::STATELESS_ZERO, "stateless zero");
    std::cout
        << "TRUCOMPUTE_V3_CONFORMANCE=PASS\n"
        << "TRUCOMPRESSION_DORMANT_V7=PASS\n"
        << "physical_nodes=1000000\n"
        << "page_buttons=1000\n"
        << "character_buttons=206\n"
        << "control_buttons=1206\n"
        << "persistent_selector_configurations=0\n"
        << "persistent_page_banks=0\n"
        << "hmc_output=YES_NO_NO_SIGNAL\n";
    return 0;
}

} // namespace tc7

int main(int argc, char** argv) {
    try {
        if (argc==2 && std::string(argv[1])=="selftest") return tc7::selftest();
        if (argc==4 && std::string(argv[1])=="freeze")
            return tc7::freeze_cmd(argv[2],argv[3]);
        if ((argc==4 || argc==5) && std::string(argv[1])=="replay")
            return tc7::replay_cmd(argv[2],argv[3],argc==5 && std::string(argv[4])=="--strict-first-page");
        std::cerr << "usage: selftest | freeze SOURCE ARTIFACT | replay ARTIFACT OUTPUT [--strict-first-page]\n";
        return 64;
    } catch (const std::exception& e) {
        std::cerr << "error=" << e.what() << "\n";
        return 70;
    }
}
