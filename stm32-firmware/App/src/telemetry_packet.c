#include "../include/telemetry_packet.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int telemetry_packet_init(
    telemetry_packet_t *packet,
    const char *device_id,
    const char *pub_key_id,
    uint32_t timestamp_sec,
    const sensor_sample_t *sample) {
    size_t device_id_len;
    size_t pub_key_id_len;

    if (packet == 0 || device_id == 0 || pub_key_id == 0 || sample == 0) {
        return -1;
    }

    device_id_len = strlen(device_id);
    pub_key_id_len = strlen(pub_key_id);
    if (device_id_len == 0U ||
        pub_key_id_len == 0U ||
        device_id_len > TELEMETRY_DEVICE_ID_MAX_LEN ||
        pub_key_id_len > TELEMETRY_PUB_KEY_ID_MAX_LEN) {
        return -1;
    }

    memset(packet, 0, sizeof(*packet));
    memcpy(packet->device_id, device_id, device_id_len);
    memcpy(packet->pub_key_id, pub_key_id, pub_key_id_len);
    packet->timestamp_sec = timestamp_sec;
    packet->sample = *sample;

    return 0;
}

int telemetry_packet_canonicalize(
    const telemetry_packet_t *packet,
    char *output,
    size_t output_len,
    size_t *written_len) {
    int write_count;

    if (packet == 0 || output == 0 || output_len == 0U || written_len == 0) {
        return -1;
    }

    write_count = snprintf(
        output,
        output_len,
        "%s|%" PRIu32
        "|{\"temperatureCenti\":%" PRId32
        ",\"humidityCenti\":%" PRIu32
        ",\"co2ppm\":%" PRIu32
        ",\"soilPhCenti\":%" PRIu32 "}",
        packet->device_id,
        packet->timestamp_sec,
        packet->sample.temperature_centi_c,
        packet->sample.humidity_centi_pct,
        packet->sample.co2_ppm,
        packet->sample.soil_ph_centi);

    if (write_count < 0 || (size_t)write_count >= output_len) {
        return -1;
    }

    *written_len = (size_t)write_count;
    return 0;
}

int telemetry_packet_attach_hash(
    telemetry_packet_t *packet,
    const uint8_t hash[TELEMETRY_HASH_SIZE]) {
    if (packet == 0 || hash == 0) {
        return -1;
    }

    memcpy(packet->hash, hash, TELEMETRY_HASH_SIZE);
    return 0;
}

int telemetry_packet_attach_signature(
    telemetry_packet_t *packet,
    const uint8_t *signature,
    size_t signature_len) {
    if (packet == 0 || signature_len > TELEMETRY_SIGNATURE_MAX_SIZE) {
        return -1;
    }

    if (signature_len > 0U && signature == 0) {
        return -1;
    }

    memset(packet->signature, 0, sizeof(packet->signature));
    if (signature_len > 0U) {
        memcpy(packet->signature, signature, signature_len);
    }
    packet->signature_len = signature_len;

    return 0;
}
