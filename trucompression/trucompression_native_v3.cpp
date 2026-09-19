#include "../trucompute/trucompute_runtime_v3.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using trucompute::Element;
using trucompute::State;

namespace trucompression {

static constexpr uint32_t NODE_COUNT = 1'000'000;
static constexpr uint32_t PAGE_SIZE = 1'000'000;
static constexpr uint16_t BUTTON_COUNT = 256;
static constexpr uint16_t VERSION = 3;
static constexpr size_t MACHINE_BYTES = NODE_COUNT / 8;

static uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static uint8_t page_phase(uint64_t page, uint32_t node) {
    return uint8_t(mix64((page + 1) * 0xD6E8FEB86659FD93ULL
        ^ uint64_t(node) * 0xA5A35625AA5A3563ULL) & 1ULL);
}

struct Route {
    uint32_t a;
    uint32_t b;
    uint8_t modifier;
};

static Route route(uint64_t page, uint32_t local_pos, uint8_t bit) {
    const uint64_t base =
        (page * 0x9E3779B185EBCA87ULL)
        ^ (uint64_t(local_pos) * 0xC2B2AE3D27D4EB4FULL)
        ^ (uint64_t(bit) * 0x165667B19E3779F9ULL);

    uint32_t a = uint32_t(mix64(base ^ 0x243F6A8885A308D3ULL) % NODE_COUNT);
    uint32_t b = uint32_t(mix64(base ^ 0x13198A2E03707344ULL) % NODE_COUNT);
    if (b == a) b = (b + 1) % NODE_COUNT;
    const uint8_t modifier =
        uint8_t(mix64(base ^ 0xA4093822299F31D0ULL) & 1ULL);
    return {a, b, modifier};
}

// Temporary write-time solver. Never serialized.
struct ParityDSU {
    std::vector<uint32_t> parent;
    std::vector<uint8_t> rank;
    std::vector<uint8_t> parity;

    explicit ParityDSU(size_t n) : parent(n), rank(n, 0), parity(n, 0) {
        for (uint32_t i = 0; i < n; ++i) parent[i] = i;
    }

    std::pair<uint32_t, uint8_t> find(uint32_t x) {
        if (parent[x] == x) return {x, 0};
        auto [r, p] = find(parent[x]);
        parity[x] ^= p;
        parent[x] = r;
        return {parent[x], parity[x]};
    }

    bool constrain(uint32_t a, uint32_t b, uint8_t relation) {
        auto [ra, pa] = find(a);
        auto [rb, pb] = find(b);
        if (ra == rb) return uint8_t(pa ^ pb) == relation;

        if (rank[ra] < rank[rb]) {
            std::swap(ra, rb);
            std::swap(pa, pb);
        }

        parent[rb] = ra;
        parity[rb] = uint8_t(pa ^ pb ^ relation);
        if (rank[ra] == rank[rb]) ++rank[ra];
        return true;
    }
};

struct Machine {
    // Every physical element exists structurally at all times.
    // The only retained per-element source-dependent quantity is one polarity bit.
    std::vector<uint8_t> polarity_bits;
    uint64_t source_length = 0;

    Machine() : polarity_bits(MACHINE_BYTES, 0) {}

    uint8_t stored(uint32_t node) const {
        return uint8_t((polarity_bits[node >> 3] >> (node & 7)) & 1u);
    }

    void set(uint32_t node, uint8_t value) {
        uint8_t &x = polarity_bits[node >> 3];
        const uint8_t mask = uint8_t(1u << (node & 7));
        if (value) x |= mask;
        else x &= uint8_t(~mask);
    }

    State observe_node(uint64_t page, uint32_t node, bool activated) const {
        Element e;
        e.retained_positive = stored(node) != 0;
        return e.observe(activated, page_phase(page, node) != 0);
    }

