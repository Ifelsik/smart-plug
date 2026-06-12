#include "led.h"

#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"

#define LED_BLINK_PERIOD_MS 200
#define LED_IDLE_POLL_MS    100

static const char *TAG = "LED";

struct led_ctx_t {
    int gpio_pin;
    volatile led_state_t state;
    TaskHandle_t task;
};

static void led_task(void *arg) {
    struct led_ctx_t *ctx = (struct led_ctx_t *) arg;
    bool blink_level = false;

    while(1) {
        switch (ctx->state) {
            case LED_STATE_BLINK:
                blink_level = !blink_level;
                gpio_set_level(ctx->gpio_pin, blink_level);
                vTaskDelay(pdMS_TO_TICKS(LED_BLINK_PERIOD_MS));
                break;
            case LED_STATE_ON:
                gpio_set_level(ctx->gpio_pin, 1);
                vTaskDelay(pdMS_TO_TICKS(LED_IDLE_POLL_MS));
                break;
            case LED_STATE_OFF:
            default:
                gpio_set_level(ctx->gpio_pin, 0);
                vTaskDelay(pdMS_TO_TICKS(LED_IDLE_POLL_MS));
                break;
        }
    }
}

led_handle_t led_init(int gpio_pin) {
    struct led_ctx_t *ctx = calloc(1, sizeof(struct led_ctx_t));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Не удалось выделить память под индикатор");
        return NULL;
    }

    ctx->gpio_pin = gpio_pin;
    ctx->state = LED_STATE_OFF;

    gpio_reset_pin(gpio_pin);
    gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio_pin, 0);

    if (xTaskCreate(led_task, "led_task", 2048, ctx, 5, &ctx->task) != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу индикатора");
        free(ctx);
        return NULL;
    }

    ESP_LOGI(TAG, "Индикатор инициализирован на GPIO %d", gpio_pin);
    return ctx;
}

void led_set_state(led_handle_t led, led_state_t state) {
    if (led == NULL) {
        return;
    }
    led->state = state;
}

void led_destroy(led_handle_t led) {
    if (led == NULL) {
        return;
    }
    if (led->task != NULL) {
        vTaskDelete(led->task);
    }
    gpio_set_level(led->gpio_pin, 0);
    free(led);
}
