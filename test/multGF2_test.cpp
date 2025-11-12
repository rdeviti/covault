#include <emp-sh2pc/emp-sh2pc.h>
#include <cassert>
#include <climits>
#include <memory>
#include <typeinfo>

#include "macs/verify.h"

// number of gates
int gates(int party) {
    int gates = -1;
    if (party == emp::ALICE) {
        emp::HalfGateGen<NetIO>* circ =
            (emp::HalfGateGen<NetIO>*)CircuitExecution::circ_exec;
        gates = circ->num_and();
    }
    return gates;
}

int main(int argc, char** argv) {
    int port, party;
    if (argc < 3) {
        std::cerr << "Usage: ./multGF2_test party port\n";
        std::exit(-1);
    }
    emp::parse_party_and_port(argv, &party, &port);
    auto io =
        std::make_unique<NetIO>(party == ALICE ? nullptr : "127.0.0.1", port);

    emp::setup_semi_honest(io.get(), party);

    auto const start = std::chrono::high_resolution_clock::now();

    // set up variables
    emp::Integer a(LAMBDA, 42);
    emp::Integer b(LAMBDA, 28);
    emp::Integer poly(LAMBDA, IRRED_POLY);

    // do the multiplication
    multGF2(b, a, poly);

    // measurements
    std::cout << "gates for one mult\n"
              << "measured: " << gates(party) << "\n"
              << "theoretical: " << 2 * LAMBDA * LAMBDA << "\n";

    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "multGF2 " << (party == emp::ALICE ? "(gen)" : "(eva)") << ": "
              << elapsed.count() << '\n';
}
