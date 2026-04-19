#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

class MQTTManager {
public:
    typedef void (*CommandCallback)(bool on, uint32_t minutes);
    MQTTManager(const char* server, uint16_t port, const char* user, const char* pass, const char* clientId);
    void begin(Client& wifiClient, CommandCallback cb);
    void loop();
    bool publishStatus(JsonDocument& doc);
    bool isConnected();
private:
    PubSubClient client;
    const char* server;
    uint16_t port;
    const char* user;
    const char* pass;
    const char* clientId;
    CommandCallback cmdCallback;
    void reconnect();
    static void staticCallback(char* topic, byte* payload, unsigned int length);
    void handleCallback(char* topic, byte* payload, unsigned int length);
};

#endif