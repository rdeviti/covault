#pragma once

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/writer.h"

using namespace std::string_literals;

// reducer equality check parser
void parse(std::string file, int& party, int& port, std::string& peer_ip_1,
        std::string& peer_ip_2, size_t* tile_size, std::string* outfile = nullptr) {
    // reading JSON file
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) {
        std::cout << "Error: The JSON file in input does not exist: " << file
                  << std::endl;
        std::exit(-1);
    }
    char input[65536];
    rapidjson::FileReadStream is(fp, input, sizeof(input));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);

    // parsing values in JSON options
    if (d.HasMember("options")) {
        const rapidjson::Value& options = d["options"];
        // check necessary fields
        if (options.HasMember("party") && options.HasMember("port")) {
            party = options["party"].GetInt();
            port = options["port"].GetInt();
        } else {
            std::cerr << "Error: Required fields missing in JSON input!\n";
            std::exit(0);
        }
        // check optional fields
        if (options.HasMember("peer_ip_1"))
            peer_ip_1 = options["peer_ip_1"].GetString();
        if (options.HasMember("peer_ip_2"))
            peer_ip_2 = options["peer_ip_2"].GetString();
        if (options.HasMember("tile_size"))
            *tile_size = options["tile_size"].GetInt();
        if (options.HasMember("outfile"))
            if (outfile != nullptr) *outfile = options["outfile"].GetString();
    }
}

// primitives parser
void parse(std::string file, int& party, int& port, std::string& peer_ip,
           size_t* tile_size = nullptr, int* n_reps = nullptr,
           std::string* outfile = nullptr) {
    // reading JSON file
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) {
        std::cout << "Error: The JSON file in input does not exist: " << file
                  << std::endl;
        std::exit(-1);
    }
    char input[65536];
    rapidjson::FileReadStream is(fp, input, sizeof(input));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);

    // parsing values in JSON options
    if (d.HasMember("options")) {
        const rapidjson::Value& options = d["options"];
        // check necessary fields
        if (options.HasMember("party") && options.HasMember("port")) {
            party = options["party"].GetInt();
            port = options["port"].GetInt();
        } else {
            std::cerr << "Error: Required fields missing in JSON input!\n";
            std::exit(0);
        }
        // check optional fields
        if (options.HasMember("peer_ip"))
            peer_ip = options["peer_ip"].GetString();
        if (options.HasMember("tile_size"))
            if (tile_size != nullptr)
                *tile_size = options["tile_size"].GetInt();
        if (options.HasMember("n_reps"))
            if (n_reps != nullptr) *n_reps = options["n_reps"].GetInt();
        if (options.HasMember("outfile"))
            if (outfile != nullptr) *outfile = options["outfile"].GetString();
    }
}

// ingress parser
void parse(std::string file, int& party, int& port, std::string& peer_ip,
           std::string& redis_ip, uint16_t* redis_port, int& n_reps,
           std::string& outfile) {
    // reading JSON file
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) {
        std::cout << "Error: The JSON file in input does not exist: " << file
                  << std::endl;
        std::exit(-1);
    }
    char input[65536];
    rapidjson::FileReadStream is(fp, input, sizeof(input));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);

    // parsing values in JSON options
    if (d.HasMember("options")) {
        const rapidjson::Value& options = d["options"];
        // check necessary fields
        if (options.HasMember("party") && options.HasMember("port")) {
            party = options["party"].GetInt();
            port = options["port"].GetInt();
        } else {
            std::cerr << "Error: Required fields missing in JSON input!\n";
            std::exit(0);
        }
        // check optional fields
        if (options.HasMember("peer_ip"))
            peer_ip = options["peer_ip"].GetString();
        if (options.HasMember("outfile"))
            outfile = options["outfile"].GetString();
        if (options.HasMember("n_reps")) n_reps = options["n_reps"].GetInt();
        if (options.HasMember("redis_ip"))
            redis_ip = options["redis_ip"].GetString();
        if (options.HasMember("redis_port"))
            *redis_port = options["redis_port"].GetInt();
    } else {
        std::cerr << "Error: Options missing in JSON input!\n";
        std::exit(-1);
    }

    std::cout << "Input variables (PID: " << getpid() << "): " << std::endl
              << "Party\t\t" << party << std::endl
              << "Peer IP\t\t" << peer_ip << std::endl
              << "Port\t\t" << port << std::endl
              << "Redis IP\t" << redis_ip << std::endl
              << "Redis Port\t" << *redis_port << std::endl
              << "Repetitions\t" << n_reps << std::endl
              << "Outfile\t\t" << outfile << std::endl;
}

