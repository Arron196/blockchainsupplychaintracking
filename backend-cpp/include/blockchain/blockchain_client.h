#pragma once

#include <cstdint>
#include <string>

#include "domain/telemetry_record.h"

namespace agri {

class BlockchainClient {
   public:
    virtual ~BlockchainClient() = default;
    virtual BlockchainReceipt SubmitHash(
        const std::string& hashHex,
        const std::string& deviceId,
        std::uint64_t timestamp) = 0;
};

class MockBlockchainClient final : public BlockchainClient {
   public:
    BlockchainReceipt SubmitHash(
        const std::string& hashHex,
        const std::string& deviceId,
        std::uint64_t timestamp) override;
};

struct EthereumRpcConfig {
    std::string rpcUrl{"http:" "//127.0.0.1:8545"};
    std::string fromAddress;
    std::string toAddress;
    std::uint32_t pollIntervalMs{500};
    std::uint32_t maxWaitMs{15000};
};

class EthereumRpcBlockchainClient final : public BlockchainClient {
   public:
    explicit EthereumRpcBlockchainClient(EthereumRpcConfig config);

    BlockchainReceipt SubmitHash(
        const std::string& hashHex,
        const std::string& deviceId,
        std::uint64_t timestamp) override;

   private:
    EthereumRpcConfig config_;
};

}
