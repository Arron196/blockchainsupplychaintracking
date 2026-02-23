#include "utils/hash_utils.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#if AGRI_USE_OPENSSL && __has_include(<openssl/evp.h>)
#include <openssl/evp.h>
#define AGRI_HASH_OPENSSL_ENABLED 1
#else
#define AGRI_HASH_OPENSSL_ENABLED 0
#endif

#if !AGRI_HASH_OPENSSL_ENABLED
#include <functional>
#endif

namespace agri {

namespace {

#if AGRI_HASH_OPENSSL_ENABLED
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
#endif

}

std::string Sha256Hex(std::string_view input) {
#if AGRI_HASH_OPENSSL_ENABLED
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        return std::string(64, '0');
    }

    unsigned char digest[EVP_MAX_MD_SIZE] = {0};
    unsigned int digestLength = 0;

    const int initOk = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    const int updateOk = EVP_DigestUpdate(ctx, input.data(), input.size());
    const int finalOk = EVP_DigestFinal_ex(ctx, digest, &digestLength);

    EVP_MD_CTX_free(ctx);

    if (initOk != 1 || updateOk != 1 || finalOk != 1) {
        return std::string(64, '0');
    }

    return HexEncode(digest, digestLength);
#else
    std::hash<std::string> hasher;
    const std::string text(input);
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << hasher(text + "|0")
           << std::hex << std::setw(16) << std::setfill('0') << hasher(text + "|1")
           << std::hex << std::setw(16) << std::setfill('0') << hasher(text + "|2")
           << std::hex << std::setw(16) << std::setfill('0') << hasher(text + "|3");
    return stream.str();
#endif
}

std::string CurrentUtcIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm utc{};
    gmtime_r(&nowTime, &utc);

    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

}
