#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/telemetry_packet.h"
#include "domain/telemetry_record.h"

namespace agri {

class TelemetryRepository {
   public:
    virtual ~TelemetryRepository() = default;

    virtual std::uint64_t Save(const TelemetryPacket& packet) = 0;
    virtual bool AttachReceipt(std::uint64_t recordId, const BlockchainReceipt& receipt) = 0;
    virtual bool Delete(std::uint64_t recordId) = 0;
    virtual std::optional<TelemetryRecord> LatestByDevice(const std::string& deviceId) const = 0;
    virtual std::optional<TelemetryRecord> FindByTransaction(const std::string& txHash) const = 0;
    virtual std::vector<TelemetryRecord> FindByBatch(const std::string& batchCode) const = 0;
    virtual std::uint64_t Size() const = 0;
};

}
