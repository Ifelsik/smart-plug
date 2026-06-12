#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "esp_err.h"
#include "cJSON.h"
#include "modem.h"
#include "relay.h"
#include "mqtt.h"

// Инициализация обработчика команд. Передаём зависимости.
esp_err_t command_handler_init(modem_handle_t modem, relay_handle_t relay, mqtt_handle_t mqtt);

// Обработать команду. Совместимая сигнатура с mqtt_command_callback_t.
esp_err_t command_handler_process(const char *cmd, cJSON *payload);

#endif
