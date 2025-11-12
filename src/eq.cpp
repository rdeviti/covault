#include "include/dualex/eq.h"
#include "emp-sh2pc/emp-sh2pc.h"
#include "include/dualex/gen_circuit.h"

namespace {
void usage(char const* arg0) {
    std::cout << "Usage: " << arg0 << " -h\n"
              << "       " << arg0 << " -c [bitsize]\n";
    std::exit(0);
}
}  // namespace

int main(int argc, char** argv) {
    using namespace std::string_literals;
    size_t bit_size = 0;

    if (argc == 2 && argv[1] == "-h"s) {
        usage(argv[0]);
    } else if (argc == 3 && argv[1] == "-c"s) {
        bit_size = std::atoi(argv[argc - 1]);
    } else {
        usage(argv[0]);
    }

    std::cout << "constructing circuit...\n";
    std::string filename = "circuits/eq" + std::to_string(bit_size) + ".txt";
    emp::setup_plain_prot(true, filename);  // write circuit to file
    // inputs (all zeroes) = { value }
    std::vector<int> input_a(1, 0);  // (repeat, value)
    std::vector<int> input_b(1, 0);
    int res = gen_circuit("eq", input_a, input_b, bit_size);
    if (res == -1) {
        std::cout << "Error writing circuit to file.\n";
        std::exit(0);
    }
    emp::finalize_plain_prot();
    std::cout << "Circuit written to file.\n";
}
