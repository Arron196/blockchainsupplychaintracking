#include "storage/in_memory_telemetry_repository.h"

#include <algorithm>

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

bool InMemoryTelemetryRepository::Delete(std::uint64_t recordId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto positionIt = positionById_.find(recordId);
    if (positionIt == positionById_.end()) {
        return false;
    }

    const std::size_t position = positionIt->second;
    const TelemetryRecord& record = records_[position];

    auto eraseRecordId = [recordId](std::vector<std::uint64_t>* ids) {
        ids->erase(std::remove(ids->begin(), ids->end(), recordId), ids->end());
    };

    if (const auto deviceIt = recordIdsByDevice_.find(record.packet.deviceId); deviceIt != recordIdsByDevice_.end()) {
        eraseRecordId(&deviceIt->second);
        if (deviceIt->second.empty()) {
            recordIdsByDevice_.erase(deviceIt);
        }
    }

    if (!record.packet.batchCode.empty()) {
        if (const auto batchIt = recordIdsByBatch_.find(record.packet.batchCode); batchIt != recordIdsByBatch_.end()) {
            eraseRecordId(&batchIt->second);
            if (batchIt->second.empty()) {
                recordIdsByBatch_.erase(batchIt);
            }
        }
    }

    if (record.receipt.has_value()) {
        recordIdByTxHash_.erase(record.receipt->txHash);
    }

    records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(position));
    positionById_.erase(positionIt);

    for (std::size_t i = position; i < records_.size(); ++i) {
        positionById_[records_[i].recordId] = i;
    }

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
