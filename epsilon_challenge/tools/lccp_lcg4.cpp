#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using Sym = uint32_t;

static constexpr uint8_t VERSION = 1;
static constexpr uint8_t MODE_RAW = 0;
static constexpr uint8_t MODE_GRAPH = 1;

static void put_u32(std::ostream& o, uint32_t v) {
    for (int i = 0; i < 4; ++i) o.put(char((v >> (8 * i)) & 255));
}
static uint32_t get_u32(std::istream& i) {
    uint32_t v = 0;
    for (int k = 0; k < 4; ++k) {
        int c = i.get();
        if (c < 0) throw std::runtime_error("short u32");
        v |= uint32_t(c) << (8 * k);
    }
    return v;
}
static void put_u64(std::ostream& o, uint64_t v) {
    for (int i = 0; i < 8; ++i) o.put(char((v >> (8 * i)) & 255));
}
static uint64_t get_u64(std::istream& i) {
    uint64_t v = 0;
    for (int k = 0; k < 8; ++k) {
        int c = i.get();
        if (c < 0) throw std::runtime_error("short u64");
        v |= uint64_t(c) << (8 * k);
    }
    return v;
}
static void put_var(std::ostream& o, uint64_t v) {
    while (v >= 128) {
        o.put(char((v & 127) | 128));
        v >>= 7;
    }
    o.put(char(v));
}
static uint64_t get_var(std::istream& i) {
    uint64_t v = 0;
    int sh = 0;
    while (true) {
        int c = i.get();
        if (c < 0) throw std::runtime_error("short varint");
        v |= uint64_t(c & 127) << sh;
        if (!(c & 128)) return v;
        sh += 7;
        if (sh > 63) throw std::runtime_error("varint overflow");
    }
}
static uint64_t var_size(uint64_t v) {
    uint64_t n = 1;
    while (v >= 128) { v >>= 7; ++n; }
    return n;
}

struct ConcatRule {
    Sym left;
    Sym right;
};

struct RepeatRule {
    Sym child;
    uint64_t count;
};

struct HCode {
    Sym sym;
    uint8_t len;
    uint64_t code;
};

struct HuffmanModel {
    std::vector<uint8_t> lengths;
    std::vector<HCode> codes;
    uint64_t bit_count = 0;
    uint64_t codebook_bytes = 0;
    bool valid = true;
};

struct HNode {
    uint64_t freq;
    int left = -1;
    int right = -1;
    int sym = -1;
};

static HuffmanModel build_huffman(const std::vector<Sym>& seq, size_t alphabet) {
    HuffmanModel m;
    m.lengths.assign(alphabet, 0);
    if (seq.empty()) return m;

    std::vector<uint64_t> freq(alphabet, 0);
    for (Sym s : seq) {
        if (s >= alphabet) throw std::runtime_error("symbol outside alphabet");
        ++freq[s];
    }

    using Q = std::pair<uint64_t, int>;
    std::priority_queue<Q, std::vector<Q>, std::greater<Q>> pq;
    std::vector<HNode> nodes;
    nodes.reserve(alphabet * 2);

    for (size_t s = 0; s < alphabet; ++s) {
        if (!freq[s]) continue;
        int idx = (int)nodes.size();
        nodes.push_back({freq[s], -1, -1, (int)s});
        pq.push({freq[s], idx});
    }

    if (pq.size() == 1) {
        m.lengths[(size_t)nodes[pq.top().second].sym] = 1;
    } else {
        while (pq.size() > 1) {
            auto a = pq.top(); pq.pop();
            auto b = pq.top(); pq.pop();
            int idx = (int)nodes.size();
            nodes.push_back({a.first + b.first, a.second, b.second, -1});
            pq.push({a.first + b.first, idx});
        }

        int root = pq.top().second;
        std::vector<std::pair<int,int>> st{{root, 0}};
        while (!st.empty()) {
            auto [idx, depth] = st.back();
            st.pop_back();
            const HNode& n = nodes[idx];
            if (n.sym >= 0) {
                if (depth <= 0 || depth > 63) {
                    m.valid = false;
                    return m;
                }
                m.lengths[(size_t)n.sym] = (uint8_t)depth;
            } else {
                st.push_back({n.left, depth + 1});
                st.push_back({n.right, depth + 1});
            }
        }
    }

    std::vector<std::pair<uint8_t,Sym>> ordered;
    for (size_t s = 0; s < alphabet; ++s) {
        if (m.lengths[s]) ordered.push_back({m.lengths[s], (Sym)s});
    }
    std::sort(ordered.begin(), ordered.end(), [](auto a, auto b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
    });

    uint64_t code = 0;
    uint8_t prev_len = 0;
    for (auto [len, sym] : ordered) {
        if (len > prev_len) code <<= (len - prev_len);
        m.codes.push_back({sym, len, code});
        ++code;
        prev_len = len;
        m.codebook_bytes += var_size(sym) + 1;
    }

    for (size_t s = 0; s < alphabet; ++s) {
        if (freq[s]) m.bit_count += freq[s] * uint64_t(m.lengths[s]);
    }
    return m;
}

