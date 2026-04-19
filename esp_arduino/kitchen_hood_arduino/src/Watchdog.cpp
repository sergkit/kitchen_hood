#include "Watchdog.h"

Watchdog::Watchdog(uint8_t pin, uint32_t intervalMs) : pin(pin), interval(intervalMs) {
    lastTrigger = 0;
}

void Watchdog::begin() {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void Watchdog::pulse() {
    digitalWrite(pin, HIGH);
    delayMicroseconds(2);
    digitalWrite(pin, LOW);
}

void Watchdog::loop() {
    if (millis() - lastTrigger >= interval) {
        pulse();
        lastTrigger = millis();
    }
}