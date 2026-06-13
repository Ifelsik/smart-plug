#include "app.h"

// Управляющие выводы модема
#define MODEM_PWR_KEY_PIN  14   // PWRKEY (через инвертор VT2)
#define MODEM_TX           9    // ESP TX -> модем RXD (через TXS0104E)
#define MODEM_RX           10   // ESP RX <- модем TXD (через TXS0104E)
#define MODEM_CTS          11
#define MODEM_RTS          12
#define PIN_DCDC_EN        13   // EN преобразователя MP2307 (питание модема)

#define LED_PIN            18   // индикаторный светодиод VD9
#define RELAY_PIN          40   // реле K1 (через VT3)

// GPIO кнопки. Пока не разведён в прошивке: -1 отключает компонент кнопки.
#define BUTTON_PIN         8

void app_main(void) {
    const app_config_t config = {
        .modem_config = {
            .pin_rx = MODEM_RX,
            .pin_tx = MODEM_TX,
            .pin_cts = MODEM_CTS,
            .pin_rts = MODEM_RTS,
            .pin_pwrkey = MODEM_PWR_KEY_PIN,
            .pin_dc_dc_en = PIN_DCDC_EN,
            .apn = "internet.tele2.ru"
        },
        .mqtt_config = {
            .uri = "mqtt://small.isgood.host:1883",
            .cmd_topic = "thesis/plug/command",
            .resp_topic = "thesis/plug/state"
        },
        .relay_pin = RELAY_PIN,
        .indicator_pin = LED_PIN,
        .button_pin = BUTTON_PIN
    };

    app_start(&config);
}
