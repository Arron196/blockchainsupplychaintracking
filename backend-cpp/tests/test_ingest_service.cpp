#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef AGRI_USE_OPENSSL
#define AGRI_USE_OPENSSL 0
#endif

#if AGRI_USE_OPENSSL && __has_include(<openssl/evp.h>) && __has_include(<openssl/pem.h>)
#define AGRI_TEST_OPENSSL_ENABLED 1
#include <openssl/evp.h>
#include <openssl/pem.h>
#else
#define AGRI_TEST_OPENSSL_ENABLED 0
#endif

#include "blockchain/blockchain_client.h"
#include "security/signature_verifier.h"
#include "services/ingest_service.h"
#include "storage/in_memory_telemetry_repository.h"
#include "storage/telemetry_repository.h"
#include "utils/hash_utils.h"

namespace {

constexpr const char* kTestPrivatePem =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEIBAhvHyy+MkYiKfJ6i80jbDZEzsDC8943UwQe5ZdPp+noAoGCCqGSM49\n"
    "AwEHoUQDQgAES4hVNSi27fHAishx1nXki+lfFdhrSVKT4ubhb9IbG9Kj3NYu14Mm\n"
    "VQKq13CS9jAYfnc/HDEzUHmJ9jSB3ZU2CA==\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr const char* kTestPublicPem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAES4hVNSi27fHAishx1nXki+lfFdhr\n"
    "SVKT4ubhb9IbG9Kj3NYu14MmVQKq13CS9jAYfnc/HDEzUHmJ9jSB3ZU2CA==\n"
    "-----END PUBLIC KEY-----\n";

std::string HexEncode(const unsigned char* bytes, std::size_t length) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char value = bytes[i];
        out.push_back(kHex[(value >> 4) & 0x0F]);
        out.push_back(kHex[value & 0x0F]);
    }
    return out;
}

#if AGRI_TEST_OPENSSL_ENABLED

EVP_PKEY* LoadPrivateKey() {
    BIO* bio = BIO_new_mem_buf(kTestPrivatePem, -1);
    if (bio == nullptr) {
        return nullptr;
    }
    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return key;
}

std::string SignHashHex(const std::string& hashHex) {
    EVP_PKEY* privateKey = LoadPrivateKey();
    if (privateKey == nullptr) {
        return "";
    }

    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    if (mdCtx == nullptr) {
        EVP_PKEY_free(privateKey);
        return "";
    }

    std::size_t signatureLength = 0;
    const int initOk = EVP_DigestSignInit(mdCtx, nullptr, EVP_sha256(), nullptr, privateKey);
    const int updateOk = EVP_DigestSignUpdate(mdCtx, hashHex.data(), hashHex.size());
    const int sizeOk = (initOk == 1 && updateOk == 1)
                           ? EVP_DigestSignFinal(mdCtx, nullptr, &signatureLength)
                           : 0;

    std::vector<unsigned char> signature(signatureLength, 0);
    const int signOk = (sizeOk == 1)
                           ? EVP_DigestSignFinal(mdCtx, signature.data(), &signatureLength)
                           : 0;

    EVP_MD_CTX_free(mdCtx);
    EVP_PKEY_free(privateKey);

    if (signOk != 1) {
        return "";
    }

    signature.resize(signatureLength);
    return HexEncode(signature.data(), signature.size());
}

#endif

std::string BuildSignature(const std::string& hashHex, const std::string& pubKeyId) {
#if AGRI_TEST_OPENSSL_ENABLED
    (void)pubKeyId;
    return SignHashHex(hashHex);
#else
    return hashHex + ":" + pubKeyId;
#endif
}

agri::PublicKeyMap BuildPublicKeys() {
    agri::PublicKeyMap keys;
    keys["pubkey-1"] = kTestPublicPem;
    return keys;
}

agri::TelemetryPacket MakeValidPacket() {
    agri::TelemetryPacket packet;
    packet.deviceId = "stm32-node-1";
    packet.timestamp = 1700001000;
    packet.telemetryJson = "{\"temperature\":24.5,\"humidity\":62.3}";
    packet.pubKeyId = "pubkey-1";
    packet.transport = "wifi";
    packet.batchCode = "BATCH-2026-0001";

    const std::string canonical =
        packet.deviceId + "|" + std::to_string(packet.timestamp) + "|" + packet.telemetryJson;
    packet.hashHex = agri::Sha256Hex(canonical);
    packet.signature = BuildSignature(packet.hashHex, packet.pubKeyId);
    return packet;
}

class ThrowingBlockchainClient final : public agri::BlockchainClient {
   public:
    agri::BlockchainReceipt SubmitHash(const std::string&, const std::string&, std::uint64_t) override {
        throw std::runtime_error("simulated blockchain outage");
    }
};

class AttachReceiptFailingRepository final : public agri::TelemetryRepository {
   public:
    explicit AttachReceiptFailingRepository(bool throwOnDelete)
        : throwOnDelete_(throwOnDelete) {}

    std::uint64_t Save(const agri::TelemetryPacket&) override {
        hasRecord_ = true;
        return 1;
    }

