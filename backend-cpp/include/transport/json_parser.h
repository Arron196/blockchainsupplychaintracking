#pragma once

#include <string>
#include <string_view>

#include "domain/telemetry_packet.h"

namespace agri {

struct ParseTelemetryResult {
    bool ok{false};
    TelemetryPacket packet;
    std::string error;
};

ParseTelemetryResult ParseTelemetryPacketJson(std::string_view payload);
bool IsHex64(std::string_view value);
std::string JsonEscape(std::string_view value);

}
