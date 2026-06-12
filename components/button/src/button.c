#include "button.h"

#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_log.h"

#define BUTTON_DEBOUNCE_MS 30

static const char *TAG = "BUTTON";

struct button_ctx_t {
    int gpio_pin;
    button_press_cb_t cb;
    void *user;
    QueueHandle_t evt_queue;
    TaskHandle_t task;
};

static void IRAM_ATTR button_isr_handler(void *arg) {
    struct button_ctx_t *ctx = (struct button_ctx_t *) arg;
    uint32_t dummy = 0;
    xQueueSendFromISR(ctx->evt_queue, &dummy, NULL);
}

static void button_task(void *arg) {
    struct button_ctx_t *ctx = (struct button_ctx_t *) arg;
    uint32_t dummy;

    for (;;) {
        if (xQueueReceive(ctx->evt_queue, &dummy, portMAX_DELAY)) {
            // антидребезг: ждём и проверяем, что кнопка всё ещё нажата (low)
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
            if (gpio_get_level(ctx->gpio_pin) == 0 && ctx->cb != NULL) {
                ctx->cb(ctx->user);
            }
        }
    }
}

button_handle_t button_init(int gpio_pin, button_press_cb_t cb, void *user) {
    struct button_ctx_t *ctx = calloc(1, sizeof(struct button_ctx_t));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Не удалось выделить память под кнопку");
        return NULL;
    }

    ctx->gpio_pin = gpio_pin;
    ctx->cb = cb;
    ctx->user = user;

    ctx->evt_queue = xQueueCreate(4, sizeof(uint32_t));
    if (ctx->evt_queue == NULL) {
        ESP_LOGE(TAG, "Не удалось создать очередь событий кнопки");
        free(ctx);
        return NULL;
    }

    const gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    // Сервис прерываний GPIO может быть уже установлен другим компонентом.
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Не удалось установить ISR service: %s", esp_err_to_name(err));
        vQueueDelete(ctx->evt_queue);
        free(ctx);
        return NULL;
    }

    if (xTaskCreate(button_task, "button_task", 2560, ctx, 6, &ctx->task) != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу кнопки");
        vQueueDelete(ctx->evt_queue);
        free(ctx);
        return NULL;
    }

    gpio_isr_handler_add(gpio_pin, button_isr_handler, ctx);

    ESP_LOGI(TAG, "Кнопка инициализирована на GPIO %d", gpio_pin);
    return ctx;
}

void button_destroy(button_handle_t button) {
    if (button == NULL) {
        return;
    }
    gpio_isr_handler_remove(button->gpio_pin);
    if (button->task != NULL) {
        vTaskDelete(button->task);
    }
    if (button->evt_queue != NULL) {
        vQueueDelete(button->evt_queue);
    }
    free(button);
}
