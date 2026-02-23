#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "storage/telemetry_repository.h"

struct sqlite3;
struct sqlite3_stmt;

namespace agri {

class SQLiteTelemetryRepository final : public TelemetryRepository {
   public:
    explicit SQLiteTelemetryRepository(const std::string& databasePath);
    ~SQLiteTelemetryRepository() override;

    SQLiteTelemetryRepository(const SQLiteTelemetryRepository&) = delete;
    SQLiteTelemetryRepository& operator=(const SQLiteTelemetryRepository&) = delete;

    std::uint64_t Save(const TelemetryPacket& packet) override;
    bool AttachReceipt(std::uint64_t recordId, const BlockchainReceipt& receipt) override;
    std::optional<TelemetryRecord> LatestByDevice(const std::string& deviceId) const override;
    std::optional<TelemetryRecord> FindByTransaction(const std::string& txHash) const override;
    std::vector<TelemetryRecord> FindByBatch(const std::string& batchCode) const override;
    std::uint64_t Size() const override;

   private:
    void EnsureSchema();
    static TelemetryRecord RowToRecord(::sqlite3_stmt* statement);

    mutable std::mutex mutex_;
    sqlite3* db_{nullptr};
};

}
