#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "storage/telemetry_repository.h"

namespace agri {

class InMemoryTelemetryRepository final : public TelemetryRepository {
   public:
    std::uint64_t Save(const TelemetryPacket& packet) override;
    bool AttachReceipt(std::uint64_t recordId, const BlockchainReceipt& receipt) override;
    bool Delete(std::uint64_t recordId) override;
    std::optional<TelemetryRecord> LatestByDevice(const std::string& deviceId) const override;
    std::optional<TelemetryRecord> FindByTransaction(const std::string& txHash) const override;
    std::vector<TelemetryRecord> FindByBatch(const std::string& batchCode) const override;
    std::uint64_t Size() const override;

   private:
    std::optional<TelemetryRecord> FindByIdLocked(std::uint64_t recordId) const;

    mutable std::mutex mutex_;
    std::uint64_t nextRecordId_{1};
    std::vector<TelemetryRecord> records_;
    std::unordered_map<std::uint64_t, std::size_t> positionById_;
    std::unordered_map<std::string, std::vector<std::uint64_t>> recordIdsByDevice_;
    std::unordered_map<std::string, std::vector<std::uint64_t>> recordIdsByBatch_;
    std::unordered_map<std::string, std::uint64_t> recordIdByTxHash_;
};

}