static void build_repeat_graph(const std::vector<Sym>& seq,
                               size_t concat_count,
                               std::vector<RepeatRule>& repeats,
                               std::vector<Sym>& root) {
    repeats.clear();
    root.clear();
    root.reserve(seq.size());

    for (size_t i = 0; i < seq.size();) {
        size_t j = i + 1;
        while (j < seq.size() && seq[j] == seq[i]) ++j;
        uint64_t run = uint64_t(j - i);

        // Conservative threshold: a repeat node must pay for child+count metadata.
        if (run >= 4) {
            Sym id = Sym(256 + concat_count + repeats.size());
            repeats.push_back({seq[i], run});
            root.push_back(id);
        } else {
            for (uint64_t k = 0; k < run; ++k) root.push_back(seq[i]);
        }
        i = j;
    }
}

static uint64_t graph_size(const std::vector<Sym>& seq,
                           const std::vector<ConcatRule>& concats,
                           HuffmanModel* out_model = nullptr,
                           std::vector<RepeatRule>* out_repeats = nullptr,
                           std::vector<Sym>* out_root = nullptr) {
    std::vector<RepeatRule> repeats;
    std::vector<Sym> root;
    build_repeat_graph(seq, concats.size(), repeats, root);

    HuffmanModel hm = build_huffman(root, 256 + concats.size() + repeats.size());
    if (!hm.valid) return std::numeric_limits<uint64_t>::max();

    uint64_t n = 4 + 1 + 1 + 8; // magic, version, mode, original bytes
    n += 4;                      // concat count
    for (const auto& r : concats) n += var_size(r.left) + var_size(r.right);
    n += 4;                      // repeat count
    for (const auto& r : repeats) n += var_size(r.child) + var_size(r.count);
    n += 8;                      // root symbol count
    n += 4;                      // used symbol count
    n += hm.codebook_bytes;
    n += 8;                      // root bit count
    n += (hm.bit_count + 7) / 8;

    if (out_model) *out_model = std::move(hm);
    if (out_repeats) *out_repeats = std::move(repeats);
    if (out_root) *out_root = std::move(root);
    return n;
}

class BitWriter {
    std::ostream& out;
    uint8_t cur = 0;
    int used = 0;
public:
    explicit BitWriter(std::ostream& o) : out(o) {}
    void put(uint64_t code, uint8_t len) {
        for (int b = int(len) - 1; b >= 0; --b) {
            cur = uint8_t((cur << 1) | ((code >> b) & 1));
            ++used;
            if (used == 8) {
                out.put(char(cur));
                cur = 0;
                used = 0;
            }
        }
    }
    void finish() {
        if (used) {
            cur <<= (8 - used);
            out.put(char(cur));
        }
    }
};

class BitReader {
    std::istream& in;
    uint8_t cur = 0;
    int left = 0;
public:
    explicit BitReader(std::istream& i) : in(i) {}
    int get() {
        if (!left) {
            int c = in.get();
            if (c < 0) throw std::runtime_error("short bitstream");
            cur = uint8_t(c);
            left = 8;
        }
        int bit = (cur >> 7) & 1;
        cur <<= 1;
        --left;
        return bit;
    }
};

static std::vector<uint8_t> read_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("open input");
    std::streamsize sz = in.tellg();
    if (sz < 0) throw std::runtime_error("input size");
    in.seekg(0);
    std::vector<uint8_t> data((size_t)sz);
    if (sz) {
        in.read(reinterpret_cast<char*>(data.data()), sz);
        if (in.gcount() != sz) throw std::runtime_error("short input");
    }
    return data;
}

