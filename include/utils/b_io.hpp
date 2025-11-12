// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include <boost/asio.hpp>
#include <iostream>

using namespace boost::asio;
using ip::tcp;

void accept_handler(const boost::system::error_code& error) {
    if (!error) {
        // Accept succeeded.
    }
}

emp::block read_block(tcp::socket& socket) {
    boost::asio::streambuf buf;
    size_t n = sizeof(emp::block);
    boost::asio::read(socket, buf, boost::asio::transfer_exactly(n));
    const char* data = boost::asio::buffer_cast<const char*>(buf.data());
    return (*((emp::block*)data));
}

void send_block(tcp::socket& socket, emp::block& block) {
    boost::system::error_code error;
    boost::asio::write(
        socket, boost::asio::buffer((void*)&block, sizeof(block)), error);
    if (error) std::cerr << "Send failed: " << error.message() << std::endl;
}
