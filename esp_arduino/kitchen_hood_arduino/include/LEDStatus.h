#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <Arduino.h>

enum LEDState {
    LED_OFF,
    LED_CONNECTING,   // медленное мигание
    LED_CONNECTED,    // горит постоянно
    LED_ERROR         // быстрое мигание
};

class LEDStatus {
public:
    LEDStatus(uint8_t pin, bool activeHigh = false);
    void begin();
    void setState(LEDState state);
    void loop();  // обновление мигания
private:
    uint8_t pin;
    bool activeHigh;
    LEDState currentState;
    uint32_t lastToggle;
    bool ledOn;
    void setPhysical(bool on);
};

#endif