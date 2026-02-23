#include "blockchain/blockchain_client.h"

#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "transport/json_parser.h"
#include "utils/hash_utils.h"

namespace agri {

namespace {

constexpr std::uint32_t kRpcHttpMaxAttempts = 3;
constexpr std::uint32_t kRpcRetryDelayMs = 100;
constexpr std::size_t kRpcErrorPreviewLimit = 768;
constexpr char kRpcHttpStatusPrefix[] = "rpc http status ";

enum class RpcFailureType {
    ResolveHost,
    Connect,
    Send,
    Read,
    InvalidResponse,
    InvalidStatusLine,
    HttpStatus
};

class RpcTransportError final : public std::runtime_error {
   public:
    RpcTransportError(RpcFailureType type, std::string message, int httpStatusCode = 0)
        : std::runtime_error(std::move(message)),
          type_(type),
          httpStatusCode_(httpStatusCode) {}

    RpcFailureType type() const { return type_; }
    int httpStatusCode() const { return httpStatusCode_; }

   private:
    RpcFailureType type_;
    int httpStatusCode_;
};

struct ParsedHttpUrl {
    std::string host;
    std::string port;
    std::string path;
};

std::optional<ParsedHttpUrl> ParseHttpUrl(const std::string& url) {
    const std::string prefix = "http://";
    if (url.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }

    const std::string address = url.substr(prefix.size());
    const std::size_t slashPos = address.find('/');
    const std::string hostPort = (slashPos == std::string::npos) ? address : address.substr(0, slashPos);
    const std::string path = (slashPos == std::string::npos) ? "/" : address.substr(slashPos);

    if (hostPort.empty()) {
        return std::nullopt;
    }

    ParsedHttpUrl parsed;
    const std::size_t colonPos = hostPort.rfind(':');
    if (colonPos == std::string::npos) {
        parsed.host = hostPort;
        parsed.port = "80";
    } else {
        parsed.host = hostPort.substr(0, colonPos);
        parsed.port = hostPort.substr(colonPos + 1);
        if (parsed.host.empty() || parsed.port.empty()) {
            return std::nullopt;
        }
    }
    parsed.path = path;
    return parsed;
}

std::string ReadSocketFully(int socketFd) {
    std::string response;
    char buffer[4096];

    while (true) {
        const ssize_t received = recv(socketFd, buffer, sizeof(buffer), 0);
        if (received == 0) {
            break;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw RpcTransportError(RpcFailureType::Read, "failed to read rpc response");
        }
        response.append(buffer, static_cast<std::size_t>(received));
    }
    return response;
}

void SendAll(int socketFd, std::string_view data) {
    std::size_t sentBytes = 0;

#if defined(MSG_NOSIGNAL)
    constexpr int sendFlags = MSG_NOSIGNAL;
#else
    constexpr int sendFlags = 0;
#endif

    while (sentBytes < data.size()) {
        const ssize_t sent = send(socketFd, data.data() + sentBytes, data.size() - sentBytes, sendFlags);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw RpcTransportError(RpcFailureType::Send, "failed to send rpc request");
        }
        if (sent == 0) {
            throw RpcTransportError(RpcFailureType::Send, "failed to send rpc request");
        }
        sentBytes += static_cast<std::size_t>(sent);
    }
}

std::string HttpPostJson(const std::string& url, const std::string& payload) {
    const auto parsedUrl = ParseHttpUrl(url);
    if (!parsedUrl.has_value()) {
        throw std::runtime_error("rpc url must start with http://");
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    const int resolveCode = getaddrinfo(
        parsedUrl->host.c_str(),
        parsedUrl->port.c_str(),
        &hints,
        &result);
    if (resolveCode != 0 || result == nullptr) {
        throw RpcTransportError(RpcFailureType::ResolveHost, "cannot resolve rpc host");
    }

    int socketFd = -1;
    for (addrinfo* current = result; current != nullptr; current = current->ai_next) {
        socketFd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (socketFd < 0) {
            continue;
        }
        if (connect(socketFd, current->ai_addr, current->ai_addrlen) == 0) {
            break;
        }
        close(socketFd);
        socketFd = -1;
    }
    freeaddrinfo(result);

    if (socketFd < 0) {
        throw RpcTransportError(RpcFailureType::Connect, "cannot connect to rpc endpoint");
    }

    std::ostringstream request;
    request << "POST " << parsedUrl->path << " HTTP/1.1\r\n"
            << "Host: " << parsedUrl->host << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << payload.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << payload;
    const std::string requestText = request.str();

    std::string response;
    try {
        SendAll(socketFd, requestText);
        response = ReadSocketFully(socketFd);
    } catch (...) {
        close(socketFd);
        throw;
    }
    close(socketFd);

    const std::size_t headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        throw RpcTransportError(RpcFailureType::InvalidResponse, "invalid rpc response");
    }

    const std::string statusLine = response.substr(0, response.find("\r\n"));
    const std::regex statusPattern("HTTP/1\\.[01]\\s+([0-9]{3})");
    std::smatch statusMatch;
    if (!std::regex_search(statusLine, statusMatch, statusPattern)) {
        throw RpcTransportError(RpcFailureType::InvalidStatusLine, "invalid rpc status line");
    }

    const int statusCode = std::stoi(statusMatch[1].str());
    const std::string body = response.substr(headerEnd + 4);
    if (statusCode < 200 || statusCode >= 300) {
        throw RpcTransportError(
            RpcFailureType::HttpStatus,
            std::string(kRpcHttpStatusPrefix) + std::to_string(statusCode),
            statusCode);
    }
    return body;
}

std::optional<std::string> ExtractJsonStringField(const std::string& json, const std::string& fieldName) {
    const std::regex pattern("\\\"" + fieldName + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }
    return match[1].str();
}

std::optional<std::uint64_t> ParseHexNumber(std::string_view hexValue) {
    if (hexValue.empty()) {
        return std::nullopt;
    }
    std::string text(hexValue);
    if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) {
        text = text.substr(2);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    try {
        return std::stoull(text, nullptr, 16);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::int64_t> ExtractJsonIntegerField(const std::string& json, const std::string& fieldName) {
    const std::regex pattern("\\\"" + fieldName + "\\\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }
    try {
        return std::stoll(match[1].str());
    } catch (...) {
        return std::nullopt;
    }
}

std::string ExtractRpcError(const std::string& json) {
    const std::size_t errorPos = json.find("\"error\"");
    if (errorPos == std::string::npos) {
        return "unknown rpc error";
    }

    const std::string errorJson = json.substr(errorPos, kRpcErrorPreviewLimit);
    const auto code = ExtractJsonIntegerField(errorJson, "code");
    const auto message = ExtractJsonStringField(errorJson, "message");
    const auto data = ExtractJsonStringField(errorJson, "data");

    std::string decoded;
    if (code.has_value()) {
        decoded = "rpc error " + std::to_string(*code);
        if (message.has_value() && !message->empty()) {
            decoded += ": " + *message;
        }
    } else if (message.has_value() && !message->empty()) {
        decoded = *message;
    } else {
        decoded = "unknown rpc error";
    }

    if (data.has_value() && !data->empty()) {
        decoded += " (" + *data + ")";
    }
    return decoded;
}

bool IsReceiptNull(const std::string& body) {
    const std::regex pattern("\\\"result\\\"\\s*:\\s*null");
    return std::regex_search(body, pattern);
}

bool IsTransientRpcFailure(const RpcTransportError& error) {
    switch (error.type()) {
        case RpcFailureType::Connect:
        case RpcFailureType::Send:
        case RpcFailureType::Read:
        case RpcFailureType::InvalidResponse:
        case RpcFailureType::InvalidStatusLine:
            return true;
        case RpcFailureType::HttpStatus:
            return error.httpStatusCode() >= 500;
        case RpcFailureType::ResolveHost:
            return false;
    }

    return false;
}

std::string HttpPostJsonWithRetry(const std::string& url, const std::string& payload) {
    RpcFailureType lastFailureType = RpcFailureType::Connect;
    int lastHttpStatusCode = 0;
    std::string lastMessage = "rpc retry loop exhausted";

    for (std::uint32_t attempt = 1; attempt <= kRpcHttpMaxAttempts; ++attempt) {
        try {
            return HttpPostJson(url, payload);
        } catch (const RpcTransportError& error) {
            lastFailureType = error.type();
            lastHttpStatusCode = error.httpStatusCode();
            lastMessage = error.what();

            if (attempt == kRpcHttpMaxAttempts || !IsTransientRpcFailure(error)) {
                throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(kRpcRetryDelayMs));
        }
    }

    throw RpcTransportError(lastFailureType, lastMessage, lastHttpStatusCode);
}

}  

EthereumRpcBlockchainClient::EthereumRpcBlockchainClient(EthereumRpcConfig config)
    : config_(std::move(config)) {
    if (config_.toAddress.empty()) {
        config_.toAddress = config_.fromAddress;
    }
}

BlockchainReceipt EthereumRpcBlockchainClient::SubmitHash(
    const std::string& hashHex,
    const std::string& deviceId,
    std::uint64_t timestamp) {
    (void)deviceId;
    (void)timestamp;

    if (config_.fromAddress.empty() || config_.toAddress.empty()) {
        throw std::runtime_error("from/to address not configured");
    }

    const std::string data = "0x" + hashHex;
    std::ostringstream sendTxPayload;
    sendTxPayload << "{"
                  << "\"jsonrpc\":\"2.0\"," 
                  << "\"method\":\"eth_sendTransaction\"," 
                  << "\"params\":[{"
                  << "\"from\":\"" << JsonEscape(config_.fromAddress) << "\"," 
                  << "\"to\":\"" << JsonEscape(config_.toAddress) << "\"," 
                  << "\"data\":\"" << JsonEscape(data) << "\""
                  << "}],"
                  << "\"id\":1"
                  << "}";

    const std::string sendTxResponse = HttpPostJsonWithRetry(config_.rpcUrl, sendTxPayload.str());
    if (sendTxResponse.find("\"error\"") != std::string::npos) {
        throw std::runtime_error(ExtractRpcError(sendTxResponse));
    }

    const auto txHash = ExtractJsonStringField(sendTxResponse, "result");
    if (!txHash.has_value() || txHash->empty()) {
        throw std::runtime_error("missing transaction hash in rpc response");
    }

    BlockchainReceipt receipt;
    receipt.txHash = *txHash;
    receipt.submittedAtIso8601 = CurrentUtcIso8601();

    const auto start = std::chrono::steady_clock::now();
    while (true) {
        std::ostringstream receiptPayload;
        receiptPayload << "{"
                       << "\"jsonrpc\":\"2.0\"," 
                       << "\"method\":\"eth_getTransactionReceipt\"," 
                       << "\"params\":[\"" << JsonEscape(*txHash) << "\"],"
                       << "\"id\":2"
                       << "}";

        const std::string receiptResponse = HttpPostJsonWithRetry(config_.rpcUrl, receiptPayload.str());
        if (receiptResponse.find("\"error\"") != std::string::npos) {
            throw std::runtime_error(ExtractRpcError(receiptResponse));
        }

        if (!IsReceiptNull(receiptResponse)) {
            const auto blockHex = ExtractJsonStringField(receiptResponse, "blockNumber");
            if (blockHex.has_value()) {
                receipt.blockHeight = ParseHexNumber(*blockHex).value_or(0);
            }
            return receipt;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() >= config_.maxWaitMs) {
            return receipt;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.pollIntervalMs));
    }
}

}
