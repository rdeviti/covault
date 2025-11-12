#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/circuits/sha3_256.h>
#include <include/encounter.hpp>
#include <include/secrets.hpp>

using namespace emp;
using namespace std;

namespace secrets {
int runtests(int argc, char **argv) {
    int port, party;
    if (argc < 3) {
        std::cerr << "Usage: ./encounter_test party port\n";
        std::exit(-1);
    }
    parse_party_and_port(argv, &party, &port);
    auto io =
        std::make_unique<NetIO>(party == ALICE ? nullptr : "127.0.0.1", port);

    setup_semi_honest(io.get(), party);
    secrets::Reusable_Secrets shared_secrets = secrets::Reusable_Secrets(false);
    return 0;
}
}  // namespace secrets

int main(int argc, char **argv) { return (secrets::runtests(argc, argv)); }
