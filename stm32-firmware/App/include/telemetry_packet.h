#ifndef TELEMETRY_PACKET_H
#define TELEMETRY_PACKET_H

#include <stddef.h>
#include <stdint.h>

#include "sensor_manager.h"

#define TELEMETRY_DEVICE_ID_MAX_LEN 16U
#define TELEMETRY_PUB_KEY_ID_MAX_LEN 16U
#define TELEMETRY_HASH_SIZE 32U
#define TELEMETRY_SIGNATURE_MAX_SIZE 72U
#define TELEMETRY_CANONICAL_MAX_LEN 160U

typedef struct {
    char device_id[TELEMETRY_DEVICE_ID_MAX_LEN + 1U];
    char pub_key_id[TELEMETRY_PUB_KEY_ID_MAX_LEN + 1U];
    uint32_t timestamp_sec;
    sensor_sample_t sample;
    uint8_t hash[TELEMETRY_HASH_SIZE];
    uint8_t signature[TELEMETRY_SIGNATURE_MAX_SIZE];
    size_t signature_len;
} telemetry_packet_t;

int telemetry_packet_init(
    telemetry_packet_t *packet,
    const char *device_id,
    const char *pub_key_id,
    uint32_t timestamp_sec,
    const sensor_sample_t *sample);

int telemetry_packet_canonicalize(
    const telemetry_packet_t *packet,
    char *output,
    size_t output_len,
    size_t *written_len);

int telemetry_packet_attach_hash(
    telemetry_packet_t *packet,
    const uint8_t hash[TELEMETRY_HASH_SIZE]);

int telemetry_packet_attach_signature(
    telemetry_packet_t *packet,
    const uint8_t *signature,
    size_t signature_len);

#endif
