#ifndef CONFIG_H
#define CONFIG_H
#include "const.h"

#ifndef WIFI_SSID
// WiFi Configuration
#define WIFI_SSID  "your_wifi_ssid"
#define WIFI_PASSWORD  "your_wifi_password"

// MQTT Configuration
#define MQTT_SERVER  "mqtt_broker_ip"
#define MQTT_PORT  1883
#define MQTT_CLIENT_ID  "boiler_controller"
#define  MQTT_USER "user"
#define  MQTT_PASS "password"
#endif

// === Пины ===
#define PIN_VENT        14      // D5 – управление вентилятором (0=вкл, 1=выкл)
#define PIN_WD          15      // D8 – сигнал Watchdog (положительный импульс)
#define PIN_WIRE        12      // D6 – шина OneWire (DS18B20)
#define PIN_LED         2       // D4 – встроенный светодиод (NodeMCU, активный LOW)

// === Интервалы (мс) ===
#define SENSOR_READ_MS  60000   // Период опроса датчиков
#define WD_TIMER_MS     60000   // Период импульса Watchdog
#define MQTT_RECONNECT_MS 5000  // Попытка переподключения MQTT

// === Параметры управления ===
#define DT_LIMIT        1.5f    // Дельта t для включения (аналог app.dt_level/10)

// === MQTT (если не используется WiFiManager) ===
#define MQTT_STATUS_TOPIC "kitchen/fan/status"
#define MQTT_COMMAND_TOPIC "kitchen/fan/set"

// === Режим работы ===
// true  – использовать WiFiManager для интерактивной настройки
// false – использовать статические SSID/PASS из кода (задайте их в main.ino)
#define USE_WIFI_MANAGER true

#endif