#pragma once

#include <cstdint>
#include <string>

namespace agri {

struct TelemetryPacket {
    std::string deviceId;
    std::uint64_t timestamp{0};
    std::string telemetryJson;
    std::string hashHex;
    std::string signature;
    std::string pubKeyId;
    std::string transport;
    std::string batchCode;
};

}
