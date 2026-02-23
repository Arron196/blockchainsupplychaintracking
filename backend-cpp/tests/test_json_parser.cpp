#include <cassert>
#include <iostream>
#include <string>

#include "transport/json_parser.h"

namespace {

void TestParsesValidPayload() {
    const std::string payload =
        "{"
        "\"deviceId\":\"stm32-node-1\","
        "\"timestamp\":1700001000,"
        "\"telemetry\":{\"temperature\":24.5,\"humidity\":62.3},"
        "\"hash\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
        "\"signature\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\","
        "\"pubKeyId\":\"pubkey-1\","
        "\"transport\":\"wifi\","
        "\"batchCode\":\"BATCH-2026-0001\""
        "}";

    const agri::ParseTelemetryResult parsed = agri::ParseTelemetryPacketJson(payload);
    assert(parsed.ok);
    assert(parsed.packet.deviceId == "stm32-node-1");
    assert(parsed.packet.timestamp == 1700001000);
    assert(parsed.packet.telemetryJson == "{\"temperature\":24.5,\"humidity\":62.3}");
    assert(parsed.packet.pubKeyId == "pubkey-1");
    assert(parsed.packet.transport == "wifi");
    assert(parsed.packet.batchCode == "BATCH-2026-0001");
}

void TestRejectsMissingTelemetry() {
    const std::string payload =
        "{"
        "\"deviceId\":\"stm32-node-1\","
        "\"timestamp\":1700001000,"
        "\"hash\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
        "\"signature\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\""
        "}";

    const agri::ParseTelemetryResult parsed = agri::ParseTelemetryPacketJson(payload);
    assert(!parsed.ok);
    assert(parsed.error == "missing telemetry object");
}

}

int main() {
    TestParsesValidPayload();
    TestRejectsMissingTelemetry();
    std::cout << "test_json_parser passed" << std::endl;
    return 0;
}
