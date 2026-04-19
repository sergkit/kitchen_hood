#include "MQTTManager.h"
#include "config.h"

MQTTManager* mqttInstance = nullptr; // для статического колбэка

MQTTManager::MQTTManager(const char* server, uint16_t port, const char* user, const char* pass, const char* clientId)
    : server(server), port(port), user(user), pass(pass), clientId(clientId) {
    cmdCallback = nullptr;
    mqttInstance = this;
}

void MQTTManager::begin(Client& wifiClient, CommandCallback cb) {
    cmdCallback = cb;
    client.setClient(wifiClient);
    client.setServer(server, port);
    client.setCallback(staticCallback);
}

void MQTTManager::staticCallback(char* topic, byte* payload, unsigned int length) {
    if (mqttInstance) {
        mqttInstance->handleCallback(topic, payload, length);
    }
}

void MQTTManager::handleCallback(char* topic, byte* payload, unsigned int length) {
    // Парсим JSON команду: {"minutes":5, "on_off":true}
    payload[length] = '\0';
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) return;
    int minutes = doc["minutes"] | 0;
    bool on_off = doc["on_off"] | false;
    if (minutes > 0 && cmdCallback) {
        cmdCallback(on_off, minutes);
    }
}

void MQTTManager::reconnect() {
    while (!client.connected()) {
        if (client.connect(clientId, user, pass)) {
            client.subscribe(MQTT_COMMAND_TOPIC);
        } else {
            delay(MQTT_RECONNECT_MS);
        }
    }
}

void MQTTManager::loop() {
    if (!client.connected()) reconnect();
    client.loop();
}

bool MQTTManager::publishStatus(JsonDocument& doc) {
    String payload;
    serializeJson(doc, payload);
    return client.publish(MQTT_STATUS_TOPIC, payload.c_str(), true);
}

bool MQTTManager::isConnected(){
    return client.connected();
}