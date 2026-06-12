#include "command_handler.h"

#include "esp_log.h"
#include "esp_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "CMD_HANDLER";

static modem_handle_t g_modem = NULL;
static relay_handle_t g_relay = NULL;
static mqtt_handle_t g_mqtt = NULL;

esp_err_t command_handler_init(modem_handle_t modem, relay_handle_t relay, mqtt_handle_t mqtt) {
    if (modem == NULL || relay == NULL || mqtt == NULL) {
        ESP_LOGE(TAG, "command_handler_init: missing dependency");
        return ESP_ERR_INVALID_ARG;
    }
    g_modem = modem;
    g_relay = relay;
    g_mqtt = mqtt;
    return ESP_OK;
}

static esp_err_t publish_response(const char *cmd, const char *result, const char *desc) {
    if (g_mqtt == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *resp_topic = mqtt_get_resp_topic(g_mqtt);
    const char *cmd_topic = mqtt_get_cmd_topic(g_mqtt);
    char topic_buf[128];

    // вот этот момент странно выглядит
    // почему тут в разные топики пытаемся публиковать?
    if (resp_topic) {
        snprintf(topic_buf, sizeof(topic_buf), "%s", resp_topic);
    } else if (cmd_topic) {
        snprintf(topic_buf, sizeof(topic_buf), "%s/resp", cmd_topic);
    } else {
        snprintf(topic_buf, sizeof(topic_buf), "device/resp");
    }
    char *resp = form_json_response(cmd, result, desc);
    if (resp == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t rc = mqtt_app_publish(g_mqtt, topic_buf, resp);
    free(resp);
    return rc;
}

esp_err_t command_handler_process(const char *cmd, cJSON *_) {
    if (cmd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Handling command: %s", cmd);

    if (strcmp(cmd, "reboot") == 0) {
        publish_response(cmd, "ok", "restarting");
        esp_restart();
    }

    if (strcmp(cmd, "relay_on") == 0) {
        relay_set_state(g_relay, true);
        return publish_response(cmd, "ok", "relay turned on");
    }

    if (strcmp(cmd, "relay_off") == 0) {
        relay_set_state(g_relay, false);
        return publish_response(cmd, "ok", "relay turned off");
    }

    if (strcmp(cmd, "status") == 0) {
        bool state = relay_get_state(g_relay);
        int rssi = 0;
        if (modem_driver_get_signal_quality(g_modem, &rssi) != ESP_OK) {
            char desc[64];
            snprintf(desc, sizeof(desc), "relay:%s", state ? "on" : "off");
            return publish_response(cmd, "partial", desc);
        }
        char desc[128];
        snprintf(desc, sizeof(desc), "relay:%s,rssi:%d", state ? "on" : "off", rssi);
        return publish_response(cmd, "ok", desc);
    }

    return publish_response(cmd, "error", "unknown command");
}