static std::vector<Sym> replace_pair(const std::vector<Sym>& seq,
                                     Sym a, Sym b, Sym new_sym,
                                     uint64_t* replacements = nullptr) {
    std::vector<Sym> out;
    out.reserve(seq.size());
    uint64_t reps = 0;
    for (size_t i = 0; i < seq.size();) {
        if (i + 1 < seq.size() && seq[i] == a && seq[i + 1] == b) {
            out.push_back(new_sym);
            i += 2;
            ++reps;
        } else {
            out.push_back(seq[i]);
            ++i;
        }
    }
    if (replacements) *replacements = reps;
    return out;
}

static void encode(const std::string& inpath,
                   const std::string& outpath,
                   size_t max_rules) {
    std::vector<uint8_t> src = read_all(inpath);
    const uint64_t raw_carrier = 4 + 1 + 1 + 8 + src.size();

    std::vector<Sym> seq;
    seq.reserve(src.size());
    for (uint8_t b : src) seq.push_back((Sym)b);

    std::vector<ConcatRule> rules;
    rules.reserve(max_rules);

    uint64_t current_size = graph_size(seq, rules);
    uint64_t best_size = current_size;
    std::vector<Sym> best_seq = seq;
    std::vector<ConcatRule> best_rules = rules;

    size_t no_improve = 0;
    const size_t patience = 32;
    uint64_t explored = 0;

    for (size_t iter = 0; iter < max_rules && seq.size() >= 2; ++iter) {
        std::unordered_map<uint64_t, uint32_t> counts;
        counts.reserve(std::min<size_t>(seq.size() / 2 + 1, 524288));

        for (size_t i = 0; i + 1 < seq.size(); ++i) {
            uint64_t key = (uint64_t(seq[i]) << 32) | uint64_t(seq[i + 1]);
            auto it = counts.find(key);
            if (it == counts.end()) counts.emplace(key, 1);
            else if (it->second != UINT32_MAX) ++it->second;
        }

        uint64_t best_key = std::numeric_limits<uint64_t>::max();
        uint32_t best_count = 0;
        for (const auto& kv : counts) {
            if (kv.second > best_count ||
                (kv.second == best_count && kv.first < best_key)) {
                best_count = kv.second;
                best_key = kv.first;
            }
        }
        if (best_count < 3) break;

        Sym a = Sym(best_key >> 32);
        Sym b = Sym(best_key & 0xffffffffu);
        Sym new_sym = Sym(256 + rules.size());

        uint64_t reps = 0;
        std::vector<Sym> candidate = replace_pair(seq, a, b, new_sym, &reps);
        if (reps < 2) break;

        std::vector<ConcatRule> candidate_rules = rules;
        candidate_rules.push_back({a, b});
        uint64_t candidate_size = graph_size(candidate, candidate_rules);

        std::cerr << "GRAPH_ITER=" << iter
                  << " A=" << a
                  << " B=" << b
                  << " OBS=" << best_count
                  << " REPLACED=" << reps
                  << " BEFORE=" << current_size
                  << " AFTER=" << candidate_size << "\n";

        seq.swap(candidate);
        rules.swap(candidate_rules);
        current_size = candidate_size;
        ++explored;

        if (current_size < best_size) {
            best_size = current_size;
            best_seq = seq;
            best_rules = rules;
            no_improve = 0;
            std::cerr << "GRAPH_CHECKPOINT_BEST=" << best_size
                      << " CONCAT_NODES=" << best_rules.size() << "\n";
        } else {
            ++no_improve;
        }

        if (no_improve >= patience) {
            std::cerr << "GRAPH_STOP_PATIENCE=" << patience << "\n";
            break;
        }
    }

    seq.swap(best_seq);
    rules.swap(best_rules);

    HuffmanModel hm;
    std::vector<RepeatRule> repeats;
    std::vector<Sym> root;
    uint64_t final_graph_size = graph_size(seq, rules, &hm, &repeats, &root);
    bool use_graph = final_graph_size < raw_carrier && !src.empty();

    std::ofstream out(outpath, std::ios::binary);
    if (!out) throw std::runtime_error("open output");

    out.write("LCG4", 4);
    out.put(char(VERSION));
    out.put(char(use_graph ? MODE_GRAPH : MODE_RAW));
    put_u64(out, src.size());

    if (!use_graph) {
        if (!src.empty()) out.write(reinterpret_cast<const char*>(src.data()), src.size());
    } else {
        put_u32(out, (uint32_t)rules.size());
        for (const auto& r : rules) {
            put_var(out, r.left);
            put_var(out, r.right);
        }

        put_u32(out, (uint32_t)repeats.size());
        for (const auto& r : repeats) {
            put_var(out, r.child);
            put_var(out, r.count);
        }

        put_u64(out, root.size());

        uint32_t used = 0;
        for (uint8_t x : hm.lengths) if (x) ++used;
        put_u32(out, used);
        for (Sym s = 0; s < hm.lengths.size(); ++s) {
            if (!hm.lengths[s]) continue;
            put_var(out, s);
            out.put(char(hm.lengths[s]));
        }

        put_u64(out, hm.bit_count);

        std::vector<uint64_t> code_by_sym(hm.lengths.size(), 0);
        for (const HCode& c : hm.codes) code_by_sym[c.sym] = c.code;

        BitWriter bw(out);
        for (Sym s : root) bw.put(code_by_sym[s], hm.lengths[s]);
        bw.finish();
    }

    out.flush();
    std::ifstream chk(outpath, std::ios::binary | std::ios::ate);
    uint64_t actual = chk ? uint64_t(chk.tellg()) : 0;

    std::cerr << "SOURCE_BYTES=" << src.size() << "\n";
    std::cerr << "RAW_CARRIER_BYTES=" << raw_carrier << "\n";
    std::cerr << "GRAPH_CARRIER_EST=" << final_graph_size << "\n";
    std::cerr << "CONCAT_NODES=" << rules.size() << "\n";
    std::cerr << "REPEAT_NODES=" << repeats.size() << "\n";
    std::cerr << "ROOT_NODES=" << root.size() << "\n";
    std::cerr << "RULES_EXPLORED=" << explored << "\n";
    std::cerr << "SELECTED_MODE=" << (use_graph ? "graph" : "raw") << "\n";
    std::cerr << "ACTUAL_CARRIER_BYTES=" << actual << "\n";
    std::cerr << "DECODE_MODEL=walk_root_expand_graph_emit_bytes\n";
}

