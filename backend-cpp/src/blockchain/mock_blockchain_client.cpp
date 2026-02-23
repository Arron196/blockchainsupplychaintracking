#include "blockchain/blockchain_client.h"

#include <atomic>
#include <string>

#include "utils/hash_utils.h"

namespace agri {

BlockchainReceipt MockBlockchainClient::SubmitHash(
    const std::string& hashHex,
    const std::string& deviceId,
    std::uint64_t timestamp) {
    static std::atomic<std::uint64_t> counter{1};
    const std::uint64_t nonce = counter.fetch_add(1);

    const std::string payload =
        hashHex + "|" + deviceId + "|" + std::to_string(timestamp) + "|" + std::to_string(nonce);
    const std::string txHash = Sha256Hex(payload);

    std::uint64_t blockHeight = 100000;
    try {
        blockHeight += std::stoull(txHash.substr(0, 8), nullptr, 16) % 900000;
    } catch (...) {
        blockHeight = 100000;
    }

    BlockchainReceipt receipt;
    receipt.txHash = txHash;
    receipt.blockHeight = blockHeight;
    receipt.submittedAtIso8601 = CurrentUtcIso8601();
    return receipt;
}

}
