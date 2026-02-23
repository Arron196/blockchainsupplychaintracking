#pragma once

#include <mutex>

#include "blockchain/blockchain_client.h"
#include "domain/ingest_result.h"
#include "domain/metrics_snapshot.h"
#include "security/signature_verifier.h"
#include "storage/telemetry_repository.h"

namespace agri {

class IngestService {
   public:
    IngestService(
        TelemetryRepository& repository,
        const SignatureVerifier& signatureVerifier,
        BlockchainClient& blockchainClient);

    IngestResult Ingest(const TelemetryPacket& packet);
    MetricsSnapshot GetMetricsSnapshot() const;

   private:
    void RecordAccepted(std::uint64_t processingMs);
    void RecordRejected(std::uint64_t processingMs);

    TelemetryRepository& repository_;
    const SignatureVerifier& signatureVerifier_;
    BlockchainClient& blockchainClient_;

    mutable std::mutex metricsMutex_;
    std::uint64_t totalRequests_{0};
    std::uint64_t acceptedRequests_{0};
    std::uint64_t rejectedRequests_{0};
    std::uint64_t totalProcessingMs_{0};
};

}
