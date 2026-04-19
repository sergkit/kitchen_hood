#include <ESP8266WiFi.h>
#include <WiFiManager.h>          // для удобной настройки WiFi
#include "config.h"
#include "TemperatureSensor.h"
#include "FanController.h"
#include "MQTTManager.h"
#include "Watchdog.h"
#include "LEDStatus.h"

// Глобальные объекты
TemperatureSensor tempSensor(PIN_WIRE);
FanController fanCtrl(PIN_VENT, DT_LIMIT);
Watchdog watchdog(PIN_WD, WD_TIMER_MS);
LEDStatus led(PIN_LED, false);    // активный LOW для NodeMCU

WiFiClient espClient;
MQTTManager mqtt(MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASS, MQTT_CLIENT_ID);

// Таймеры для периодических задач (non-blocking)
uint32_t lastSensorRead = 0;
uint32_t lastMqttPublish = 0;

// Функция обратного вызова для MQTT команд
void onMqttCommand(bool on_off, uint32_t minutes) {
    uint32_t durationMs = minutes * 60000UL;
    fanCtrl.setOverride(on_off, durationMs);
    // Опубликовать состояние сразу
    StaticJsonDocument<256> doc;
    fanCtrl.publishStatus(doc);
    mqtt.publishStatus(doc);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nStarting Fan Controller...");

    // Инициализация периферии
    fanCtrl.begin();
    watchdog.begin();
    led.begin();

    // Настройка WiFi
    led.setState(LED_CONNECTING);
#if USE_WIFI_MANAGER
    WiFiManager wm;
    wm.autoConnect("KitchenFan_AP");
#else
    WiFi.begin(WIFI_SSID, WIFI_PASS);   // задайте эти макросы при статической настройке
    while (WiFi.status() != WL_CONNECTED) delay(500);
#endif
    led.setState(LED_CONNECTED);
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());

    // Инициализация MQTT
    mqtt.begin(espClient, onMqttCommand);
    led.setState(LED_CONNECTING); // пока MQTT не подключится
    while (!mqtt.isConnected()) {
        mqtt.loop();
        delay(100);
    }
    led.setState(LED_CONNECTED);
    Serial.println("MQTT connected");

    // Инициализация датчиков температуры
    if (!tempSensor.begin()) {
        Serial.println("ERROR: No DS18B20 found!");
        led.setState(LED_ERROR);
    } else {
        Serial.printf("Found %d sensors\n", tempSensor.getDeviceCount());
    }
}

void loop() {
    // 1. MQTT (поддержание связи, обработка входящих команд)
    mqtt.loop();

    // 2. Watchdog – периодический импульс
    watchdog.loop();

    // 3. LED мигание (по состоянию)
    led.loop();

    // 4. Обработка таймера переопределения (rpc_timer)
    fanCtrl.loop();

    // 5. Чтение датчиков с заданным интервалом
    if (millis() - lastSensorRead >= SENSOR_READ_MS) {
        lastSensorRead = millis();

        tempSensor.requestTemperatures();
        float t1 = tempSensor.getTempC(0);
        float t2 = tempSensor.getTempC(1);
        if (t1 != DEVICE_DISCONNECTED_C && t2 != DEVICE_DISCONNECTED_C) {
            fanCtrl.update(t1, t2);
        } else if (tempSensor.getDeviceCount() == 1) {
            // если только один датчик, используем его для t1, а t2 игнорируем
            fanCtrl.update(t1, t1);
        } else {
            Serial.println("Sensor read error");
        }
    }

    // 6. Публикация состояния в MQTT (каждый цикл чтения + при изменении)
    //    Для простоты публикуем при каждом чтении датчиков, но можно и чаще
    if (millis() - lastMqttPublish >= SENSOR_READ_MS) {
        lastMqttPublish = millis();
        StaticJsonDocument<256> doc;
        fanCtrl.publishStatus(doc);
        mqtt.publishStatus(doc);
    }
}