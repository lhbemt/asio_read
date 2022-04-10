#include "asio.hpp"

int main(int argc, char* argv[]) {

    asio::io_context ioc;
    asio::ip::tcp::acceptor acceptor(ioc, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 8000)); // 最简单的一个acceptor，主要是为了研究它的reactor
    acceptor.async_accept([](std::error_code ec, asio::ip::tcp::socket s){});
    ioc.run();

    return 0;
}