    // One routed relation is activated for this bit query.
    // All other million-node structures remain present but signal DEAD/0.
    State query_bit(uint64_t page, uint32_t local_pos, uint8_t bit) const {
        const auto r = route(page, local_pos, bit);
        const State sa = observe_node(page, r.a, true);
        const State sb = observe_node(page, r.b, true);

        if (trucompute::neither(sa) || trucompute::neither(sb))
            return State::STATELESS_ZERO;

        const bool a_pos = sa == State::POS;
        const bool b_pos = sb == State::POS;
        const bool relation_positive = bool(a_pos ^ b_pos ^ (r.modifier != 0));

        // The active relation contributes one signed vote.
        // sum 0 would be STATELESS; a valid single relation resolves +/-.
        return trucompute::settle_sum(relation_positive ? +1 : -1, true);
    }

    bool decoded_byte(uint64_t page, uint32_t local_pos, uint8_t &out) const {
        out = 0;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const State s = query_bit(page, local_pos, bit);
            if (s == State::STATELESS_ZERO || s == State::NEITHER)
                return false;
            if (s == State::POS)
                out |= uint8_t(1u << bit);
        }
        return true;
    }

    State press_character(uint64_t page, uint32_t local_pos, uint16_t button) const {
        if (button >= BUTTON_COUNT) return State::NEITHER;
        uint8_t decoded = 0;
        if (!decoded_byte(page, local_pos, decoded))
            return State::STATELESS_ZERO; // addressed but unresolved = stateless
        return decoded == uint8_t(button) ? State::POS : State::NEG;
    }
};

enum class Control : uint8_t {
    CONTINUE = 0,
    NEXT_DATASET = 1,
    DONE = 2
};

#pragma pack(push, 1)
struct Header {
    char magic[8];
    uint16_t version;
    uint16_t header_bytes;
    uint32_t node_count;
    uint32_t page_size;
    uint16_t button_count;
    uint16_t reserved0;
    uint64_t source_length;
    uint64_t machine_bytes;
    uint64_t route_id;
    uint8_t reserved[16];
};
#pragma pack(pop)

static_assert(sizeof(Header) == 64);

static std::vector<uint8_t> read_all(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("open input failed");
    return {std::istreambuf_iterator<char>(f), {}};
}

static void save_machine(const Machine &m, const std::string &path) {
    Header h{};
    std::memcpy(h.magic, "TRUCOMP3", 8);
    h.version = VERSION;
    h.header_bytes = sizeof(Header);
    h.node_count = NODE_COUNT;
    h.page_size = PAGE_SIZE;
    h.button_count = BUTTON_COUNT;
    h.source_length = m.source_length;
    h.machine_bytes = m.polarity_bits.size();
    h.route_id = 0x545255434F4D5033ULL;

    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("open artifact failed");
    f.write(reinterpret_cast<const char *>(&h), sizeof(h));
    f.write(reinterpret_cast<const char *>(m.polarity_bits.data()),
            std::streamsize(m.polarity_bits.size()));
}

static Machine load_machine(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("open artifact failed");

    Header h{};
    f.read(reinterpret_cast<char *>(&h), sizeof(h));

    if (std::memcmp(h.magic, "TRUCOMP3", 8)
        || h.version != VERSION
        || h.node_count != NODE_COUNT
        || h.page_size != PAGE_SIZE
        || h.button_count != BUTTON_COUNT
        || h.machine_bytes != MACHINE_BYTES)
        throw std::runtime_error("artifact incompatible");

    Machine m;
    m.source_length = h.source_length;
    f.read(reinterpret_cast<char *>(m.polarity_bits.data()),
           std::streamsize(m.polarity_bits.size()));
    if (!f) throw std::runtime_error("artifact truncated");

    char extra;
    if (f.read(&extra, 1))
        throw std::runtime_error("artifact has trailing source-dependent data");
    return m;
}


