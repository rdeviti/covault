#include <memory>
#include <typeinfo>

#include <bloomfilter.h>
#include <emp-sh2pc/emp-sh2pc.h>
#include <cassert>

#include <immintrin.h>

using namespace emp;
using namespace std;

bool compareM256Eq(volatile const __m256i x, volatile const __m256i y) {
    return (((uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(x, y))) ==
            0xFFFFFFFF);
}
void printxM256(__m256i x) {
    for (size_t i = 0; i < sizeof(__m256i); ++i) {
        printf("%02x", (((unsigned char*)(&x))[i]));
    }
}

int main(int argc, char** argv) {
    int port, party;
    if (argc < 3) {
        std::cerr << "Usage: ./sort_test party port\n";
        std::exit(-1);
    }
    parse_party_and_port(argv, &party, &port);
    auto io =
        std::make_unique<NetIO>(party == ALICE ? nullptr : "127.0.0.1", port);

    setup_semi_honest(io.get(), party);

    const int size = 1000;
    Integer* res = new Integer[size];
    __m256i m;
    uint64_t* m_ptr = nullptr;
    m_ptr = (uint64_t*)&m;
    m_ptr[0] = 0;
    m_ptr[1] = 0;
    m_ptr[2] = 0;
    for (uint64_t j = 0; j < size; ++j) {
        m_ptr[3] = (716192737 + j) % size;
        res[j] =
            Integer(&m, ((emp::ALICE % 2) == (j % 2)) ? emp::ALICE : emp::BOB);
    }

    Bit* nullptrbit = nullptr;
    auto const start = std::chrono::high_resolution_clock::now();
    sort(res, size, nullptrbit, true);
    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "sort " << (party == emp::ALICE ? "(gen)" : "(eva)") << ": "
              << elapsed.count() << '\n';

    for (uint64_t i = 0; i < size; ++i) {
        res[i].reveal(&m, emp::PUBLIC);
        if (m_ptr[3] != i) {
            std::cerr << "res[" << i
                      << "] was not what it was supposed to be\n";
            printxM256(m);
            std::cout << "\n";
        }
    }
    return 0;
}
