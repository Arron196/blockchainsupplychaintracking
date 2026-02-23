#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../App/include/signer.h"
#include "../App/include/telemetry_packet.h"

static void bytes_to_hex(const uint8_t *input, size_t len, char *output) {
    static const char alphabet[] = "0123456789abcdef";
    size_t i;

    for (i = 0U; i < len; ++i) {
        output[2U * i] = alphabet[(input[i] >> 4U) & 0x0fU];
        output[(2U * i) + 1U] = alphabet[input[i] & 0x0fU];
    }
    output[2U * len] = '\0';
}

int main(void) {
    telemetry_packet_t packet;
    sensor_sample_t sample;
    char canonical[TELEMETRY_CANONICAL_MAX_LEN];
    char canonical_second[TELEMETRY_CANONICAL_MAX_LEN];
    size_t canonical_len;
    size_t canonical_second_len;
    uint8_t digest[SIGNER_SHA256_DIGEST_SIZE];
    char digest_hex[(SIGNER_SHA256_DIGEST_SIZE * 2U) + 1U];
    const char *expected_canonical =
        "deviceId=stm32-001;timestamp=1700000000;tempCenti=2356;humidityCenti=4512;co2ppm=604;soilPhCenti=678";
    const char *expected_sha256 =
        "d3cdc79dd1f29291d29b7988c8635f0f3ed3ff1c6ea47559e8faaf5889fa3def";

    sample.temperature_centi_c = 2356;
    sample.humidity_centi_pct = 4512;
    sample.co2_ppm = 604;
    sample.soil_ph_centi = 678;

    assert(telemetry_packet_init(
               &packet,
               "stm32-001",
               "ecc-slot-0",
               1700000000U,
               &sample) == 0);

    assert(telemetry_packet_canonicalize(
               &packet,
               canonical,
               sizeof(canonical),
               &canonical_len) == 0);
    assert(telemetry_packet_canonicalize(
               &packet,
               canonical_second,
               sizeof(canonical_second),
               &canonical_second_len) == 0);

    assert(canonical_len == strlen(expected_canonical));
    assert(canonical_second_len == canonical_len);
    assert(strcmp(canonical, expected_canonical) == 0);
    assert(strcmp(canonical_second, canonical) == 0);

    assert(signer_sha256((const uint8_t *)canonical, canonical_len, digest) == SIGNER_STATUS_OK);
    bytes_to_hex(digest, SIGNER_SHA256_DIGEST_SIZE, digest_hex);
    assert(strcmp(digest_hex, expected_sha256) == 0);

    return 0;
}