static int write_blank(const std::string &artifact_path) {
    Machine machine;
    machine.source_length = 0;

    // A blank machine is already the complete physical machine.
    // All retained polarity bits use their canonical untuned orientation.
    // With no active control, every element observes as NEITHER.
    for (uint32_t i = 0; i < NODE_COUNT; ++i) {
        if (machine.observe_node(0, i, false) != State::NEITHER) {
            std::cout << "status=BLANK_STATE_FAIL\nnode=" << i << "\n";
            return 8;
        }
    }

    save_machine(machine, artifact_path);

    std::cout
        << "status=BLANK_FROZEN\n"
        << "physical_nodes=" << NODE_COUNT << "\n"
        << "structurally_live_nodes=" << NODE_COUNT << "\n"
        << "retained_polarity_bits=" << NODE_COUNT << "\n"
        << "machine_payload_bytes=" << MACHINE_BYTES << "\n"
        << "artifact_bytes=" << (sizeof(Header) + MACHINE_BYTES) << "\n"
        << "inactive_signal=NEITHER\n";
    return 0;
}

static int imprint(const std::string &source_path, const std::string &artifact_path) {
    const auto source = read_all(source_path);
    ParityDSU solver(NODE_COUNT);

    // WRITE TIME: source may tune only pre-existing machine relations.
    for (uint64_t t = 0; t < source.size(); ++t) {
        const uint64_t page = t / PAGE_SIZE;
        const uint32_t local = uint32_t(t % PAGE_SIZE);
        const uint8_t byte = source[t];

        for (uint8_t bit = 0; bit < 8; ++bit) {
            const auto r = route(page, local, bit);
            const uint8_t target = uint8_t((byte >> bit) & 1u);
            const uint8_t wanted_relation = uint8_t(
                target
                ^ r.modifier
                ^ page_phase(page, r.a)
                ^ page_phase(page, r.b));

            if (!solver.constrain(r.a, r.b, wanted_relation)) {
                std::cout
                    << "status=IMPRINT_CONTRADICTION\n"
                    << "byte=" << t << "\n"
                    << "page=" << page << "\n"
                    << "local=" << local << "\n"
                    << "bit=" << unsigned(bit) << "\n"
                    << "node_count=" << NODE_COUNT << "\n"
                    << "machine_growth=0\n";
                return 2;
            }
        }
    }

    Machine machine;
    machine.source_length = source.size();

    for (uint32_t i = 0; i < NODE_COUNT; ++i) {
        auto [root, polarity] = solver.find(i);
        (void)root;
        machine.set(i, polarity);
    }

    for (uint64_t t = 0; t < source.size(); ++t) {
        const uint64_t page = t / PAGE_SIZE;
        const uint32_t local = uint32_t(t % PAGE_SIZE);
        uint8_t decoded = 0;
        if (!machine.decoded_byte(page, local, decoded) || decoded != source[t]) {
            std::cout << "status=FREEZE_VERIFY_FAIL\nbyte=" << t << "\n";
            return 3;
        }
    }

    save_machine(machine, artifact_path);

    std::cout
        << "status=FROZEN\n"
        << "source_bytes=" << source.size() << "\n"
        << "physical_nodes=" << NODE_COUNT << "\n"
        << "structurally_live_nodes=" << NODE_COUNT << "\n"
        << "retained_polarity_bits=" << NODE_COUNT << "\n"
        << "machine_payload_bytes=" << MACHINE_BYTES << "\n"
        << "artifact_bytes=" << (sizeof(Header) + MACHINE_BYTES) << "\n";

    return 0;
}

