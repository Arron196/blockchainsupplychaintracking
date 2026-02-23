#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stddef.h>
#include <stdint.h>

#include "comm_status.h"
#include "sensor_manager.h"
#include "telemetry_packet.h"

typedef enum {
    APP_TRANSPORT_WIFI = 0,
    APP_TRANSPORT_LORA = 1
} app_transport_t;

typedef enum {
    APP_MAIN_STATUS_OK = 0,
    APP_MAIN_STATUS_INVALID_ARGUMENT = -1,
    APP_MAIN_STATUS_SENSOR_FAILURE = -2,
    APP_MAIN_STATUS_PACKET_FAILURE = -3,
    APP_MAIN_STATUS_HASH_FAILURE = -4,
    APP_MAIN_STATUS_SIGN_FAILURE = -5
} app_main_status_t;

typedef struct {
    sensor_manager_t sensor_manager;
    app_transport_t transport;
    uint32_t next_timestamp_sec;
} app_main_t;

void app_main_init(
    app_main_t *state,
    app_transport_t transport,
    uint32_t start_timestamp_sec);

app_main_status_t app_main_prepare_packet(
    app_main_t *state,
    telemetry_packet_t *packet,
    char *canonical_output,
    size_t canonical_output_len,
    size_t *canonical_written_len);

comm_status_t app_main_send_canonical(
    app_transport_t transport,
    const char *payload,
    size_t payload_len);

#endif
