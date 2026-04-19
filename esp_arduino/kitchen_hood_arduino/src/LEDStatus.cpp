#include "LEDStatus.h"

LEDStatus::LEDStatus(uint8_t pin, bool activeHigh) : pin(pin), activeHigh(activeHigh) {
    currentState = LED_OFF;
    lastToggle = 0;
    ledOn = false;
}

void LEDStatus::begin() {
    pinMode(pin, OUTPUT);
    setPhysical(false);
}

void LEDStatus::setPhysical(bool on) {
    digitalWrite(pin, activeHigh ? on : !on);
    ledOn = on;
}

void LEDStatus::setState(LEDState state) {
    currentState = state;
    if (state == LED_OFF) {
        setPhysical(false);
    } else if (state == LED_CONNECTED) {
        setPhysical(true);
    } else {
        // мигание будет обработано в loop()
        lastToggle = 0; // сбросить, чтобы сразу начать мигать
    }
}

void LEDStatus::loop() {
    uint32_t now = millis();
    uint32_t interval = 0;
    switch (currentState) {
        case LED_CONNECTING:
            interval = 500;  // медленное мигание
            break;
        case LED_ERROR:
            interval = 200;  // частое мигание
            break;
        case LED_CONNECTED:
            setPhysical(true);
            return;
        case LED_OFF:
            setPhysical(false);
            return;
    }
    if (interval > 0 && (now - lastToggle >= interval)) {
        setPhysical(!ledOn);
        lastToggle = now;
    }
}