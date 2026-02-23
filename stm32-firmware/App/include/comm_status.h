#ifndef COMM_STATUS_H
#define COMM_STATUS_H

typedef enum {
    COMM_STATUS_OK = 0,
    COMM_STATUS_INVALID_ARGUMENT = -1,
    COMM_STATUS_NOT_READY = -2,
    COMM_STATUS_IO_ERROR = -3
} comm_status_t;

#endif