    bool AttachReceipt(std::uint64_t, const agri::BlockchainReceipt&) override {
        return false;
    }

    bool Delete(std::uint64_t) override {
        deleteCalled_ = true;
        if (throwOnDelete_) {
            throw std::runtime_error("simulated delete failure");
        }
        hasRecord_ = false;
        return true;
    }

    std::optional<agri::TelemetryRecord> LatestByDevice(const std::string&) const override {
        return std::nullopt;
    }

    std::optional<agri::TelemetryRecord> FindByTransaction(const std::string&) const override {
        return std::nullopt;
    }

    std::vector<agri::TelemetryRecord> FindByBatch(const std::string&) const override {
        return {};
    }

    std::uint64_t Size() const override {
        return hasRecord_ ? 1 : 0;
    }

    bool deleteCalled() const { return deleteCalled_; }

   private:
    bool throwOnDelete_{false};
    bool hasRecord_{false};
    bool deleteCalled_{false};
};

void TestAcceptsValidPacket() {
    agri::InMemoryTelemetryRepository repository;
    agri::BasicSignatureVerifier verifier(BuildPublicKeys());
    agri::MockBlockchainClient blockchain;
    agri::IngestService service(repository, verifier, blockchain);

    const agri::IngestResult result = service.Ingest(MakeValidPacket());
    assert(result.accepted);
    assert(result.recordId == 1);
    assert(result.receipt.has_value());
    assert(repository.Size() == 1);

    const agri::MetricsSnapshot metrics = service.GetMetricsSnapshot();
    assert(metrics.totalRequests == 1);
    assert(metrics.acceptedRequests == 1);
    assert(metrics.rejectedRequests == 0);
}

void TestRejectsHashMismatch() {
    agri::InMemoryTelemetryRepository repository;
    agri::BasicSignatureVerifier verifier(BuildPublicKeys());
    agri::MockBlockchainClient blockchain;
    agri::IngestService service(repository, verifier, blockchain);

    agri::TelemetryPacket packet = MakeValidPacket();
    packet.hashHex = agri::Sha256Hex("tampered");

    const agri::IngestResult result = service.Ingest(packet);
    assert(!result.accepted);
    assert(result.message == "hash mismatch with payload");
    assert(repository.Size() == 0);

    const agri::MetricsSnapshot metrics = service.GetMetricsSnapshot();
    assert(metrics.totalRequests == 1);
    assert(metrics.acceptedRequests == 0);
    assert(metrics.rejectedRequests == 1);
}

void TestRejectsInvalidSignature() {
    agri::InMemoryTelemetryRepository repository;
    agri::BasicSignatureVerifier verifier(BuildPublicKeys());
    agri::MockBlockchainClient blockchain;
    agri::IngestService service(repository, verifier, blockchain);

    agri::TelemetryPacket packet = MakeValidPacket();
    packet.signature = packet.signature + "00";

    const agri::IngestResult result = service.Ingest(packet);
    assert(!result.accepted);
    assert(result.message == "signature verification failed");
    assert(repository.Size() == 0);
}

void TestRollsBackStorageOnBlockchainFailure() {
    agri::InMemoryTelemetryRepository repository;
    agri::BasicSignatureVerifier verifier(BuildPublicKeys());
    ThrowingBlockchainClient blockchain;
    agri::IngestService service(repository, verifier, blockchain);

    const agri::IngestResult result = service.Ingest(MakeValidPacket());
    assert(!result.accepted);
    assert(result.message == "blockchain submit failed: simulated blockchain outage");
    assert(repository.Size() == 0);
}

void TestRollbackOnAttachReceiptFailure() {
    AttachReceiptFailingRepository repository(false);
    agri::BasicSignatureVerifier verifier(BuildPublicKeys());
    agri::MockBlockchainClient blockchain;
    agri::IngestService service(repository, verifier, blockchain);

    const agri::IngestResult result = service.Ingest(MakeValidPacket());
    assert(!result.accepted);
    assert(result.message == "receipt persistence failed after blockchain submit");
    assert(repository.Size() == 0);
    assert(repository.deleteCalled());
}

void TestRollbackFailureDoesNotMaskBlockchainError() {
    AttachReceiptFailingRepository repository(true);
    agri::BasicSignatureVerifier verifier(BuildPublicKeys());
    ThrowingBlockchainClient blockchain;
    agri::IngestService service(repository, verifier, blockchain);

    const agri::IngestResult result = service.Ingest(MakeValidPacket());
    assert(!result.accepted);
    assert(result.message ==
           "blockchain submit failed: simulated blockchain outage; rollback delete failed: simulated delete failure");
    assert(repository.deleteCalled());
}

}

int main() {
    TestAcceptsValidPacket();
    TestRejectsHashMismatch();
    TestRejectsInvalidSignature();
    TestRollsBackStorageOnBlockchainFailure();
    TestRollbackOnAttachReceiptFailure();
    TestRollbackFailureDoesNotMaskBlockchainError();
    std::cout << "test_ingest_service passed" << std::endl;
    return 0;
}
