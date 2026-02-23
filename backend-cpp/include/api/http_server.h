#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "services/ingest_service.h"
#include "storage/telemetry_repository.h"

namespace agri {

class HttpServer {
   public:
    struct HttpRequest {
        std::string method;
        std::string path;
        std::string headers;
        std::string body;
    };

    struct HttpResponse {
        int statusCode{200};
        std::string body;
        std::string contentType{"application/json"};
    };

    HttpServer(std::uint16_t port, IngestService& ingestService, const TelemetryRepository& repository);

    void Start();
    void Stop();

   private:
    bool HandleClient(int clientFd);
    bool TryUpgradeWebSocket(int clientFd, const HttpRequest& request, const std::string& path);
    void BroadcastIngestEvent(const TelemetryPacket& packet, const IngestResult& result);
    void BroadcastMessage(const std::string& payload, std::vector<int>* clients);
    HttpResponse Route(const HttpRequest& request);
    std::string BuildRawResponse(const HttpResponse& response) const;
    static std::string BuildWebSocketAccept(const std::string& key);
    static std::string BuildWebSocketFrame(const std::string& payload);
    static std::string GetHeaderValue(const std::string& headers, const std::string& key);
    static std::string StripQuery(std::string path);
    static bool ExtractPathParam(
        const std::string& path,
        const std::string& prefix,
        const std::string& suffix,
        std::string* value);

    std::uint16_t port_;
    IngestService& ingestService_;
    const TelemetryRepository& repository_;
    std::atomic<bool> running_{false};
    int listenFd_{-1};
    std::mutex wsMutex_;
    std::vector<int> telemetryWsClients_;
    std::vector<int> alertWsClients_;
};

}
