#include <memory>
#include <typeinfo>

#include <emp-sh2pc/emp-sh2pc.h>
#include <cassert>

#include "macs/utils.h"

using namespace emp;
using namespace std;

int main(int argc, char** argv) {
    string hexstring = "0xF0C310DEADBEEF";
    std::cout << "*** hex to bin test:\n";
    string bin = hex2bin(hexstring);
    std::cout << "hex: " << hexstring << "\nbin: " << bin << "\n";

    std::cout << "\n*** bin to hex test:\n";
    string hexstring2 = bin2hex(bin);
    std::cout << "bin: " << bin << "\nhex: " << hexstring2 << "\n";

    std::cout << "\nmax num bits in bitstring input to bin2hex: "
              << std::numeric_limits<unsigned long long>::digits << "\n";
}
