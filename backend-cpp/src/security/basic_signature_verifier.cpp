#include "security/signature_verifier.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef AGRI_USE_OPENSSL
#define AGRI_USE_OPENSSL 0
#endif

#if AGRI_USE_OPENSSL && __has_include(<openssl/evp.h>) && __has_include(<openssl/pem.h>)
#define AGRI_OPENSSL_ENABLED 1
#include <openssl/evp.h>
#include <openssl/pem.h>
#else
#define AGRI_OPENSSL_ENABLED 0
#endif

#include "transport/json_parser.h"

namespace fs = std::filesystem;

namespace agri {

namespace {

std::string ReadTextFile(const fs::path& filePath) {
    std::ifstream input(filePath);
    if (!input.is_open()) {
        return "";
    }

    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

int HexNibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower >= 'a' && lower <= 'f') {
        return 10 + (lower - 'a');
    }
    return -1;
}

std::optional<std::vector<unsigned char>> DecodeHex(std::string_view hex) {
    if (hex.size() % 2 != 0) {
        return std::nullopt;
    }

    std::vector<unsigned char> bytes(hex.size() / 2, 0);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const int hi = HexNibble(hex[2 * i]);
        const int lo = HexNibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        bytes[i] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return bytes;
}

#if AGRI_OPENSSL_ENABLED

EVP_PKEY* ReadPublicKeyFromPem(const std::string& pemText) {
    BIO* bio = BIO_new_mem_buf(pemText.data(), static_cast<int>(pemText.size()));
    if (bio == nullptr) {
        return nullptr;
    }

    EVP_PKEY* key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return key;
}

#endif

}

PublicKeyMap LoadPublicKeysFromDirectory(const std::string& directoryPath) {
    PublicKeyMap keys;
    const fs::path root(directoryPath);

    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return keys;
    }

    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }

        const fs::path filePath = entry.path();
        const std::string extension = filePath.extension().string();
        if (extension != ".pem" && extension != ".pub") {
            continue;
        }

        const std::string keyId = filePath.stem().string();
        if (keyId.empty()) {
            continue;
        }

        const std::string pem = ReadTextFile(filePath);
        if (!pem.empty()) {
            keys[keyId] = pem;
        }
    }

    return keys;
}

BasicSignatureVerifier::BasicSignatureVerifier(PublicKeyMap publicKeys)
    : publicKeys_(std::move(publicKeys)) {}

bool BasicSignatureVerifier::Verify(const TelemetryPacket& packet) const {
    if (packet.deviceId.empty() || packet.pubKeyId.empty()) {
        return false;
    }
    if (!IsHex64(packet.hashHex)) {
        return false;
    }
    if (packet.signature.size() < 16) {
        return false;
    }

    const auto keyIt = publicKeys_.find(packet.pubKeyId);
    if (keyIt == publicKeys_.end()) {
        return false;
    }

#if AGRI_OPENSSL_ENABLED
    EVP_PKEY* publicKey = ReadPublicKeyFromPem(keyIt->second);
    if (publicKey == nullptr) {
        return false;
    }

    const auto signatureBytes = DecodeHex(packet.signature);
    if (!signatureBytes.has_value()) {
        EVP_PKEY_free(publicKey);
        return false;
    }

    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    if (mdCtx == nullptr) {
        EVP_PKEY_free(publicKey);
        return false;
    }

    const int initOk = EVP_DigestVerifyInit(mdCtx, nullptr, EVP_sha256(), nullptr, publicKey);
    const int updateOk = EVP_DigestVerifyUpdate(mdCtx, packet.hashHex.data(), packet.hashHex.size());
    const int verifyOk = (initOk == 1 && updateOk == 1)
                             ? EVP_DigestVerifyFinal(mdCtx, signatureBytes->data(), signatureBytes->size())
                             : 0;

    EVP_MD_CTX_free(mdCtx);
    EVP_PKEY_free(publicKey);
    return verifyOk == 1;
#else
    return packet.signature == packet.hashHex + ":" + packet.pubKeyId;
#endif
}

}
