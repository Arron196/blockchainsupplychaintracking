#pragma once

#include <string>
#include <unordered_map>

#include "domain/telemetry_packet.h"

namespace agri {

using PublicKeyMap = std::unordered_map<std::string, std::string>;

PublicKeyMap LoadPublicKeysFromDirectory(const std::string& directoryPath);

class SignatureVerifier {
   public:
    virtual ~SignatureVerifier() = default;
    virtual bool Verify(const TelemetryPacket& packet) const = 0;
};

class BasicSignatureVerifier final : public SignatureVerifier {
   public:
    explicit BasicSignatureVerifier(PublicKeyMap publicKeys);

    bool Verify(const TelemetryPacket& packet) const override;

   private:
    PublicKeyMap publicKeys_;
};

}
