#include "app.h"

// Определяем пины
#define MODEM_PWR_EN 14
#define MODEM_TX 18
#define MODEM_RX 17
#define MODEM_CTS 11
#define MODEM_RTS 12
#define MODEM_UART_PORT    UART_NUM_1

#define PIN_DCDC_EN 13

#define LED_PIN 18
#define RELAY_PIN 40

void app_main(void) {
    const app_config_t config = {
        .modem_config = {
            .pin_rx = MODEM_RX,
            .pin_tx = MODEM_TX,
            .pin_cts = MODEM_CTS,
            .pin_rts = MODEM_RTS,
            .apn = "internet.tele2.ru"
        },
        .dc_dc_enable_pin = PIN_DCDC_EN,
        .modem_pwr_key_pin = MODEM_PWR_EN,
        .mqtt_config = {
            .uri = "mqtt://broker.hivemq.com:1883",
            .cmd_topic = "thesis/plug/command",
            .resp_topic = "thesis/plug/state"
        },
        .indicator_pin = LED_PIN,
        .relay_pin = RELAY_PIN
    };

    app_start(&config);
}