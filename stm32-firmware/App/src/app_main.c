#include "../include/app_main.h"

#include "../include/comm_lora.h"
#include "../include/comm_wifi.h"
#include "../include/signer.h"

static const char k_default_device_id[] = "stm32-001";
static const char k_default_pub_key_id[] = "ecc-slot-0";

void app_main_init(
    app_main_t *state,
    app_transport_t transport,
    uint32_t start_timestamp_sec) {
    if (state == 0) {
        return;
    }

    sensor_manager_init(&state->sensor_manager);
    state->transport = transport;
    state->next_timestamp_sec = start_timestamp_sec;

    signer_init();
}

app_main_status_t app_main_prepare_packet(
    app_main_t *state,
    telemetry_packet_t *packet,
    char *canonical_output,
    size_t canonical_output_len,
    size_t *canonical_written_len) {
    sensor_sample_t sample;
    uint8_t digest[SIGNER_SHA256_DIGEST_SIZE];
    uint8_t signature[SIGNER_ECDSA_SIGNATURE_MAX_SIZE];
    size_t signature_len;
    signer_status_t signer_result;

    if (state == 0 ||
        packet == 0 ||
        canonical_output == 0 ||
        canonical_output_len == 0U ||
        canonical_written_len == 0) {
        return APP_MAIN_STATUS_INVALID_ARGUMENT;
    }

    if (sensor_manager_read(&state->sensor_manager, &sample) != 0) {
        return APP_MAIN_STATUS_SENSOR_FAILURE;
    }

    if (telemetry_packet_init(
            packet,
            k_default_device_id,
            k_default_pub_key_id,
            state->next_timestamp_sec,
            &sample) != 0) {
        return APP_MAIN_STATUS_PACKET_FAILURE;
    }

    if (telemetry_packet_canonicalize(
            packet,
            canonical_output,
            canonical_output_len,
            canonical_written_len) != 0) {
        return APP_MAIN_STATUS_PACKET_FAILURE;
    }

    if (signer_sha256((const uint8_t *)canonical_output, *canonical_written_len, digest) !=
        SIGNER_STATUS_OK) {
        return APP_MAIN_STATUS_HASH_FAILURE;
    }

    if (telemetry_packet_attach_hash(packet, digest) != 0) {
        return APP_MAIN_STATUS_PACKET_FAILURE;
    }

    signature_len = sizeof(signature);
    signer_result = signer_sign_digest(digest, signature, &signature_len);
    if (signer_result == SIGNER_STATUS_OK) {
        if (telemetry_packet_attach_signature(packet, signature, signature_len) != 0) {
            return APP_MAIN_STATUS_PACKET_FAILURE;
        }
    } else if (signer_result == SIGNER_STATUS_BACKEND_NOT_CONFIGURED) {
        if (telemetry_packet_attach_signature(packet, 0, 0U) != 0) {
            return APP_MAIN_STATUS_PACKET_FAILURE;
        }
    } else {
        return APP_MAIN_STATUS_SIGN_FAILURE;
    }

    state->next_timestamp_sec += 1U;

    return APP_MAIN_STATUS_OK;
}

comm_status_t app_main_send_canonical(
    const app_main_t *state,
    const char *payload,
    size_t payload_len) {
    if (state == 0 || payload == 0 || payload_len == 0U) {
        return COMM_STATUS_INVALID_ARGUMENT;
    }

    if (state->transport == APP_TRANSPORT_WIFI) {
        return comm_wifi_send((const uint8_t *)payload, payload_len);
    }

    return comm_lora_send((const uint8_t *)payload, payload_len);
}
