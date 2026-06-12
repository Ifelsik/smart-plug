#include "app.h"

#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "mqtt.h"
#include "command_handler.h"

#define MODEM_DC_DC_EN_DELAY_MS 1500
#define MODEM_PWR_KEY_DELAY_MS 2000
#define MODEM_START_NETWORK_TIMEOUT_MS 30000

#define LED_BLINK_SYSTEM_INIT_DELAY_MS 200

const char *TAG = "APP";

modem_handle_t modem_app_start(const app_config_t *config) {
    ESP_LOGI(TAG, "Инициализация модема");
    if (config == NULL) {
        ESP_LOGE(TAG, "modem_app_start: конфиг не передан! Завершение.");
        abort();
    }

    gpio_set_direction(config->modem_pwr_key_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(config->dc_dc_enable_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(config->modem_pwr_key_pin, 0);
    gpio_set_level(config->dc_dc_enable_pin, 0);

    // включаем DC-DC преобразователь
    gpio_set_level(config->dc_dc_enable_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(MODEM_DC_DC_EN_DELAY_MS));

    // подаем сигнал на включение модема
    gpio_set_level(config->modem_pwr_key_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(MODEM_PWR_KEY_DELAY_MS));
    gpio_set_level(config->modem_pwr_key_pin, 0);

    modem_handle_t modem =  modem_driver_init(&config->modem_config);

    if (modem == NULL) {
        ESP_LOGE(TAG, "Не удалось инициализировать модем!");
        abort();
    }

    ESP_ERROR_CHECK(modem_driver_start_network(modem, MODEM_START_NETWORK_TIMEOUT_MS));

    int signal_quality = 0;
    if (modem_driver_get_signal_quality(modem, &signal_quality) == ESP_OK) {
        ESP_LOGI(TAG, "Качество сигнала (RSSI): %d", signal_quality);
    } else {
        ESP_LOGW(TAG, "Не удалось получить RSSI!");
    }

    return modem;
}

relay_handle_t relay_app_start(const app_config_t *config) {
    ESP_LOGI(TAG, "Инициализация реле");
    if (config == NULL) {
        ESP_LOGE(TAG, "relay_app_start: конфиг не передан! Завершение.");
        abort();
    }

    relay_handle_t relay = relay_init(config->relay_pin);
    if (relay == NULL) {
        ESP_LOGE(TAG, "Не удалось инициализировать реле!");
        abort();
    }

    return relay;
}

volatile bool is_init_done = false;

// Мигает, пока инициализируется переферия
void led_blink_task(void *pvParameters) {
    int led_pin = (int) pvParameters;

    gpio_reset_pin(led_pin);
    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);

    int led_state = 1;

    while (!is_init_done) {
        gpio_set_level(led_pin, led_state);
        led_state = !led_state;
        vTaskDelay(pdMS_TO_TICKS(LED_BLINK_SYSTEM_INIT_DELAY_MS));
    }

    gpio_set_level(led_pin, 1);
    
    // таска завершает себя
    vTaskDelete(NULL);
} 

void led_app_start(const app_config_t *config) {
    ESP_LOGI(TAG, "Инициализация индикатора");
    if (config == NULL) {
        ESP_LOGE(TAG, "led_app_start: конфиг не передан! Завершение.");
        abort();
    }

    xTaskCreate(led_blink_task, "led_blink_task", 2048, (void *) config->indicator_pin, 5, NULL);
}

void app_start(const app_config_t *config) {
    ESP_LOGI(TAG, "Инициализация устройства...");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    led_app_start(config);
    modem_handle_t modem = modem_app_start(config);
    relay_handle_t relay = relay_app_start(config);

    // Запускаем MQTT и регистрируем обработчик команд
    mqtt_handle_t mqtt = mqtt_app_start(config);
    if (mqtt == NULL) {
        ESP_LOGW(TAG, "MQTT не инициализирован: пропускаем регистрацию команд");
    } else {
        // инициализируем обработчик команд с зависимостями
        if (command_handler_init(modem, relay, mqtt) == ESP_OK) {
            mqtt_set_command_callback(mqtt, command_handler_process);
        }
    }

    is_init_done = true;
}