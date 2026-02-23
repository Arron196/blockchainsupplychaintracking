#include "storage/in_memory_telemetry_repository.h"

namespace agri {

std::uint64_t InMemoryTelemetryRepository::Save(const TelemetryPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::uint64_t recordId = nextRecordId_++;
    TelemetryRecord record;
    record.recordId = recordId;
    record.packet = packet;

    records_.push_back(record);
    positionById_[recordId] = records_.size() - 1;
    recordIdsByDevice_[packet.deviceId].push_back(recordId);
    if (!packet.batchCode.empty()) {
        recordIdsByBatch_[packet.batchCode].push_back(recordId);
    }

    return recordId;
}

bool InMemoryTelemetryRepository::AttachReceipt(std::uint64_t recordId, const BlockchainReceipt& receipt) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto positionIt = positionById_.find(recordId);
    if (positionIt == positionById_.end()) {
        return false;
    }

    records_[positionIt->second].receipt = receipt;
    recordIdByTxHash_[receipt.txHash] = recordId;
    return true;
}

std::optional<TelemetryRecord> InMemoryTelemetryRepository::LatestByDevice(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto recordsIt = recordIdsByDevice_.find(deviceId);
    if (recordsIt == recordIdsByDevice_.end() || recordsIt->second.empty()) {
        return std::nullopt;
    }
    return FindByIdLocked(recordsIt->second.back());
}

std::optional<TelemetryRecord> InMemoryTelemetryRepository::FindByTransaction(const std::string& txHash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto txIt = recordIdByTxHash_.find(txHash);
    if (txIt == recordIdByTxHash_.end()) {
        return std::nullopt;
    }
    return FindByIdLocked(txIt->second);
}

std::vector<TelemetryRecord> InMemoryTelemetryRepository::FindByBatch(const std::string& batchCode) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TelemetryRecord> result;

    const auto batchIt = recordIdsByBatch_.find(batchCode);
    if (batchIt == recordIdsByBatch_.end()) {
        return result;
    }

    result.reserve(batchIt->second.size());
    for (const std::uint64_t recordId : batchIt->second) {
        const auto record = FindByIdLocked(recordId);
        if (record.has_value()) {
            result.push_back(*record);
        }
    }
    return result;
}

std::uint64_t InMemoryTelemetryRepository::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_.size();
}

std::optional<TelemetryRecord> InMemoryTelemetryRepository::FindByIdLocked(std::uint64_t recordId) const {
    const auto positionIt = positionById_.find(recordId);
    if (positionIt == positionById_.end()) {
        return std::nullopt;
    }
    return records_[positionIt->second];
}

}
