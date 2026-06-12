#include "modem.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_modem_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// события модема
#define MODEM_CONNECTED_BIT BIT0

static const char *TAG = "MODEM_DRIVER";

struct modem_ctx_t {
    esp_modem_dce_t *dce;
    esp_netif_t *netif;
    EventGroupHandle_t event_group;
};

// обработчик IP-событий
static void on_ip_event(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    struct modem_ctx_t *ctx = (struct modem_ctx_t*) arg;

    if (event_id == IP_EVENT_PPP_GOT_IP) {
        ESP_LOGI(TAG, "PPP соединение установлено!");
        xEventGroupSetBits(ctx->event_group, MODEM_CONNECTED_BIT);
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGW(TAG, "PPP соединение разорвано");
        xEventGroupClearBits(ctx->event_group, MODEM_CONNECTED_BIT);
    }
}

modem_handle_t modem_driver_init(const modem_config_t *config) {
    if (config == NULL) {
        return NULL;
    }

    struct modem_ctx_t *ctx = calloc(1, sizeof(struct modem_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->event_group = xEventGroupCreate();
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    ctx->netif = esp_netif_new(&netif_ppp_config);

    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, ctx);

    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = config->pin_tx;
    dte_config.uart_config.rx_io_num = config->pin_rx;
    dte_config.uart_config.rts_io_num = config->pin_rts;
    dte_config.uart_config.cts_io_num = config->pin_cts;
    // аппартный контроль потока
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_HW;

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(config->apn);

    // конфигурируем SIMCOM A7670E (совместим с SIM7600)
    ctx->dce = esp_modem_new_dev(
        ESP_MODEM_DCE_SIM7600,
        &dte_config,
        &dce_config,
        ctx->netif
    );

    if (ctx->dce == NULL) {
        ESP_LOGE(TAG, "Не удалось создать устройство модема");
        free(ctx);
        return NULL;
    }

    return ctx;
}

esp_err_t modem_driver_start_network(modem_handle_t handle, uint32_t timeout_ms) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (esp_modem_sync(handle->dce) != ESP_OK) {
        ESP_LOGE(TAG, "Модем не отвечает на AT-команды!");
        return ESP_FAIL;
    }

    esp_err_t err = esp_modem_set_mode(handle->dce, ESP_MODEM_MODE_DATA);
    if (err != ESP_OK) {
        return err;
    }

    EventBits_t bits = xEventGroupWaitBits(
        handle->event_group,
        MODEM_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(timeout_ms)
    );

    return (bits & MODEM_CONNECTED_BIT) ? ESP_OK : ESP_FAIL;
}

esp_err_t modem_driver_get_signal_quality(modem_handle_t handle, int *rssi) {
    if (handle == NULL || rssi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int ber;
    return esp_modem_get_signal_quality(handle->dce, rssi, &ber);
}

void modem_driver_destroy(modem_handle_t handle) {
    if (handle == NULL) {
        return;
    }
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event);
    esp_modem_destroy(handle->dce);
    esp_netif_destroy(handle->netif);
    vEventGroupDelete(handle->event_group);
    free(handle);
}
