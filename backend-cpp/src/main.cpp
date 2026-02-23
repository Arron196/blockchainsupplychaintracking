#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "api/http_server.h"
#include "blockchain/blockchain_client.h"
#include "security/signature_verifier.h"
#include "services/ingest_service.h"
#include "storage/sqlite_telemetry_repository.h"

namespace {

agri::HttpServer* gServer = nullptr;

void HandleSignal(int) {
    if (gServer != nullptr) {
        gServer->Stop();
    }
}

}

int main() {
    constexpr std::uint16_t kPort = 8080;

    const char* sqlitePathEnv = std::getenv("AGRI_SQLITE_PATH");
    const std::string sqlitePath =
        (sqlitePathEnv != nullptr) ? std::string(sqlitePathEnv) : std::string("backend-cpp/data/agri_gateway.db");
    agri::SQLiteTelemetryRepository repository(sqlitePath);

    const char* keyDirEnv = std::getenv("AGRI_PUBLIC_KEYS_DIR");
    const std::string keyDir =
        (keyDirEnv != nullptr) ? std::string(keyDirEnv) : std::string("backend-cpp/keys/public");
    agri::PublicKeyMap publicKeys = agri::LoadPublicKeysFromDirectory(keyDir);
    const std::size_t loadedKeyCount = publicKeys.size();
    agri::BasicSignatureVerifier signatureVerifier(std::move(publicKeys));

    const char* chainModeEnv = std::getenv("AGRI_CHAIN_MODE");
    const std::string chainMode =
        (chainModeEnv != nullptr) ? std::string(chainModeEnv) : std::string("mock");

    std::unique_ptr<agri::BlockchainClient> blockchainClient;
    if (chainMode == "ethereum") {
        agri::EthereumRpcConfig config;

        if (const char* rpcUrl = std::getenv("AGRI_ETH_RPC_URL"); rpcUrl != nullptr) {
            config.rpcUrl = rpcUrl;
        }
        if (const char* from = std::getenv("AGRI_ETH_FROM"); from != nullptr) {
            config.fromAddress = from;
        }
        if (const char* to = std::getenv("AGRI_ETH_TO"); to != nullptr) {
            config.toAddress = to;
        }
        if (const char* pollMs = std::getenv("AGRI_ETH_POLL_MS"); pollMs != nullptr) {
            config.pollIntervalMs = static_cast<std::uint32_t>(std::stoul(pollMs));
        }
        if (const char* waitMs = std::getenv("AGRI_ETH_MAX_WAIT_MS"); waitMs != nullptr) {
            config.maxWaitMs = static_cast<std::uint32_t>(std::stoul(waitMs));
        }

        blockchainClient = std::make_unique<agri::EthereumRpcBlockchainClient>(config);
    } else {
        blockchainClient = std::make_unique<agri::MockBlockchainClient>();
    }

    agri::IngestService ingestService(repository, signatureVerifier, *blockchainClient);
    agri::HttpServer server(kPort, ingestService, repository);

    gServer = &server;
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    std::cout << "agri_gateway listening on 0.0.0.0:" << kPort << std::endl;
    std::cout << "sqlite database: " << sqlitePath << std::endl;
    std::cout << "public key directory: " << keyDir << std::endl;
    std::cout << "loaded public keys: " << loadedKeyCount << std::endl;
    std::cout << "chain mode: " << chainMode << std::endl;
    std::cout << "routes: /health, /api/v1/ingest, /api/v1/metrics/overview, /ws/telemetry, /ws/alerts"
              << std::endl;

    try {
        server.Start();
    } catch (const std::exception& ex) {
        std::cerr << "fatal server error: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "agri_gateway stopped" << std::endl;
    return 0;
}
