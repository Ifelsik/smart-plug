#include "app.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "led.h"
#include "button.h"
#include "mqtt.h"
#include "controller.h"

#define MODEM_START_NETWORK_TIMEOUT_MS 30000

static const char *TAG = "APP";

// ---- Инициализация периферии ------------------------------------------------

static modem_handle_t modem_app_start(const app_config_t *config) {
    ESP_LOGI(TAG, "Инициализация модема");

    // Питание, PWRKEY и холодный старт инкапсулированы в компоненте modem.
    modem_handle_t modem = modem_driver_init(&config->modem_config);
    if (modem == NULL) {
        ESP_LOGE(TAG, "Не удалось инициализировать модем!");
        return NULL;
    }

    // Первая попытка подключения. Если не удалась — не страшно: фоновый надзор
    // внутри компонента продолжит поднимать модем, ребут МК не нужен.
    if (modem_driver_start_network(modem, MODEM_START_NETWORK_TIMEOUT_MS) != ESP_OK) {
        ESP_LOGW(TAG, "Сеть пока не поднята, надзор продолжит попытки в фоне");
    } else {
        int signal_quality = 0;
        if (modem_driver_get_signal_quality(modem, &signal_quality) == ESP_OK) {
            ESP_LOGI(TAG, "Качество сигнала (RSSI): %d", signal_quality);
        } else {
            ESP_LOGW(TAG, "Не удалось получить RSSI!");
        }
    }

    return modem;
}

// ---- Точка входа приложения -------------------------------------------------

// Обёртка под сигнатуру button_press_cb_t (void*) без каста указателя на функцию.
static void on_button_press(void *user) {
    controller_toggle_relay((app_controller_t *) user);
}

void app_start(const app_config_t *config) {
    ESP_LOGI(TAG, "Инициализация устройства...");
    if (config == NULL) {
        ESP_LOGE(TAG, "app_start: конфиг не передан! Завершение.");
        abort();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Индикатор: мигает во время инициализации, горит постоянно после.
    led_handle_t led = led_init(config->indicator_pin);
    if (led != NULL) {
        led_set_state(led, LED_STATE_BLINK);
    }

    modem_handle_t modem = modem_app_start(config);
    if (modem == NULL) {
        abort();
    }

    relay_handle_t relay = relay_init(config->relay_pin);
    if (relay == NULL) {
        ESP_LOGE(TAG, "Не удалось инициализировать реле!");
        abort();
    }

    // Контроллер бизнес-логики. Зависимости внедряются явно.
    app_controller_t *controller = controller_create(modem, relay);
    if (controller == NULL) {
        ESP_LOGE(TAG, "Не удалось создать контроллер!");
        abort();
    }

    // MQTT: callback и контекст (контроллер) хранятся внутри объекта mqtt.
    mqtt_handle_t mqtt = mqtt_app_start(
        &config->mqtt_config,
        controller_handle_command,
        controller
    );
    if (mqtt == NULL) {
        ESP_LOGE(TAG, "Не удалось запустить MQTT!");
        abort();
    }
    controller_set_mqtt(controller, mqtt);

    // Кнопка (опционально): нажатие переключает реле.
    if (config->button_pin >= 0) {
        button_handle_t button = button_init(
            config->button_pin, on_button_press, controller);
        if (button == NULL) {
            ESP_LOGW(TAG, "Не удалось инициализировать кнопку");
        }
    }

    if (led != NULL) {
        led_set_state(led, LED_STATE_ON);
    }

    ESP_LOGI(TAG, "Инициализация завершена");
}
