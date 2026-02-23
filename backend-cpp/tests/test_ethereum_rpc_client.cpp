#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "blockchain/blockchain_client.h"

namespace {

struct ScriptedResponse {
    int statusCode{200};
    std::string body;
};

class ScriptedRpcServer {
   public:
    explicit ScriptedRpcServer(std::vector<ScriptedResponse> responses)
        : responses_(std::move(responses)) {
        Start();
    }

    ~ScriptedRpcServer() {
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }

    std::uint16_t port() const { return port_; }
    std::size_t servedRequests() const { return servedRequests_.load(); }

   private:
    void Start() {
        listenerFd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listenerFd_ < 0) {
            throw std::runtime_error("failed to create test rpc socket");
        }

        int reuse = 1;
        setsockopt(listenerFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(0);

        if (bind(listenerFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            close(listenerFd_);
            throw std::runtime_error("failed to bind test rpc socket");
        }
        if (listen(listenerFd_, 8) != 0) {
            close(listenerFd_);
            throw std::runtime_error("failed to listen test rpc socket");
        }

        socklen_t len = sizeof(address);
        if (getsockname(listenerFd_, reinterpret_cast<sockaddr*>(&address), &len) != 0) {
            close(listenerFd_);
            throw std::runtime_error("failed to get test rpc port");
        }
        port_ = ntohs(address.sin_port);

        serverThread_ = std::thread([this]() {
            for (const auto& scripted : responses_) {
                sockaddr_in clientAddr{};
                socklen_t clientLen = sizeof(clientAddr);
                const int clientFd = accept(listenerFd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
                if (clientFd < 0) {
                    break;
                }

                char buffer[4096];
                while (true) {
                    const ssize_t n = recv(clientFd, buffer, sizeof(buffer), 0);
                    if (n <= 0) {
                        break;
                    }
                    std::string_view incoming(buffer, static_cast<std::size_t>(n));
                    if (incoming.find("\r\n\r\n") != std::string_view::npos) {
                        break;
                    }
                }

                std::string statusText = "OK";
                if (scripted.statusCode >= 500) {
                    statusText = "Internal Server Error";
                } else if (scripted.statusCode >= 400) {
                    statusText = "Bad Request";
                }

                const std::string response =
                    "HTTP/1.1 " + std::to_string(scripted.statusCode) + " " + statusText + "\r\n" +
                    "Content-Type: application/json\r\n" +
                    "Content-Length: " + std::to_string(scripted.body.size()) + "\r\n" +
                    "Connection: close\r\n\r\n" +
                    scripted.body;

                send(clientFd, response.data(), response.size(), 0);
                close(clientFd);
                servedRequests_.fetch_add(1);
            }

            close(listenerFd_);
            listenerFd_ = -1;
        });
    }

    std::vector<ScriptedResponse> responses_;
    std::thread serverThread_;
    std::atomic<std::size_t> servedRequests_{0};
    int listenerFd_{-1};
    std::uint16_t port_{0};
};

void TestRetriesTransientHttpStatusAndSucceeds() {
    ScriptedRpcServer server({
        {500, R"({"jsonrpc":"2.0","id":1,"error":{"code":-32010,"message":"temporary"}})"},
        {200, R"({"jsonrpc":"2.0","id":1,"result":"0xabc"})"},
        {200, R"({"jsonrpc":"2.0","id":2,"result":null})"},
        {200, R"({"jsonrpc":"2.0","id":2,"result":{"blockNumber":"0x2a"}})"},
    });

    agri::EthereumRpcConfig config;
    config.rpcUrl = "http://127.0.0.1:" + std::to_string(server.port());
    config.fromAddress = "0x1111";
    config.toAddress = "0x2222";
    config.pollIntervalMs = 1;
    config.maxWaitMs = 500;

    agri::EthereumRpcBlockchainClient client(config);
    const auto receipt = client.SubmitHash(std::string(64, 'a'), "stm32-1", 1700000000);

    assert(receipt.txHash == "0xabc");
    assert(receipt.blockHeight == 42);
    assert(server.servedRequests() == 4);
}

void TestRpcErrorDecodingIncludesCodeAndData() {
    ScriptedRpcServer server({
        {200, R"({"jsonrpc":"2.0","id":1,"error":{"code":-32000,"message":"tx rejected","data":"nonce too low"}})"},
    });

    agri::EthereumRpcConfig config;
    config.rpcUrl = "http://127.0.0.1:" + std::to_string(server.port());
    config.fromAddress = "0x1111";
    config.toAddress = "0x2222";
    config.pollIntervalMs = 1;
    config.maxWaitMs = 100;

    agri::EthereumRpcBlockchainClient client(config);

    bool threw = false;
    try {
        (void)client.SubmitHash(std::string(64, 'b'), "stm32-2", 1700000001);
    } catch (const std::runtime_error& ex) {
        threw = true;
        const std::string message = ex.what();
        assert(message.find("rpc error -32000") != std::string::npos);
        assert(message.find("tx rejected") != std::string::npos);
        assert(message.find("nonce too low") != std::string::npos);
    }

    assert(threw);
    assert(server.servedRequests() == 1);
}

}

int main() {
    TestRetriesTransientHttpStatusAndSucceeds();
    TestRpcErrorDecodingIncludesCodeAndData();
    std::cout << "test_ethereum_rpc_client passed" << std::endl;
    return 0;
}
