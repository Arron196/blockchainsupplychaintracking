#ifndef COMM_WIFI_H
#define COMM_WIFI_H

#include <stddef.h>
#include <stdint.h>

#include "comm_status.h"

comm_status_t comm_wifi_send(const uint8_t *payload, size_t payload_len);
const char *comm_wifi_name(void);

#endif
