#include "TemperatureSensor.h"

TemperatureSensor::TemperatureSensor(uint8_t pin)
    : oneWire(pin), sensors(&oneWire) {
    deviceCount = 0;
    addressesResolved = false;
}

bool TemperatureSensor::begin() {
    sensors.begin();
    deviceCount = sensors.getDeviceCount();
    if (deviceCount >= 2) {
        addressesResolved = sensors.getAddress(addr1, 0) && sensors.getAddress(addr2, 1);
    } else if (deviceCount == 1) {
        addressesResolved = sensors.getAddress(addr1, 0);
    }
    return (deviceCount > 0);
}

void TemperatureSensor::requestTemperatures() {
    sensors.requestTemperatures();
}

float TemperatureSensor::getTempC(int index) {
    if (index == 0 && deviceCount >= 1) {
        return sensors.getTempC(addr1);
    } else if (index == 1 && deviceCount >= 2) {
        return sensors.getTempC(addr2);
    }
    return DEVICE_DISCONNECTED_C;
}

int TemperatureSensor::getDeviceCount() {
    return deviceCount;
}

bool TemperatureSensor::getAddress(uint8_t* addr, int index) {
    if (index == 0 && deviceCount >= 1) {
        memcpy(addr, addr1, 8);
        return true;
    } else if (index == 1 && deviceCount >= 2) {
        memcpy(addr, addr2, 8);
        return true;
    }
    return false;
}