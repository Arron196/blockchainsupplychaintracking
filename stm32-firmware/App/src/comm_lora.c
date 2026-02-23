#include "../include/comm_lora.h"

comm_status_t comm_lora_send(const uint8_t *payload, size_t payload_len) {
    if (payload == 0 || payload_len == 0U) {
        return COMM_STATUS_INVALID_ARGUMENT;
    }

    return COMM_STATUS_NOT_READY;
}

const char *comm_lora_name(void) {
    return "lora";
}
