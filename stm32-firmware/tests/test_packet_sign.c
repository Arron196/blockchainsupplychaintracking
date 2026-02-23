#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../App/include/app_main.h"
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

static void test_deterministic_canonical_and_hash(void) {
    telemetry_packet_t packet;
    sensor_sample_t sample;
    char canonical[TELEMETRY_CANONICAL_MAX_LEN];
    char canonical_second[TELEMETRY_CANONICAL_MAX_LEN];
    size_t canonical_len;
    size_t canonical_second_len;
    uint8_t digest[SIGNER_SHA256_DIGEST_SIZE];
    char digest_hex[(SIGNER_SHA256_DIGEST_SIZE * 2U) + 1U];
    const char *expected_canonical =
        "stm32-001|1700000000|{\"temperatureCenti\":2356,\"humidityCenti\":4512,\"co2ppm\":604,\"soilPhCenti\":678}";
    const char *expected_sha256 =
        "b71d6654c6ee410ae230564df2118d43b49539fc55c227aa441514c0bebadcaa";

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
}

static void test_error_paths(void) {
    telemetry_packet_t packet;
    sensor_sample_t sample;
    char canonical[16];
    size_t written_len;

    memset(&sample, 0, sizeof(sample));

    assert(telemetry_packet_init(0, "dev", "key", 1U, &sample) == -1);
    assert(telemetry_packet_init(&packet, "", "key", 1U, &sample) == -1);
    assert(telemetry_packet_init(&packet, "dev", "", 1U, &sample) == -1);
    assert(telemetry_packet_init(&packet, "dev", "key", 1U, &sample) == 0);

    assert(telemetry_packet_canonicalize(&packet, 0, sizeof(canonical), &written_len) == -1);
    assert(telemetry_packet_canonicalize(&packet, canonical, 1U, &written_len) == -1);
    assert(telemetry_packet_canonicalize(&packet, canonical, sizeof(canonical), 0) == -1);

    assert(telemetry_packet_attach_hash(0, packet.hash) == -1);
    assert(telemetry_packet_attach_hash(&packet, 0) == -1);

    assert(telemetry_packet_attach_signature(0, 0, 0U) == -1);
    assert(telemetry_packet_attach_signature(&packet, 0, TELEMETRY_SIGNATURE_MAX_SIZE + 1U) == -1);
    assert(telemetry_packet_attach_signature(&packet, 0, 4U) == -1);
}

static void test_timestamp_advances_only_on_success(void) {
    app_main_t state;
    telemetry_packet_t packet;
    size_t canonical_len;
    char too_small[8];
    char canonical[TELEMETRY_CANONICAL_MAX_LEN];

    app_main_init(&state, APP_TRANSPORT_WIFI, 5000U);

    assert(app_main_prepare_packet(
               &state,
               &packet,
               too_small,
               sizeof(too_small),
               &canonical_len) == APP_MAIN_STATUS_PACKET_FAILURE);
    assert(state.next_timestamp_sec == 5000U);

    assert(app_main_prepare_packet(
               &state,
               &packet,
               canonical,
               sizeof(canonical),
               &canonical_len) == APP_MAIN_STATUS_OK);
    assert(state.next_timestamp_sec == 5001U);
}

int main(void) {
    test_deterministic_canonical_and_hash();
    test_error_paths();
    test_timestamp_advances_only_on_success();

    return 0;
}
