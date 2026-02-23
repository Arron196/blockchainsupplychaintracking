#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "domain/telemetry_packet.h"

namespace agri {

struct BlockchainReceipt {
    std::string txHash;
    std::uint64_t blockHeight{0};
    std::string submittedAtIso8601;
};

struct TelemetryRecord {
    std::uint64_t recordId{0};
    TelemetryPacket packet;
    std::optional<BlockchainReceipt> receipt;
};

}