// microbenchmark parser
void parse(std::string file, int& party, int& port, int& tile_start,
           int& tile_end, size_t& tile_size, std::string& peer_ip,
           std::string& redis_ip,
           uint16_t* redis_port, int& n_reps, std::string& outfile,
           int& id) {
    // reading JSON file
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) {
        std::cout << "Error: The JSON file in input does not exist: " << file
                  << std::endl;
        std::exit(-1);
    }
    char input[65536];
    rapidjson::FileReadStream is(fp, input, sizeof(input));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);

    // parsing values in JSON options
    if (d.HasMember("options")) {
        const rapidjson::Value& options = d["options"];
        // check necessary fields
        try {
            party = options["party"].GetInt();
            port = options["port"].GetInt();
            tile_start = options["tile_start"].GetInt();
            tile_end = options["tile_end"].GetInt();
            tile_size = options["tile_size"].GetInt();
            peer_ip = options["peer_ip"].GetString();
            *redis_port = options["redis_port"].GetInt();
            redis_ip = options["redis_ip"].GetString();
            id = options["id"].GetInt();
        } catch (const std::exception& e) {
            std::cerr << "Error: Required fields missing in JSON input!\n";
            std::exit(0);
        }
        // check optional fields
        if (options.HasMember("n_reps")) n_reps = options["n_reps"].GetInt();
        if (options.HasMember("outfile"))
            outfile = options["outfile"].GetString();
    }

    // check tile range
    if (tile_end < tile_start) {
        std::cerr << "Error: tile_end < tile_start!" << std::endl;
        std::exit(-1);
    }

    /*
    std::cout << "Input variables (PID: " << getpid() << "): " << std::endl
          << "Party\t\t" << party << std::endl
          << "Peer IP\t\t" << peer_ip << std::endl
          << "Port\t\t" << port << std::endl
          << "Node ID\t\t" << id << std::endl
          << "Redis IP\t" << redis_ip << std::endl
          << "Redis Port\t" << *redis_port << std::endl
          << "Tile Key Start\t" << tile_start << std::endl
          << "Tile Key End\t" << tile_end << std::endl
          << "Tile Size\t" << tile_size << std::endl
          << "Repetitions\t" << n_reps << std::endl
          << "Outfile\t\t" << outfile << std::endl;
          */
}

// node parser
void parse(std::string file, int& party, int& port, int& tile_start,
           int& tile_end, size_t& tile_size, std::string& peer_ip,
           std::string& reducer_ip_1, std::string& reducer_ip_2,
           std::string& reducer_ip_3, int& reducer_port, std::string& redis_ip,
           uint16_t* redis_port, int& n_reps, std::string& outfile,
           int& rounds, int& id) {
    // reading JSON file
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) {
        std::cout << "Error: The JSON file in input does not exist: " << file
                  << std::endl;
        std::exit(-1);
    }
    char input[65536];
    rapidjson::FileReadStream is(fp, input, sizeof(input));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);

    // parsing values in JSON options
    if (d.HasMember("options")) {
        const rapidjson::Value& options = d["options"];
        // check necessary fields
        try {
            party = options["party"].GetInt();
            port = options["port"].GetInt();
            tile_start = options["tile_start"].GetInt();
            tile_end = options["tile_end"].GetInt();
            tile_size = options["tile_size"].GetInt();
            peer_ip = options["peer_ip"].GetString();
            *redis_port = options["redis_port"].GetInt();
            redis_ip = options["redis_ip"].GetString();
            rounds = options["rounds"].GetInt();
            id = options["id"].GetInt();
        } catch (const std::exception& e) {
            std::cerr << "Error: Required fields missing in JSON input!\n";
            std::exit(0);
        }
        // check optional fields
        if (options.HasMember("reducer_ip_1"))
            reducer_ip_1 = options["reducer_ip_1"].GetString();
        if (options.HasMember("reducer_ip_2"))
            reducer_ip_2 = options["reducer_ip_2"].GetString();
        if (options.HasMember("reducer_ip_3"))
            reducer_ip_3 = options["reducer_ip_3"].GetString();
        if (options.HasMember("reducer_port"))
            reducer_port = options["reducer_port"].GetInt();
        if (options.HasMember("n_reps")) n_reps = options["n_reps"].GetInt();
        if (options.HasMember("outfile"))
            outfile = options["outfile"].GetString();
    }

    // check tile range
    if (tile_end < tile_start) {
        std::cerr << "Error: tile_end < tile_start!" << std::endl;
        std::exit(-1);
    }

    /*
    std::cout << "Input variables (PID: " << getpid() << "): " << std::endl
          << "Party\t\t" << party << std::endl
          << "Peer IP\t\t" << peer_ip << std::endl
          << "Port\t\t" << port << std::endl
          << "Rounds\t\t" << rounds << std::endl
          << "Node ID\t\t" << id << std::endl
          << "Redis IP\t" << redis_ip << std::endl
          << "Redis Port\t" << *redis_port << std::endl
          << "Tile Key Start\t" << tile_start << std::endl
          << "Tile Key End\t" << tile_end << std::endl
          << "Tile Size\t" << tile_size << std::endl
          << "Reducer Port\t" << reducer_port << std::endl
          << "Reducer IP 1 (receive from)\t" << reducer_ip_1 << std::endl
          << "Reducer IP 2 (receive from)\t" << reducer_ip_2 << std::endl
          << "Reducer IP 3 (send to)\t" << reducer_ip_3 << std::endl
          << "Repetitions\t" << n_reps << std::endl
          << "Outfile\t\t" << outfile << std::endl;
          */
}

