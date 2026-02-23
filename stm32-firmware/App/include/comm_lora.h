#ifndef COMM_LORA_H
#define COMM_LORA_H

#include <stddef.h>
#include <stdint.h>

#include "comm_status.h"

comm_status_t comm_lora_send(const uint8_t *payload, size_t payload_len);
const char *comm_lora_name(void);

#endif
