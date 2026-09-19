#include <array>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr uint64_t PAGE_SIZE = 1'000'000ULL;
static constexpr uint16_t PAGES = 1000;
static constexpr uint64_t EXPECTED = PAGE_SIZE * PAGES;
static constexpr uint16_t CHAR_BUTTONS = 206;
static constexpr uint16_t BITS = 8;
static constexpr uint16_t WORDS = (PAGES + 63) / 64;

struct Vec {
    std::array<uint64_t, WORDS> w{};
};

struct Basis {
    std::array<Vec, PAGES> row{};
    std::array<uint8_t, PAGES> present{};
    uint16_t rank = 0;

    static bool bit(const Vec& v, uint16_t i) {
        return (v.w[i >> 6] >> (i & 63)) & 1ULL;
    }
    static void flip_xor(Vec& a, const Vec& b) {
        for (uint16_t k = 0; k < WORDS; ++k) a.w[k] ^= b.w[k];
    }

    void insert(Vec v) {
        if (rank == PAGES) return;
        for (uint16_t p = 0; p < PAGES; ++p) {
            if (!bit(v, p)) continue;
            if (present[p]) {
                flip_xor(v, row[p]);
            } else {
                row[p] = v;
                present[p] = 1;
                ++rank;
                return;
            }
        }
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: enwik9_rank_probe ENWIK9\n";
        return 64;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) throw std::runtime_error("open failed");

    struct stat st{};
    if (fstat(fd, &st) != 0) throw std::runtime_error("stat failed");
    if (uint64_t(st.st_size) != EXPECTED) {
        std::cerr << "status=SOURCE_SIZE_MISMATCH\nbytes=" << st.st_size << "\n";
        return 2;
    }

    const uint8_t* data = static_cast<const uint8_t*>(
        mmap(nullptr, EXPECTED, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) throw std::runtime_error("mmap failed");

    std::array<uint8_t, 256> seen{};
    for (uint64_t i = 0; i < EXPECTED; ++i) seen[data[i]] = 1;

    std::array<int16_t, 256> code{};
    code.fill(-1);
    uint16_t terminal_count = 0;
    for (uint16_t b = 0; b < 256; ++b) {
        if (seen[b]) code[b] = terminal_count++;
    }

    std::cout << "source_bytes=" << EXPECTED << "\n";
    std::cout << "terminal_count=" << terminal_count << "\n";
    std::cout << "page_count=" << PAGES << "\n";
    std::cout << "page_size=" << PAGE_SIZE << "\n";

    if (terminal_count != CHAR_BUTTONS) {
        std::cout << "status=TERMINAL_COUNT_MISMATCH\n";
        munmap((void*)data, EXPECTED);
        close(fd);
        return 3;
    }

    std::array<Basis, BITS> basis{};
    uint32_t positions_examined = 0;

    for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
        std::array<Vec, BITS> col{};

        for (uint16_t q = 0; q < PAGES; ++q) {
            const uint8_t raw = data[uint64_t(q) * PAGE_SIZE + i];
            const uint16_t c = uint16_t(code[raw]);
            const uint16_t word = q >> 6;
            const uint64_t mask = 1ULL << (q & 63);
            for (uint16_t b = 0; b < BITS; ++b) {
                if ((c >> b) & 1u) col[b].w[word] |= mask;
            }
        }

        bool all_full = true;
        for (uint16_t b = 0; b < BITS; ++b) {
            if (basis[b].rank < PAGES) basis[b].insert(col[b]);
            if (basis[b].rank < PAGES) all_full = false;
        }

        positions_examined = i + 1;
        if (all_full) break;
    }

    uint16_t max_rank = 0;
    for (uint16_t b = 0; b < BITS; ++b) {
        std::cout << "rank_bit" << b << "=" << basis[b].rank << "\n";
        if (basis[b].rank > max_rank) max_rank = basis[b].rank;
    }

    const uint64_t payload = uint64_t(max_rank) * 1'001'000ULL;
    const uint64_t artifact = payload + 512ULL;

    std::cout << "positions_examined=" << positions_examined << "\n";
    std::cout << "minimum_relation_lanes=" << max_rank << "\n";
    std::cout << "minimum_payload_bytes=" << payload << "\n";
    std::cout << "minimum_artifact_bytes=" << artifact << "\n";
    std::cout << "source_ratio=" << (double(artifact) / double(EXPECTED)) << "\n";
    std::cout << "status=RANK_MEASURED\n";

    munmap((void*)data, EXPECTED);
    close(fd);
    return 0;
}