// mapper and reducer parser
void parse(std::string file, bool is_mapper, int& party, int& port,
           int& tile_start, int& tile_end, size_t& tile_size,
           std::string& peer_ip, std::string& mapper_reducer_ip,
           int& reducer_port, std::string& redis_ip, uint16_t* redis_port,
           int& n_reps, std::string& outfile) {
    // reading JSON file
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) {
        std::cout << "Error: The JSON file in input does not exist: " << file
                  << std::endl;
        std::exit(-1);
    }
    char input[65536];
    rapidjson::FileReadStream is(fp, input, sizeof(input));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);

    // parsing values in JSON options
    if (d.HasMember("options")) {
        const rapidjson::Value& options = d["options"];
        // check necessary fields
        if (options.HasMember("party") && options.HasMember("port") &&
            options.HasMember("reducer_port") &&
            options.HasMember("tile_start") && options.HasMember("tile_end") &&
            options.HasMember("tile_size")) {
            party = options["party"].GetInt();
            port = options["port"].GetInt();
            reducer_port = options["reducer_port"].GetInt();
            tile_start = options["tile_start"].GetInt();
            tile_end = options["tile_end"].GetInt();
            tile_size = options["tile_size"].GetInt();
        } else {
            std::cerr << "Error: Required fields missing in JSON input!\n";
            std::exit(0);
        }
        if (is_mapper) {
            if (options.HasMember("redis_port")) {
                *redis_port = options["redis_port"].GetInt();
            } else {
                std::cerr << "Error: Required fields missing in JSON input!\n";
                std::exit(0);
            }
        }
        // check optional fields
        if (options.HasMember("peer_ip"))
            peer_ip = options["peer_ip"].GetString();
        if (options.HasMember("n_reps")) n_reps = options["n_reps"].GetInt();
        if (options.HasMember("outfile"))
            outfile = options["outfile"].GetString();
        if (is_mapper) {
            if (options.HasMember("reducer_ip"))
                mapper_reducer_ip = options["reducer_ip"].GetString();
            if (options.HasMember("redis_ip"))
                redis_ip = options["redis_ip"].GetString();
        } else {  // reducer
            if (options.HasMember("mapper_ip"))
                mapper_reducer_ip = options["mapper_ip"].GetString();
        }
    } else {
        std::cerr << "Error: Options missing in JSON input!\n";
        std::exit(-1);
    }

    std::cout << "Input variables (PID: " << getpid() << "): " << std::endl
              << "Party\t\t" << party << std::endl
              << "Peer IP\t\t" << peer_ip << std::endl
              << "Port\t\t" << port << std::endl;
    if (is_mapper) {
        std::cout << "Redis IP\t" << redis_ip << std::endl
                  << "Redis Port\t" << *redis_port << std::endl
                  << "Reducer IP\t";
    } else {
        std::cout << "Mapper IP\t";
    }
    std::cout << mapper_reducer_ip << std::endl
              << "Reducer Port\t" << reducer_port << std::endl
              << "Tile Key Start\t" << tile_start << std::endl
              << "Tile Key End\t" << tile_end << std::endl
              << "Tile Size\t" << tile_size << std::endl
              << "Repetitions\t" << n_reps << std::endl
              << "\n";

    // check tile range
    if (tile_end < tile_start) {
        std::cerr << "Error: tile_end < tile_start!" << std::endl;
        std::exit(-1);
    }
}