struct DNode {
    int child[2] = {-1, -1};
    int sym = -1;
};

static std::vector<HCode> canonical_from_lengths(
    const std::vector<std::pair<Sym,uint8_t>>& lens) {
    std::vector<std::pair<uint8_t,Sym>> ordered;
    ordered.reserve(lens.size());
    for (auto [s,l] : lens) ordered.push_back({l,s});
    std::sort(ordered.begin(), ordered.end(), [](auto a, auto b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
    });

    std::vector<HCode> out;
    uint64_t code = 0;
    uint8_t prev = 0;
    for (auto [len,sym] : ordered) {
        if (!len || len > 63) throw std::runtime_error("bad huffman length");
        if (len > prev) code <<= (len - prev);
        out.push_back({sym,len,code});
        ++code;
        prev = len;
    }
    return out;
}

struct Frame {
    Sym sym;
    uint64_t count;
};

static void emit_graph_symbol(std::ostream& out,
                              Sym root_sym,
                              const std::vector<ConcatRule>& concats,
                              const std::vector<RepeatRule>& repeats,
                              uint64_t& written,
                              uint64_t original) {
    const uint64_t concat_base = 256;
    const uint64_t repeat_base = concat_base + concats.size();

    std::vector<Frame> st;
    st.push_back({root_sym, 1});

    while (!st.empty()) {
        Frame f = st.back();
        st.pop_back();

        if (f.count == 0) continue;
        if (f.count > 1) {
            st.push_back({f.sym, f.count - 1});
            st.push_back({f.sym, 1});
            continue;
        }

        uint64_t x = f.sym;
        if (x < 256) {
            if (written >= original) throw std::runtime_error("decode overrun");
            out.put(char(uint8_t(x)));
            ++written;
        } else if (x < repeat_base) {
            size_t idx = size_t(x - concat_base);
            if (idx >= concats.size()) throw std::runtime_error("bad concat ref");
            st.push_back({concats[idx].right, 1});
            st.push_back({concats[idx].left, 1});
        } else {
            size_t idx = size_t(x - repeat_base);
            if (idx >= repeats.size()) throw std::runtime_error("bad repeat ref");
            st.push_back({repeats[idx].child, repeats[idx].count});
        }
    }
}

