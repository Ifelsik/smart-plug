#include "mqtt.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "MQTT_APP";

typedef struct mqtt_ctx_t {
    const char *uri;
    const char *cmd_topic;
    const char *resp_topic;
    esp_mqtt_client_handle_t client;
    mqtt_command_callback_t cmd_cb;
} mqtt_ctx_t;

// Глобальная переменная для простого сопоставления события и контекста
// Объявлена здесь, чтобы её можно было использовать в mqtt_app_start.
mqtt_ctx_t *g_mqtt_ctx_global = NULL;

static void mqtt_event_handler(
    void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data
) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    mqtt_ctx_t *ctx = handler_args;

    switch ((esp_mqtt_event_id_t) event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Успешно подключено к MQTT брокеру!");
            if (ctx && ctx->cmd_topic) {
                esp_mqtt_client_subscribe(client, ctx->cmd_topic, 1);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Отключено от MQTT брокера. Автоматическое переподключение...");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Успешно подписались на топик! msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "Получены данные! topic=%.*s, data=%.*s",
                event->topic_len, event->topic,
                event->data_len, event->data
            );
            mqtt_command_processor((const char*) event->topic, event->topic_len,
                                   event->data, event->data_len);
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Произошла ошибка MQTT!");
            break;
        default:
            ESP_LOGI(TAG, "Необработанное событие MQTT: id=%d", event->event_id);
    }
}

mqtt_handle_t mqtt_app_start(const app_config_t *config) {
    if (config == NULL) {
        ESP_LOGE(TAG, "Пустой конфиг!");
        abort();
    }

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = config->mqtt_config.uri,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Не удалось инициализировать MQTT клиент!");
        abort();
    }

    mqtt_ctx_t *mqtt_handle = calloc(1, sizeof(mqtt_ctx_t));
    if (mqtt_handle == NULL) {
        ESP_LOGE(TAG, "Не удалось инициализировать MQTT объект");
        esp_mqtt_client_destroy(client);
        return NULL;
    }
    mqtt_handle->client = client;
    mqtt_handle->uri = config->mqtt_config.uri;
    mqtt_handle->cmd_topic = config->mqtt_config.cmd_topic;
    mqtt_handle->resp_topic = config->mqtt_config.resp_topic;

    // сохраняем глобальный указатель для упрощённой маршрутизации команд
    g_mqtt_ctx_global = mqtt_handle;

    // регистрируем обработчик событий и передаём контекст в handler_args
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_handle
    ));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));

    return mqtt_handle;
}

esp_err_t mqtt_app_publish(mqtt_handle_t handle, const char *topic, const char *data) {
    if (handle == NULL || handle->client == NULL) {
        ESP_LOGE(TAG, "MQTT клиент не инициализирован!");
        return ESP_ERR_INVALID_ARG;
    }

    int msg_id = esp_mqtt_client_publish(handle->client, topic, data, 0, 1, 1);
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Сообщение отправлено в топик %s (msg_id=%d)", topic, msg_id);
    } else {
        ESP_LOGE(TAG, "Ошибка отправки сообщения (нет соединения)");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void mqtt_set_command_callback(mqtt_handle_t handle, mqtt_command_callback_t cb) {
    if (handle == NULL) {
        return;
    }
    handle->cmd_cb = cb;
}

// Аргумент надо освободить через free() после использования!
static char* parse_json_command(const char *json_string) {
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL) {
        return NULL;
    }

    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (!cJSON_IsString(cmd) || (cmd->valuestring == NULL)) {
        cJSON_Delete(root);
        return NULL;
    }

    char *result = strdup(cmd->valuestring);
    cJSON_Delete(root);
    return result;
}

esp_err_t mqtt_command_processor(
    const char *topic, int topic_len,
    const char *data, int data_len
) {
    // строка идёт без '\0'
    char *raw_json = malloc(data_len + 1);
    if (raw_json == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(raw_json, data, data_len);
    raw_json[data_len] = '\0';

    char *cmd = parse_json_command(raw_json);
    if (cmd == NULL) {
        free(raw_json);
        ESP_LOGE(TAG, "Не удалось распарсить JSON!");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Получена команда: %s", cmd);

    // попробуем найти глобальный контекст (через зарегистрированные клиенты)
    // Здесь проще: пройдём по всем клиентов зарегестрированным в esp-mqtt и
    // выберем тот, у которого совпадает topic подписки — но это сложно.
    // Вместо этого хранить cb в mqtt_ctx, и в mqtt_event_handler мы вызвали
    // mqtt_command_processor без ctx. Поэтому найдём клиент через event API
    // — упрощение: зарегистрируем callback в mqtt_ctx и вызовем её из mqtt_event_handler

    // Для простоты: парсинг payload в cJSON и вызов callback по всем клиентам.
    cJSON *root = cJSON_Parse(raw_json);
    if (root == NULL) {
        free(raw_json);
        free(cmd);
        ESP_LOGE(TAG, "JSON payload невалиден");
        return ESP_ERR_INVALID_ARG;
    }

    // Найдём соответствующий mqtt_ctx среди зарегистрированных клиентов.
    // У нас нет глобального списка — однако esp-mqtt передаёт контекст в обработчик.
    // В текущ архитектуре mqtt_event_handler уже знал ctx и вызвал mqtt_command_processor
    // без передачи ctx. Чтобы не менять сигнатуру, пройдём по всем клиентов не делая этого.
    // Упростим: используем esp_mqtt_client_get_handle_type trick — но его нет.

    // Workaround: найдём единственный mqtt_ctx созданный в приложении — можно хранить
    // указатель в статической переменной.
    extern mqtt_ctx_t *g_mqtt_ctx_global; // declared below
    if (g_mqtt_ctx_global && g_mqtt_ctx_global->cmd_cb) {
        // вызов бизнес-логики
        esp_err_t res = g_mqtt_ctx_global->cmd_cb(cmd, root);
        free(raw_json);
        free(cmd);
        cJSON_Delete(root);
        return res;
    }

    free(raw_json);
    free(cmd);
    cJSON_Delete(root);
    ESP_LOGW(TAG, "Команда получена, но callback не зарегистрирован");
    return ESP_ERR_NOT_FOUND;
}

const char* mqtt_get_cmd_topic(mqtt_handle_t handle) {
    if (handle == NULL) return NULL;
    return handle->cmd_topic;
}

const char* mqtt_get_resp_topic(mqtt_handle_t handle) {
    if (handle == NULL) return NULL;
    return handle->resp_topic;
}

// Перенесённая функция для формирования ответа
char* form_json_response(const char *cmd, const char *result, const char *desc) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    if (cmd == NULL) {
        cJSON_AddNullToObject(root, "cmd");
    } else {
        cJSON_AddStringToObject(root, "cmd", cmd);
    }

    if (result == NULL) {
        cJSON_AddNullToObject(root, "result");
    } else {
        cJSON_AddStringToObject(root, "result", result);
    }

    if (desc == NULL) {
        cJSON_AddNullToObject(root, "desc");
    } else {
        cJSON_AddStringToObject(root, "desc", desc);
    }
    
    char *josn_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return josn_string;
}
