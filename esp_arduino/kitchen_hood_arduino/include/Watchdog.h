#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <Arduino.h>

class Watchdog {
public:
    Watchdog(uint8_t pin, uint32_t intervalMs);
    void begin();
    void loop();  // вызывать в loop(), генерирует импульс по таймеру
private:
    uint8_t pin;
    uint32_t interval;
    uint32_t lastTrigger;
    void pulse();
};

#endif