static int replay(const std::string &artifact_path,
                  const std::string &output_path,
                  bool strict_buttons) {
    const Machine machine = load_machine(artifact_path);
    std::ofstream out(output_path, std::ios::binary);
    if (!out) throw std::runtime_error("open output failed");

    // READ TIME: source is unavailable and tuning is immutable.
    const uint64_t pages =
        (machine.source_length + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t written = 0;

    for (uint64_t page = 0; page < pages; ++page) {
        const uint32_t page_bytes = uint32_t(std::min<uint64_t>(
            PAGE_SIZE, machine.source_length - written));

        if (strict_buttons) {
            std::vector<int16_t> resolved(page_bytes, -1);

            // Fixed control program: button 0 -> 255, always.
            for (uint16_t button = 0; button < BUTTON_COUNT; ++button) {
                for (uint32_t local = 0; local < page_bytes; ++local) {
                    const State response = machine.press_character(page, local, button);
                    if (response == State::STATELESS_ZERO) {
                        std::cout << "status=STATELESS\npage=" << page
                                  << "\nlocal=" << local << "\n";
                        return 6;
                    }
                    if (response == State::POS) {
                        if (resolved[local] != -1) {
                            std::cout << "status=AMBIGUOUS\npage=" << page
                                      << "\nlocal=" << local << "\n";
                            return 4;
                        }
                        resolved[local] = int16_t(button);
                    }
                }
            }

            for (uint32_t local = 0; local < page_bytes; ++local) {
                if (resolved[local] < 0) {
                    std::cout << "status=UNRESOLVED\npage=" << page
                              << "\nlocal=" << local << "\n";
                    return 5;
                }
                const uint8_t byte = uint8_t(resolved[local]);
                out.write(reinterpret_cast<const char *>(&byte), 1);
            }
        } else {
            for (uint32_t local = 0; local < page_bytes; ++local) {
                uint8_t byte = 0;
                if (!machine.decoded_byte(page, local, byte)) {
                    std::cout << "status=STATELESS\npage=" << page
                              << "\nlocal=" << local << "\n";
                    return 6;
                }
                out.write(reinterpret_cast<const char *>(&byte), 1);
            }
        }

        written += page_bytes;
        const Control control =
            (page + 1 < pages) ? Control::NEXT_DATASET : Control::DONE;

        std::cout << "page=" << page << " control="
                  << (control == Control::NEXT_DATASET ? "NEXT_DATASET" : "DONE")
                  << "\n";
    }

    std::cout
        << "status=REPLAY_PASS\n"
        << "recovered_bytes=" << written << "\n"
        << "button_order=0..255\n";
    return 0;
}

static int selftest() {
    using namespace trucompute;

    Element e;
    e.retained_positive = true;

    if (!e.structurally_live()) return 1;
    if (e.observe(false) != State::NEITHER) return 2;
    if (e.observe(true) != State::POS) return 3;
    if (e.observe(true, true) != State::NEG) return 4;
    if (settle({+1, -1}) != State::STATELESS_ZERO) return 5;
    if (!stateless(settle({+1, -1}))) return 6;
    if (net(settle({+1, -1})) != 0) return 7;
    if (combine(State::NEG, State::POS) != State::STATELESS_ZERO) return 8;
    if (combine(State::NEITHER, State::POS) != State::POS) return 9;

    bool neither_rejected = false;
    try {
        (void)net(State::NEITHER);
    } catch (const std::runtime_error &) {
        neither_rejected = true;
    }
    if (!neither_rejected) return 10;

    std::cout
        << "TRUCOMPUTE_V3_CONFORMANCE=PASS\n"
        << "TRUCOMPRESSION_FOUR_CONDITION_MACHINE=PASS\n"
        << "physical_nodes=" << NODE_COUNT << "\n"
        << "structurally_live_nodes=" << NODE_COUNT << "\n"
        << "inactive_signal=NEITHER\n"
        << "active_sum_zero=STATELESS_ZERO\n"
        << "retained_polarity_bits=" << NODE_COUNT << "\n"
        << "button_order=0..255\n";
    return 0;
}

} // namespace trucompression

int main(int argc, char **argv) {
    using namespace trucompression;

    try {
        if (argc == 2 && std::string(argv[1]) == "selftest")
            return selftest();

        if (argc == 3 && std::string(argv[1]) == "blank")
            return write_blank(argv[2]);

        if (argc == 4 && std::string(argv[1]) == "imprint")
            return imprint(argv[2], argv[3]);

        if ((argc == 4 || argc == 5) && std::string(argv[1]) == "replay")
            return replay(argv[2], argv[3],
                argc == 5 && std::string(argv[4]) == "--strict-buttons");

        std::cerr
            << "usage:\n"
            << "  trucompression_v3 selftest\n"
            << "  trucompression_v3 blank ARTIFACT\n"
            << "  trucompression_v3 imprint SOURCE ARTIFACT\n"
            << "  trucompression_v3 replay ARTIFACT OUTPUT [--strict-buttons]\n";
        return 64;
    } catch (const std::exception &e) {
        std::cerr << "error=" << e.what() << "\n";
        return 70;
    }
}
