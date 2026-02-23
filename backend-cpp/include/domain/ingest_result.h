#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "domain/telemetry_record.h"

namespace agri {

struct IngestResult {
    bool accepted{false};
    std::string message;
    std::uint64_t recordId{0};
    std::optional<BlockchainReceipt> receipt;
    std::uint64_t processingMs{0};
};

}
