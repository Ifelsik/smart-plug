#ifndef MQTT_H
#define MQTT_H

#include "app.h"
#include "esp_err.h"
#include "cJSON.h"

typedef struct mqtt_ctx_t* mqtt_handle_t;

typedef esp_err_t (*mqtt_command_callback_t)(const char *cmd, cJSON *payload);

mqtt_handle_t mqtt_app_start(const app_config_t *config);
esp_err_t mqtt_app_publish(mqtt_handle_t handle, const char *topic, const char *data);
void mqtt_set_command_callback(mqtt_handle_t handle, mqtt_command_callback_t cb);
esp_err_t mqtt_command_processor(const char *topic, int topic_len, const char *data, int data_len);

// helper to form JSON response; caller must free returned string with free()
char* form_json_response(const char *cmd, const char *result, const char *desc);

// Геттер для плучения топика команд. Может вернуть NULL, если топик неизвестен.
const char* mqtt_get_cmd_topic(mqtt_handle_t handle);

// Геттер для получения топика ответов. Возвращает NULL, если топик неизвестен.
const char* mqtt_get_resp_topic(mqtt_handle_t handle);

#endif