#pragma once

#include <cstdint>

namespace agri {

struct MetricsSnapshot {
    std::uint64_t totalRequests{0};
    std::uint64_t acceptedRequests{0};
    std::uint64_t rejectedRequests{0};
    std::uint64_t averageProcessingMs{0};
    std::uint64_t repositorySize{0};
};

}
