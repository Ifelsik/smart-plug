#ifndef MODEM_H
#define MODEM_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

typedef struct modem_ctx_t* modem_handle_t;

typedef struct {
    int pin_rx;
    int pin_tx;
    int pin_cts;
    int pin_rts;
    const char *apn;
} modem_config_t;

modem_handle_t modem_driver_init(const modem_config_t *config);

esp_err_t modem_driver_start_network(modem_handle_t handle, uint32_t timeout);

esp_err_t modem_driver_get_signal_quality(modem_handle_t handle, int *rssi);

void modem_driver_destroy(modem_handle_t handle);

#endif