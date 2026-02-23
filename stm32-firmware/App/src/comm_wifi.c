#include "../include/comm_wifi.h"

comm_status_t comm_wifi_send(const uint8_t *payload, size_t payload_len) {
    if (payload == 0 || payload_len == 0U) {
        return COMM_STATUS_INVALID_ARGUMENT;
    }

    return COMM_STATUS_NOT_READY;
}

const char *comm_wifi_name(void) {
    return "wifi";
}
