#include "api/http_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "transport/json_parser.h"

namespace agri {

namespace {

std::string StatusText(int statusCode) {
    switch (statusCode) {
        case 200:
            return "OK";
        case 202:
            return "Accepted";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        default:
            return "Internal Server Error";
    }
}

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string Trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return std::string(text.substr(begin, end - begin));
}

std::size_t ParseContentLength(const std::string& headers) {
    const std::regex pattern("Content-Length:\\s*([0-9]+)", std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_search(headers, match, pattern)) {
        return 0;
    }
    try {
        return static_cast<std::size_t>(std::stoull(match[1].str()));
    } catch (...) {
        return 0;
    }
}

std::string ReadHttpRequest(int fd) {
    constexpr std::size_t kMaxRequestBytes = 1024 * 1024;

    std::string raw;
    raw.reserve(4096);

    std::size_t expectedTotalSize = 0;
    bool haveExpectedSize = false;

    while (raw.size() < kMaxRequestBytes) {
        char buffer[4096];
        const ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }
        raw.append(buffer, static_cast<std::size_t>(received));

        const std::size_t headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            continue;
        }

        if (!haveExpectedSize) {
            const std::string headers = raw.substr(0, headerEnd);
            const std::size_t contentLength = ParseContentLength(headers);
            expectedTotalSize = headerEnd + 4 + contentLength;
            haveExpectedSize = true;
            if (contentLength == 0) {
                break;
            }
        }

        if (haveExpectedSize && raw.size() >= expectedTotalSize) {
            break;
        }
    }

    return raw;
}

