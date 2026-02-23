#include "../include/sensor_manager.h"

void sensor_manager_init(sensor_manager_t *manager) {
    if (manager == 0) {
        return;
    }

    manager->sample_sequence = 0U;
}

int sensor_manager_read(sensor_manager_t *manager, sensor_sample_t *sample) {
    if (manager == 0 || sample == 0) {
        return -1;
    }

    sample->temperature_centi_c = 2300 + (int32_t)(manager->sample_sequence % 40U);
    sample->humidity_centi_pct = 4500 + (manager->sample_sequence % 70U);
    sample->co2_ppm = 600 + (manager->sample_sequence % 15U);
    sample->soil_ph_centi = 670 + (manager->sample_sequence % 10U);

    manager->sample_sequence += 1U;
    return 0;
}
