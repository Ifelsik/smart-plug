#ifndef APP_H
#define APP_H

#include "relay.h"
#include "modem.h"

typedef struct mqtt_config_t {
    const char *uri;
    const char *cmd_topic;
    const char *resp_topic;
} mqtt_config_t;

typedef struct app_config_t {
    modem_config_t modem_config;
    mqtt_config_t mqtt_config;
    int modem_pwr_key_pin;
    int dc_dc_enable_pin;
    int relay_pin;
    int indicator_pin;
} app_config_t;

void app_start(const app_config_t *config);

#endif