bool SendAll(int fd, const std::string& payload) {
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const ssize_t sent = send(fd, payload.data() + offset, payload.size() - offset, 0);
        if (sent <= 0) {
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

std::optional<HttpServer::HttpRequest> ParseHttpRequest(const std::string& raw) {
    const std::size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return std::nullopt;
    }

    const std::string headers = raw.substr(0, headerEnd);
    const std::string body = raw.substr(headerEnd + 4);

    const std::size_t firstLineEnd = headers.find("\r\n");
    if (firstLineEnd == std::string::npos) {
        return std::nullopt;
    }

    const std::string firstLine = headers.substr(0, firstLineEnd);
    std::istringstream lineStream(firstLine);

    HttpServer::HttpRequest request;
    std::string httpVersion;
    if (!(lineStream >> request.method >> request.path >> httpVersion)) {
        return std::nullopt;
    }

    request.headers = headers.substr(firstLineEnd + 2);
    request.body = body;
    return request;
}

std::string BoolAsJson(bool value) {
    return value ? "true" : "false";
}

std::string ReceiptToJson(const std::optional<BlockchainReceipt>& receipt) {
    if (!receipt.has_value()) {
        return "null";
    }

    const BlockchainReceipt& r = *receipt;
    std::ostringstream out;
    out << "{"
        << "\"txHash\":\"" << JsonEscape(r.txHash) << "\","
        << "\"blockHeight\":" << r.blockHeight << ","
        << "\"submittedAt\":\"" << JsonEscape(r.submittedAtIso8601) << "\""
        << "}";
    return out.str();
}

std::string RecordToJson(const TelemetryRecord& record) {
    std::ostringstream out;
    out << "{"
        << "\"recordId\":" << record.recordId << ","
        << "\"deviceId\":\"" << JsonEscape(record.packet.deviceId) << "\","
        << "\"timestamp\":" << record.packet.timestamp << ","
        << "\"telemetry\":" << record.packet.telemetryJson << ","
        << "\"hash\":\"" << JsonEscape(record.packet.hashHex) << "\","
        << "\"signature\":\"" << JsonEscape(record.packet.signature) << "\","
        << "\"pubKeyId\":\"" << JsonEscape(record.packet.pubKeyId) << "\","
        << "\"transport\":\"" << JsonEscape(record.packet.transport) << "\"";

    if (!record.packet.batchCode.empty()) {
        out << ",\"batchCode\":\"" << JsonEscape(record.packet.batchCode) << "\"";
    }

    out << ",\"receipt\":" << ReceiptToJson(record.receipt) << "}";
    return out.str();
}

std::array<unsigned char, 20> Sha1Digest(std::string_view input) {
    std::vector<unsigned char> message(input.begin(), input.end());
    const std::uint64_t bitLength = static_cast<std::uint64_t>(message.size()) * 8;

    message.push_back(0x80);
    while ((message.size() % 64) != 56) {
        message.push_back(0x00);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<unsigned char>((bitLength >> shift) & 0xFF));
    }

    auto rol = [](std::uint32_t value, int bits) {
        return static_cast<std::uint32_t>((value << bits) | (value >> (32 - bits)));
    };

    std::uint32_t h0 = 0x67452301;
    std::uint32_t h1 = 0xEFCDAB89;
    std::uint32_t h2 = 0x98BADCFE;
    std::uint32_t h3 = 0x10325476;
    std::uint32_t h4 = 0xC3D2E1F0;

    for (std::size_t chunk = 0; chunk < message.size(); chunk += 64) {
        std::uint32_t w[80] = {0};
        for (int i = 0; i < 16; ++i) {
            const std::size_t offset = chunk + static_cast<std::size_t>(i) * 4;
            w[i] = (static_cast<std::uint32_t>(message[offset]) << 24) |
                   (static_cast<std::uint32_t>(message[offset + 1]) << 16) |
                   (static_cast<std::uint32_t>(message[offset + 2]) << 8) |
                   static_cast<std::uint32_t>(message[offset + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        std::uint32_t e = h4;

        for (int i = 0; i < 80; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            const std::uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::array<unsigned char, 20> digest{};
    const std::uint32_t words[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
        digest[i * 4] = static_cast<unsigned char>((words[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<unsigned char>((words[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<unsigned char>((words[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<unsigned char>(words[i] & 0xFF);
    }
    return digest;
}

std::string Base64Encode(const unsigned char* bytes, std::size_t length) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((length + 2) / 3) * 4);

    std::size_t i = 0;
    while (i + 2 < length) {
        const std::uint32_t value = (static_cast<std::uint32_t>(bytes[i]) << 16) |
                                    (static_cast<std::uint32_t>(bytes[i + 1]) << 8) |
                                    static_cast<std::uint32_t>(bytes[i + 2]);
        out.push_back(kAlphabet[(value >> 18) & 0x3F]);
        out.push_back(kAlphabet[(value >> 12) & 0x3F]);
        out.push_back(kAlphabet[(value >> 6) & 0x3F]);
        out.push_back(kAlphabet[value & 0x3F]);
        i += 3;
    }

    const std::size_t remaining = length - i;
    if (remaining == 1) {
        const std::uint32_t value = static_cast<std::uint32_t>(bytes[i]) << 16;
        out.push_back(kAlphabet[(value >> 18) & 0x3F]);
        out.push_back(kAlphabet[(value >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (remaining == 2) {
        const std::uint32_t value =
            (static_cast<std::uint32_t>(bytes[i]) << 16) | (static_cast<std::uint32_t>(bytes[i + 1]) << 8);
        out.push_back(kAlphabet[(value >> 18) & 0x3F]);
        out.push_back(kAlphabet[(value >> 12) & 0x3F]);
        out.push_back(kAlphabet[(value >> 6) & 0x3F]);
        out.push_back('=');
    }

    return out;
}

} 

HttpServer::HttpServer(
    std::uint16_t port,
    IngestService& ingestService,
    const TelemetryRepository& repository)
    : port_(port), ingestService_(ingestService), repository_(repository) {}

void HttpServer::Start() {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        throw std::runtime_error("failed to create socket");
    }

    int reuse = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);

    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        throw std::runtime_error("failed to bind socket");
    }

    if (listen(listenFd_, 16) < 0) {
        throw std::runtime_error("failed to listen");
    }

    running_ = true;
    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        const int clientFd = accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            if (running_) {
                continue;
            }
            break;
        }

#ifdef SO_NOSIGPIPE
        int noSigPipe = 1;
        setsockopt(clientFd, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe));
#endif

        const bool keepOpen = HandleClient(clientFd);
        if (!keepOpen) {
            close(clientFd);
        }
    }
}

void HttpServer::Stop() {
    running_ = false;
    if (listenFd_ >= 0) {
        shutdown(listenFd_, SHUT_RDWR);
        close(listenFd_);
        listenFd_ = -1;
    }

    std::lock_guard<std::mutex> lock(wsMutex_);
    for (const int fd : telemetryWsClients_) {
        close(fd);
    }
    for (const int fd : alertWsClients_) {
        close(fd);
    }
    telemetryWsClients_.clear();
    alertWsClients_.clear();
}

bool HttpServer::HandleClient(int clientFd) {
    const std::string rawRequest = ReadHttpRequest(clientFd);
    if (rawRequest.empty()) {
        return false;
    }

    const auto request = ParseHttpRequest(rawRequest);
    if (!request.has_value()) {
        const HttpResponse response{400, "{\"error\":\"invalid HTTP request\"}", "application/json"};
        const std::string rawResponse = BuildRawResponse(response);
        SendAll(clientFd, rawResponse);
        return false;
    }

    const std::string path = StripQuery(request->path);
    if (path == "/ws/telemetry" || path == "/ws/alerts") {
        return TryUpgradeWebSocket(clientFd, *request, path);
    }

    const HttpResponse response = Route(*request);
    const std::string rawResponse = BuildRawResponse(response);
    SendAll(clientFd, rawResponse);
    return false;
}

bool HttpServer::TryUpgradeWebSocket(int clientFd, const HttpRequest& request, const std::string& path) {
    if (request.method != "GET") {
        return false;
    }

    const std::string upgrade = ToLower(GetHeaderValue(request.headers, "Upgrade"));
    const std::string connection = ToLower(GetHeaderValue(request.headers, "Connection"));
    const std::string key = Trim(GetHeaderValue(request.headers, "Sec-WebSocket-Key"));

    if (upgrade != "websocket" || connection.find("upgrade") == std::string::npos || key.empty()) {
        const HttpResponse response{400, "{\"error\":\"invalid websocket upgrade\"}", "application/json"};
        SendAll(clientFd, BuildRawResponse(response));
        return false;
    }

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << BuildWebSocketAccept(key) << "\r\n"
             << "\r\n";

    if (!SendAll(clientFd, response.str())) {
        return false;
    }

    std::lock_guard<std::mutex> lock(wsMutex_);
    if (path == "/ws/telemetry") {
        telemetryWsClients_.push_back(clientFd);
    } else {
        alertWsClients_.push_back(clientFd);
    }
    return true;
}

void HttpServer::BroadcastIngestEvent(const TelemetryPacket& packet, const IngestResult& result) {
    std::lock_guard<std::mutex> lock(wsMutex_);

    if (result.accepted) {
        std::ostringstream body;
        body << "{"
             << "\"type\":\"telemetry.ingested\"," 
             << "\"deviceId\":\"" << JsonEscape(packet.deviceId) << "\"," 
             << "\"recordId\":" << result.recordId << ","
             << "\"timestamp\":" << packet.timestamp << ","
             << "\"transport\":\"" << JsonEscape(packet.transport) << "\"," 
             << "\"txHash\":\""
             << JsonEscape(result.receipt.has_value() ? result.receipt->txHash : std::string(""))
             << "\""
             << "}";
        BroadcastMessage(body.str(), &telemetryWsClients_);
    } else {
        std::ostringstream body;
        body << "{"
             << "\"type\":\"ingest.rejected\"," 
             << "\"deviceId\":\"" << JsonEscape(packet.deviceId) << "\"," 
             << "\"message\":\"" << JsonEscape(result.message) << "\""
             << "}";
        BroadcastMessage(body.str(), &alertWsClients_);
    }
}

void HttpServer::BroadcastMessage(const std::string& payload, std::vector<int>* clients) {
    std::vector<int> active;
    active.reserve(clients->size());

    const std::string frame = BuildWebSocketFrame(payload);
    for (const int fd : *clients) {
        if (SendAll(fd, frame)) {
            active.push_back(fd);
        } else {
            close(fd);
        }
    }
    clients->swap(active);
}

HttpServer::HttpResponse HttpServer::Route(const HttpRequest& request) {
    const std::string path = StripQuery(request.path);

    if (request.method == "GET" && path == "/health") {
        return HttpResponse{200, "{\"status\":\"ok\"}", "application/json"};
    }

    if (request.method == "POST" && path == "/api/v1/ingest") {
        const ParseTelemetryResult parsed = ParseTelemetryPacketJson(request.body);
        if (!parsed.ok) {
            return HttpResponse{
                400,
                std::string("{\"error\":\"") + JsonEscape(parsed.error) + "\"}",
                "application/json"};
        }

        const IngestResult result = ingestService_.Ingest(parsed.packet);
        BroadcastIngestEvent(parsed.packet, result);

        std::ostringstream body;
        body << "{"
             << "\"accepted\":" << BoolAsJson(result.accepted) << ","
             << "\"message\":\"" << JsonEscape(result.message) << "\","
             << "\"recordId\":" << result.recordId << ","
             << "\"processingMs\":" << result.processingMs << ","
             << "\"receipt\":" << ReceiptToJson(result.receipt)
             << "}";

        return HttpResponse{result.accepted ? 202 : 400, body.str(), "application/json"};
    }

    if (request.method == "GET" && path == "/api/v1/metrics/overview") {
        const MetricsSnapshot metrics = ingestService_.GetMetricsSnapshot();
        std::ostringstream body;
        body << "{"
             << "\"totalRequests\":" << metrics.totalRequests << ","
             << "\"acceptedRequests\":" << metrics.acceptedRequests << ","
             << "\"rejectedRequests\":" << metrics.rejectedRequests << ","
             << "\"averageProcessingMs\":" << metrics.averageProcessingMs << ","
             << "\"repositorySize\":" << metrics.repositorySize
             << "}";
        return HttpResponse{200, body.str(), "application/json"};
    }

    std::string param;
    if (request.method == "GET" && ExtractPathParam(path, "/api/v1/devices/", "/latest", &param)) {
        const auto record = repository_.LatestByDevice(param);
        if (!record.has_value()) {
            return HttpResponse{404, "{\"error\":\"device not found\"}", "application/json"};
        }
        return HttpResponse{200, RecordToJson(*record), "application/json"};
    }

    if (request.method == "GET" && ExtractPathParam(path, "/api/v1/batches/", "/trace", &param)) {
        const auto records = repository_.FindByBatch(param);
        std::ostringstream body;
        body << "{"
             << "\"batchCode\":\"" << JsonEscape(param) << "\","
             << "\"count\":" << records.size() << ","
             << "\"records\":[";
        for (std::size_t i = 0; i < records.size(); ++i) {
            if (i != 0) {
                body << ",";
            }
            body << RecordToJson(records[i]);
        }
        body << "]}";
        return HttpResponse{200, body.str(), "application/json"};
    }

    if (request.method == "GET" && ExtractPathParam(path, "/api/v1/transactions/", "", &param)) {
        const auto record = repository_.FindByTransaction(param);
        if (!record.has_value()) {
            return HttpResponse{404, "{\"error\":\"transaction not found\"}", "application/json"};
        }
        return HttpResponse{200, RecordToJson(*record), "application/json"};
    }

    return HttpResponse{404, "{\"error\":\"route not found\"}", "application/json"};
}

std::string HttpServer::BuildRawResponse(const HttpResponse& response) const {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.statusCode << " " << StatusText(response.statusCode) << "\r\n"
        << "Content-Type: " << response.contentType << "\r\n"
        << "Content-Length: " << response.body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << response.body;
    return out.str();
}

std::string HttpServer::BuildWebSocketAccept(const std::string& key) {
    const std::string source = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const std::array<unsigned char, 20> digest = Sha1Digest(source);
    return Base64Encode(digest.data(), digest.size());
}

std::string HttpServer::BuildWebSocketFrame(const std::string& payload) {
    std::string frame;
    frame.reserve(payload.size() + 10);
    frame.push_back(static_cast<char>(0x81));

    const std::size_t size = payload.size();
    if (size <= 125) {
        frame.push_back(static_cast<char>(size));
    } else if (size <= 0xFFFF) {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((size >> 8) & 0xFF));
        frame.push_back(static_cast<char>(size & 0xFF));
    } else {
        frame.push_back(static_cast<char>(127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<char>((size >> shift) & 0xFF));
        }
    }

    frame.append(payload);
    return frame;
}

std::string HttpServer::GetHeaderValue(const std::string& headers, const std::string& key) {
    std::istringstream stream(headers);
    std::string line;
    const std::string target = ToLower(key);

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string lineKey = ToLower(Trim(line.substr(0, colon)));
        if (lineKey == target) {
            return Trim(line.substr(colon + 1));
        }
    }
    return "";
}

std::string HttpServer::StripQuery(std::string path) {
    const std::size_t queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        path.erase(queryPos);
    }
    return path;
}

bool HttpServer::ExtractPathParam(
    const std::string& path,
    const std::string& prefix,
    const std::string& suffix,
    std::string* value) {
    if (path.rfind(prefix, 0) != 0) {
        return false;
    }

    const std::size_t begin = prefix.size();
    if (suffix.empty()) {
        if (begin >= path.size()) {
            return false;
        }
        *value = path.substr(begin);
        return !value->empty();
    }

    if (path.size() <= begin + suffix.size()) {
        return false;
    }
    if (!path.ends_with(suffix)) {
        return false;
    }

    *value = path.substr(begin, path.size() - begin - suffix.size());
    return !value->empty();
}

}
