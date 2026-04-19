#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class TemperatureSensor {
public:
    TemperatureSensor(uint8_t pin);
    bool begin();
    void requestTemperatures();
    float getTempC(int index);  // index 0 – первый датчик, 1 – второй
    int getDeviceCount();
    bool getAddress(uint8_t* addr, int index);
private:
    OneWire oneWire;
    DallasTemperature sensors;
    uint8_t addr1[8];
    uint8_t addr2[8];
    int deviceCount;
    bool addressesResolved;
};

#endif