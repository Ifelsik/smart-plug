#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "esp_err.h"
#include "cJSON.h"

#include "modem.h"
#include "relay.h"
#include "mqtt.h"

// Контроллер — оркестратор бизнес-логики. Владеет ссылками на драйверы и
// маршрутизирует команды. Непрозрачный тип: внутренности скрыты от вызывающих.
typedef struct app_controller_t app_controller_t;

// Конструктор. Зависимости передаются явно (внедрение зависимостей),
// глобальные переменные не используются. Возвращает NULL при ошибке.
// MQTT привязывается отдельно через controller_set_mqtt (разрыв цикла:
// MQTT хранит контроллер как user, а контроллер публикует через MQTT).
app_controller_t* controller_create(modem_handle_t modem, relay_handle_t relay);

// Привязать MQTT-клиент после его создания.
void controller_set_mqtt(app_controller_t *self, mqtt_handle_t mqtt);

// Обработчик входящей команды. Сигнатура совместима с mqtt_command_cb_t:
// user должен быть указателем app_controller_t*.
esp_err_t controller_handle_command(void *user, const char *cmd, cJSON *payload);

// Переключить реле в противоположное состояние (например, по нажатию кнопки)
// и опубликовать новое состояние. Безопасно вызывать из любой задачи.
void controller_toggle_relay(app_controller_t *self);

#endif
