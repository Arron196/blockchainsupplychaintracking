#include "services/ingest_service.h"

#include <chrono>
#include <exception>

#include "transport/json_parser.h"
#include "utils/hash_utils.h"

namespace agri {

IngestService::IngestService(
    TelemetryRepository& repository,
    const SignatureVerifier& signatureVerifier,
    BlockchainClient& blockchainClient)
    : repository_(repository),
      signatureVerifier_(signatureVerifier),
      blockchainClient_(blockchainClient) {}

IngestResult IngestService::Ingest(const TelemetryPacket& packet) {
    const auto begin = std::chrono::steady_clock::now();

    IngestResult result;
    auto finishWith = [&](bool accepted, const std::string& message) {
        const auto elapsed = std::chrono::steady_clock::now() - begin;
        const auto elapsedMs =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        result.accepted = accepted;
        result.message = message;
        result.processingMs = elapsedMs;
        if (accepted) {
            RecordAccepted(elapsedMs);
        } else {
            RecordRejected(elapsedMs);
        }
    };

    if (packet.deviceId.empty()) {
        finishWith(false, "deviceId is required");
        return result;
    }
    if (packet.timestamp == 0) {
        finishWith(false, "timestamp must be positive");
        return result;
    }
    if (packet.telemetryJson.empty()) {
        finishWith(false, "telemetry payload is required");
        return result;
    }
    if (!IsHex64(packet.hashHex)) {
        finishWith(false, "hash must be 64 hex characters");
        return result;
    }

    const std::string canonical =
        packet.deviceId + "|" + std::to_string(packet.timestamp) + "|" + packet.telemetryJson;
    const std::string expectedHash = Sha256Hex(canonical);
    if (packet.hashHex != expectedHash) {
        finishWith(false, "hash mismatch with payload");
        return result;
    }

    if (!signatureVerifier_.Verify(packet)) {
        finishWith(false, "signature verification failed");
        return result;
    }

    const std::uint64_t recordId = repository_.Save(packet);
    result.recordId = recordId;

    try {
        const BlockchainReceipt receipt =
            blockchainClient_.SubmitHash(packet.hashHex, packet.deviceId, packet.timestamp);
        if (!repository_.AttachReceipt(recordId, receipt)) {
            repository_.Delete(recordId);
            finishWith(false, "receipt persistence failed");
            return result;
        }
        result.receipt = receipt;
        finishWith(true, "accepted");
        return result;
    } catch (const std::exception& ex) {
        repository_.Delete(recordId);
        finishWith(false, std::string("blockchain submit failed: ") + ex.what());
        return result;
    } catch (...) {
        repository_.Delete(recordId);
        finishWith(false, "blockchain submit failed: unknown error");
        return result;
    }
}

MetricsSnapshot IngestService::GetMetricsSnapshot() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    MetricsSnapshot snapshot;
    snapshot.totalRequests = totalRequests_;
    snapshot.acceptedRequests = acceptedRequests_;
    snapshot.rejectedRequests = rejectedRequests_;
    snapshot.averageProcessingMs = (totalRequests_ == 0) ? 0 : (totalProcessingMs_ / totalRequests_);
    snapshot.repositorySize = repository_.Size();
    return snapshot;
}

void IngestService::RecordAccepted(std::uint64_t processingMs) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    ++totalRequests_;
    ++acceptedRequests_;
    totalProcessingMs_ += processingMs;
}

void IngestService::RecordRejected(std::uint64_t processingMs) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    ++totalRequests_;
    ++rejectedRequests_;
    totalProcessingMs_ += processingMs;
}

}
