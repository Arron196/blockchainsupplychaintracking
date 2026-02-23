#include "transport/json_parser.h"

#include <cstdint>
#include <cctype>
#include <optional>
#include <regex>
#include <string>

namespace agri {

namespace {

std::optional<std::string> ExtractStringValue(std::string_view json, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_search(json.begin(), json.end(), match, pattern)) {
        return std::nullopt;
    }
    return std::string(match[1].first, match[1].second);
}

std::optional<std::uint64_t> ExtractUnsignedValue(std::string_view json, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*([0-9]+)");
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_search(json.begin(), json.end(), match, pattern)) {
        return std::nullopt;
    }
    try {
        return std::stoull(std::string(match[1].first, match[1].second));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> ExtractObjectValue(std::string_view json, const std::string& key) {
    const std::string keyToken = "\"" + key + "\"";
    const std::size_t keyPos = json.find(keyToken);
    if (keyPos == std::string_view::npos) {
        return std::nullopt;
    }

    const std::size_t colonPos = json.find(':', keyPos + keyToken.size());
    if (colonPos == std::string_view::npos) {
        return std::nullopt;
    }

    const std::size_t objectStart = json.find('{', colonPos + 1);
    if (objectStart == std::string_view::npos) {
        return std::nullopt;
    }

    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (std::size_t i = objectStart; i < json.size(); ++i) {
        const char c = json[i];

        if (escape) {
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            continue;
        }
        if (c == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }

        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                return std::string(json.substr(objectStart, i - objectStart + 1));
            }
        }
    }

    return std::nullopt;
}

}

ParseTelemetryResult ParseTelemetryPacketJson(std::string_view payload) {
    ParseTelemetryResult result;

    const auto deviceId = ExtractStringValue(payload, "deviceId");
    if (!deviceId.has_value()) {
        result.error = "missing deviceId";
        return result;
    }

    const auto timestamp = ExtractUnsignedValue(payload, "timestamp");
    if (!timestamp.has_value()) {
        result.error = "missing timestamp";
        return result;
    }

    const auto telemetry = ExtractObjectValue(payload, "telemetry");
    if (!telemetry.has_value()) {
        result.error = "missing telemetry object";
        return result;
    }

    const auto hashHex = ExtractStringValue(payload, "hash");
    if (!hashHex.has_value()) {
        result.error = "missing hash";
        return result;
    }

    const auto signature = ExtractStringValue(payload, "signature");
    if (!signature.has_value()) {
        result.error = "missing signature";
        return result;
    }

    result.packet.deviceId = *deviceId;
    result.packet.timestamp = *timestamp;
    result.packet.telemetryJson = *telemetry;
    result.packet.hashHex = *hashHex;
    result.packet.signature = *signature;
    result.packet.pubKeyId = ExtractStringValue(payload, "pubKeyId").value_or("default-pubkey");
    result.packet.transport = ExtractStringValue(payload, "transport").value_or("wifi");
    result.packet.batchCode = ExtractStringValue(payload, "batchCode").value_or("");

    result.ok = true;
    return result;
}

bool IsHex64(std::string_view value) {
    if (value.size() != 64) {
        return false;
    }
    for (const char c : value) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

std::string JsonEscape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char c : value) {
        switch (c) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += c;
                break;
        }
    }
    return escaped;
}

}
