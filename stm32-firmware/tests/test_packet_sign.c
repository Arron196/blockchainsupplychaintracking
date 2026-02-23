#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../App/include/app_main.h"
#include "../App/include/comm_lora.h"
#include "../App/include/comm_wifi.h"
#include "../App/include/sensor_manager.h"
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
    /*
     * SHA-256 for expected_canonical verified independently with:
     * printf '%s' "stm32-001|1700000000|{\"temperatureCenti\":2356,\"humidityCenti\":4512,\"co2ppm\":604,\"soilPhCenti\":678}" | sha256sum
     */
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
    sample.temperature_centi_c = 1234;
    sample.humidity_centi_pct = 5678;
    sample.co2_ppm = 910;
    sample.soil_ph_centi = 345;

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

    sample.temperature_centi_c = 9000;
    assert(telemetry_packet_init(&packet, "dev", "key", 1U, &sample) == -1);
    sample.temperature_centi_c = 1200;
    sample.humidity_centi_pct = 12000;
    assert(telemetry_packet_init(&packet, "dev", "key", 1U, &sample) == -1);
    sample.humidity_centi_pct = 6000;
    sample.soil_ph_centi = 1900;
    assert(telemetry_packet_init(&packet, "dev", "key", 1U, &sample) == -1);
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

static int mock_sign_digest(
    const uint8_t digest[SIGNER_SHA256_DIGEST_SIZE],
    uint8_t *signature,
    size_t *signature_len,
    void *context) {
    size_t i;
    uint32_t *call_count = (uint32_t *)context;

    if (call_count != 0) {
        *call_count += 1U;
    }

    if (*signature_len < SIGNER_SHA256_DIGEST_SIZE) {
        return -1;
    }

    for (i = 0U; i < SIGNER_SHA256_DIGEST_SIZE; ++i) {
        signature[i] = digest[i];
    }
    *signature_len = SIGNER_SHA256_DIGEST_SIZE;
    return 0;
}

static void test_signer_backend_integration(void) {
    signer_backend_t backend;
    uint8_t digest[SIGNER_SHA256_DIGEST_SIZE];
    uint8_t signature[SIGNER_ECDSA_SIGNATURE_MAX_SIZE];
    size_t signature_len;
    uint32_t call_count;
    size_t i;

    for (i = 0U; i < SIGNER_SHA256_DIGEST_SIZE; ++i) {
        digest[i] = (uint8_t)i;
    }

    signer_init();
    signature_len = sizeof(signature);
    assert(signer_sign_digest(digest, signature, &signature_len) == SIGNER_STATUS_BACKEND_NOT_CONFIGURED);

    call_count = 0U;
    backend.sign_digest = mock_sign_digest;
    backend.context = &call_count;
    signer_set_backend(backend);

    signature_len = sizeof(signature);
    assert(signer_sign_digest(digest, signature, &signature_len) == SIGNER_STATUS_OK);
    assert(signature_len == SIGNER_SHA256_DIGEST_SIZE);
    assert(call_count == 1U);
    assert(memcmp(signature, digest, SIGNER_SHA256_DIGEST_SIZE) == 0);

    signer_init();
    signature_len = sizeof(signature);
    assert(signer_sign_digest(digest, signature, &signature_len) == SIGNER_STATUS_BACKEND_NOT_CONFIGURED);
}

static void test_sensor_manager_and_transport_stubs(void) {
    sensor_manager_t manager;
    sensor_sample_t first;
    sensor_sample_t second;
    uint8_t payload[3] = {0x01U, 0x02U, 0x03U};

    assert(sensor_manager_read(0, &first) == -1);
    assert(sensor_manager_read(&manager, 0) == -1);

    sensor_manager_init(&manager);
    assert(sensor_manager_read(&manager, &first) == 0);
    assert(sensor_manager_read(&manager, &second) == 0);
    assert(second.temperature_centi_c != first.temperature_centi_c);
    assert(second.humidity_centi_pct != first.humidity_centi_pct);

    assert(comm_wifi_send(0, 0U) == COMM_STATUS_INVALID_ARGUMENT);
    assert(comm_lora_send(0, 0U) == COMM_STATUS_INVALID_ARGUMENT);
    assert(comm_wifi_send(payload, sizeof(payload)) == COMM_STATUS_NOT_READY);
    assert(comm_lora_send(payload, sizeof(payload)) == COMM_STATUS_NOT_READY);
    assert(strcmp(comm_wifi_name(), "wifi") == 0);
    assert(strcmp(comm_lora_name(), "lora") == 0);
}

int main(void) {
    test_deterministic_canonical_and_hash();
    test_error_paths();
    test_timestamp_advances_only_on_success();
    test_signer_backend_integration();
    test_sensor_manager_and_transport_stubs();

    return 0;
}
