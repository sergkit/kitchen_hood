#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <Arduino.h>
#include <ArduinoJson.h>

class FanController {
public:
    FanController(uint8_t ventPin, float dtLimit);
    void begin();
    void update(float t1, float t2);               // Основная логика
    void setOverride(bool on, uint32_t durationMs); // Ручное управление (MQTT)
    bool isOn() const { return is_on; }
    const char* getReason() const { return reason; }
    float getTmin() const { return tmin; }
    void publishStatus(JsonDocument& doc);         // Заполнить JSON текущим состоянием
    void loop();                                   // Обработка таймера переопределения
private:
    uint8_t pinVent;
    float dtLimit;
    float t1, t2;
    float t1_last;
    float tmin;
    bool is_on;
    bool previous_change;
    bool temp_down;
    bool rpc_timer;
    bool ignore_up;
    char reason[32];
    uint32_t overrideEndMs;
    void setVent(bool state);
};

#endif