static void decode(const std::string& inpath, const std::string& outpath) {
    std::ifstream in(inpath, std::ios::binary);
    if (!in) throw std::runtime_error("open encoded");

    char magic[4];
    in.read(magic, 4);
    if (in.gcount() != 4 || std::string(magic, 4) != "LCG4")
        throw std::runtime_error("bad magic");

    int ver = in.get();
    if (ver != VERSION) throw std::runtime_error("bad version");

    int mode = in.get();
    if (mode < 0) throw std::runtime_error("short mode");

    uint64_t original = get_u64(in);

    std::ofstream out(outpath, std::ios::binary);
    if (!out) throw std::runtime_error("open decoded");

    if (mode == MODE_RAW) {
        std::array<char,65536> buf{};
        uint64_t left = original;
        while (left) {
            size_t n = (size_t)std::min<uint64_t>(left, buf.size());
            in.read(buf.data(), n);
            if ((size_t)in.gcount() != n) throw std::runtime_error("short raw payload");
            out.write(buf.data(), n);
            left -= n;
        }
        return;
    }
    if (mode != MODE_GRAPH) throw std::runtime_error("unknown mode");

    uint32_t nc = get_u32(in);
    std::vector<ConcatRule> concats;
    concats.reserve(nc);
    for (uint32_t i = 0; i < nc; ++i) {
        uint64_t a = get_var(in), b = get_var(in);
        uint64_t max_allowed = 256ull + i;
        if (a >= max_allowed || b >= max_allowed)
            throw std::runtime_error("forward concat ref");
        concats.push_back({(Sym)a,(Sym)b});
    }

    uint32_t nr = get_u32(in);
    std::vector<RepeatRule> repeats;
    repeats.reserve(nr);
    uint64_t repeat_base = 256ull + nc;
    for (uint32_t i = 0; i < nr; ++i) {
        uint64_t child = get_var(in);
        uint64_t count = get_var(in);
        if (child >= repeat_base || count < 2)
            throw std::runtime_error("bad repeat node");
        repeats.push_back({(Sym)child,count});
    }

    uint64_t root_count = get_u64(in);
    uint32_t used = get_u32(in);
    uint64_t alphabet = repeat_base + nr;

    std::vector<std::pair<Sym,uint8_t>> lens;
    lens.reserve(used);
    for (uint32_t i = 0; i < used; ++i) {
        uint64_t s = get_var(in);
        int l = in.get();
        if (s >= alphabet || l <= 0 || l > 63)
            throw std::runtime_error("bad codebook");
        lens.push_back({(Sym)s,(uint8_t)l});
    }

    uint64_t bit_count = get_u64(in);
    (void)bit_count;

    std::vector<HCode> codes = canonical_from_lengths(lens);
    std::vector<DNode> tree(1);
    for (const HCode& c : codes) {
        int node = 0;
        for (int b = int(c.len) - 1; b >= 0; --b) {
            int bit = int((c.code >> b) & 1);
            if (tree[node].child[bit] < 0) {
                tree[node].child[bit] = (int)tree.size();
                tree.push_back(DNode{});
            }
            node = tree[node].child[bit];
        }
        if (tree[node].sym >= 0) throw std::runtime_error("duplicate code");
        tree[node].sym = (int)c.sym;
    }

    BitReader br(in);
    uint64_t written = 0;
    for (uint64_t k = 0; k < root_count; ++k) {
        int node = 0;
        while (tree[node].sym < 0) {
            int bit = br.get();
            int next = tree[node].child[bit];
            if (next < 0) throw std::runtime_error("invalid root stream");
            node = next;
        }
        emit_graph_symbol(out, (Sym)tree[node].sym, concats, repeats, written, original);
    }

    if (written != original) throw std::runtime_error("size mismatch");
}

int main(int argc, char** argv) {
    try {
        if (argc < 4) {
            std::cerr << "usage: lccp_lcg4 e|d input output [max_concat_nodes]\n";
            return 2;
        }

        std::string mode = argv[1];
        if (mode == "e") {
            size_t max_rules = argc > 4 ? std::stoul(argv[4]) : 4096;
            encode(argv[2], argv[3], max_rules);
        } else if (mode == "d") {
            decode(argv[2], argv[3]);
        } else {
            throw std::runtime_error("mode");
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR " << e.what() << "\n";
        return 1;
    }
    return 0;
}
