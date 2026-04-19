#include "FanController.h"
#include <Arduino.h>

FanController::FanController(uint8_t ventPin, float dtLimit)
    : pinVent(ventPin), dtLimit(dtLimit) {
    t1 = t2 = t1_last = tmin = 0.0;
    is_on = false;
    previous_change = false;
    temp_down = false;
    rpc_timer = false;
    ignore_up = false;
    reason[0] = '\0';
    overrideEndMs = 0;
}

void FanController::begin() {
    pinMode(pinVent, OUTPUT);
    setVent(false);
}

void FanController::setVent(bool state) {
    // Вентилятор активным уровнем LOW (как в исходнике)
    digitalWrite(pinVent, state ? LOW : HIGH);
}

void FanController::setOverride(bool on, uint32_t durationMs) {
    if (durationMs > 0) {
        rpc_timer = true;
        is_on = on;
        setVent(is_on);
        strcpy(reason, "rpc");
        overrideEndMs = millis() + durationMs;
    }
}

void FanController::loop() {
    if (rpc_timer && overrideEndMs != 0 && millis() >= overrideEndMs) {
        rpc_timer = false;
        overrideEndMs = 0;
        // Сброс флагов, чтобы автономная логика снова заработала
        previous_change = false;
        ignore_up = false;
        // Состояние вентилятора остаётся текущим – далее логика сама решит
    }
}

void FanController::update(float new_t1, float new_t2) {
    t1 = new_t1;
    t2 = new_t2;
    if (t1 > -50 && t2 > -50) {  // валидные показания
        reason[0] = '\0';

        // Условия включения / выключения (полностью скопированы из оригинального main.c)
        if (((t1 - t1_last > 0.5f && t1_last > 0.0f && !is_on && !ignore_up) ||
             (t1 < tmin && is_on) ||
             (t1_last - t1 > 0.1f && t1_last > 0.0f && temp_down && is_on) ||
             (temp_down && t1_last > t1 && is_on)) &&
            !previous_change && !rpc_timer) {

            // Формируем причину переключения
            if (t1 - t1_last > 0.5f && t1_last > 0 && !is_on) strcat(reason, " +0.5");
            if (t1 < tmin && is_on) strcat(reason, " tmin");
            if (t1_last - t1 > 0.1f && t1_last > 0 && temp_down && is_on) strcat(reason, " -0.1");
            if (temp_down && t1_last > t1 && is_on) {
                strcat(reason, " -x2");
                ignore_up = true;   // предотвратить ложное включение после охлаждения корпуса
            }

            is_on = !is_on;
            setVent(is_on);
        } else {
            previous_change = false;
            ignore_up = false;
        }

        temp_down = (t1 < t1_last);
        t1_last = t1;

        // Управление tmin (температура верхнего датчика в момент включения)
        if (tmin < 0.0f && is_on) tmin = t2;
        if (!is_on && tmin > 0.0f) tmin = -1.0f;
    }
}

void FanController::publishStatus(JsonDocument& doc) {
    doc["t1"] = t1;
    doc["t2"] = t2;
    doc["is_on"] = is_on;
    doc["tmin"] = tmin;
    doc["reason"] = reason;
    doc["ver"] = "v3";
}