// code taken from emp-ag2pc tutorials

#include <emp-tool/emp-tool.h>
#include "include/single_execution.h"

int main(int argc, char** argv) {
       
        if (argc < 4) {
        	std::cerr << "Usage: " << argv[0] << " <party> <port> <ip_address> <filename>" << std::endl;
        	return 1;
    	} 	
	
	int party, port;
	emp::parse_party_and_port(argv, &party, &port);
	const char* ip_address = argv[3];
    	const std::string filename = argv[4];

	emp::NetIO* io = new emp::NetIO(party==emp::ALICE ? nullptr:ip_address, port);
//      io->set_nodelay();
        test<NetIO>(party, io, circuit_file_location+filename);
        delete io;
        return 0;
}
