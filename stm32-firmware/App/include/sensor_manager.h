#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>

typedef struct {
    int32_t temperature_centi_c;
    uint32_t humidity_centi_pct;
    uint32_t co2_ppm;
    uint32_t soil_ph_centi;
} sensor_sample_t;

typedef struct {
    uint32_t sample_sequence;
} sensor_manager_t;

void sensor_manager_init(sensor_manager_t *manager);
int sensor_manager_read(sensor_manager_t *manager, sensor_sample_t *sample);

#endif
