// webcart-gateway: the HTTP/XML bridge that lets the Java storefront reach
// the business tier. It speaks the same protocol as asp/gateway.asp; this
// build hosts the portable core in-process so the bridge also runs on Linux.
//
//   POST /webcart/gateway   body: <request op="..." .../>   -> text/xml
//   GET  /health            -> "ok"
//
// One connection at a time, HTTP/1.0 semantics (Connection: close). The Shop
// is internally locked, so a threaded accept loop would be a local change.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "webcart/gateway_protocol.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static void close_socket(socket_t s) { closesocket(s); }
static bool valid(socket_t s) { return s != INVALID_SOCKET; }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static void close_socket(socket_t s) { ::close(s); }
static bool valid(socket_t s) { return s >= 0; }
#endif

namespace {

constexpr std::size_t kMaxRequest = 64 * 1024;

struct Options {
    std::string bind = "127.0.0.1";
    int port = 8099;
    std::string catalog = "data/catalog.csv";
    std::string promotions = "data/promotions.csv";
    std::string journal = "orders.journal";
};

bool send_all(socket_t s, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int n = ::send(s, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

std::string response(int status, const char* reason, const std::string& type, const std::string& body) {
    return "HTTP/1.0 " + std::to_string(status) + " " + reason + "\r\nContent-Type: " + type +
           "\r\nContent-Length: " + std::to_string(body.size()) +
           "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + body;
}

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

void serve_one(socket_t client, webcart::GatewayProtocol& protocol) {
    std::string data;
    char buf[4096];
    std::size_t header_end = std::string::npos;
    while (header_end == std::string::npos) {
        const int n = ::recv(client, buf, sizeof buf, 0);
        if (n <= 0) return;
        data.append(buf, static_cast<std::size_t>(n));
        header_end = data.find("\r\n\r\n");
        if (data.size() > kMaxRequest) {
            send_all(client, response(413, "Payload Too Large", "text/plain", "too large\n"));
            return;
        }
    }
    const std::string head = data.substr(0, header_end);
    const std::string request_line = head.substr(0, head.find("\r\n"));
    std::size_t content_length = 0;
    for (std::size_t pos = head.find("\r\n"); pos != std::string::npos;) {
        const std::size_t next = head.find("\r\n", pos + 2);
        const std::string line = head.substr(pos + 2, next == std::string::npos ? std::string::npos : next - pos - 2);
        const auto colon = line.find(':');
        if (colon != std::string::npos && lower(line.substr(0, colon)) == "content-length")
            content_length = std::strtoul(line.c_str() + colon + 1, nullptr, 10);
        pos = next;
    }
    if (content_length > kMaxRequest) {
        send_all(client, response(413, "Payload Too Large", "text/plain", "too large\n"));
        return;
    }
    std::string body = data.substr(header_end + 4);
    while (body.size() < content_length) {
        const int n = ::recv(client, buf, sizeof buf, 0);
        if (n <= 0) return;
        body.append(buf, static_cast<std::size_t>(n));
    }
    body.resize(content_length);

    if (request_line.rfind("POST /webcart/gateway ", 0) == 0) {
        send_all(client, response(200, "OK", "text/xml; charset=utf-8", protocol.handle(body)));
    } else if (request_line.rfind("GET /health ", 0) == 0) {
        send_all(client, response(200, "OK", "text/plain", "ok\n"));
    } else {
        send_all(client, response(404, "Not Found", "text/plain", "not found\n"));
    }
}

int usage() {
    std::cerr << "usage: webcart-gateway [--bind ADDR] [--port N (0 = any)] [--catalog CSV]\n"
                 "                      [--promotions CSV] [--journal FILE]\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (i + 1 >= argc) return usage();
        const std::string v = argv[++i];
        if (a == "--bind") opt.bind = v;
        else if (a == "--port") opt.port = std::atoi(v.c_str());
        else if (a == "--catalog") opt.catalog = v;
        else if (a == "--promotions") opt.promotions = v;
        else if (a == "--journal") opt.journal = v;
        else return usage();
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
#endif

    webcart::ShopConfig config;
    config.journal_path = opt.journal;
    webcart::Shop shop(webcart::Catalog::load_csv(opt.catalog),
                       webcart::load_promotions_csv(opt.promotions), config,
                       std::make_shared<webcart::SystemClock>());
    webcart::GatewayProtocol protocol(shop);

    socket_t server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!valid(server)) return 1;
    int yes = 1;
    ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(opt.port));
    if (inet_pton(AF_INET, opt.bind.c_str(), &addr.sin_addr) != 1) return usage();
    if (::bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || ::listen(server, 16) != 0) {
        std::perror("bind/listen");
        return 1;
    }
    socklen_t len = sizeof addr;
    ::getsockname(server, reinterpret_cast<sockaddr*>(&addr), &len);

    const auto& report = shop.startup_report();
    // The first stdout line is machine-read by the Java integration test.
    std::cout << "listening on " << opt.bind << ":" << ntohs(addr.sin_port) << std::endl;
    std::cerr << "journal " << opt.journal << ": " << report.orders_replayed << " orders replayed, "
              << report.groups_discarded << " incomplete discarded" << std::endl;

    for (;;) {
        socket_t client = ::accept(server, nullptr, nullptr);
        if (!valid(client)) continue;
        try {
            serve_one(client, protocol);
        } catch (const std::exception& e) {
            std::cerr << "request failed: " << e.what() << std::endl;
        }
        close_socket(client);
    }
}
