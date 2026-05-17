#pragma once

// MQTT-configuration
// Please note: all lengths will be published n-1 as maxlength to GUI
constexpr uint8_t mqttClientIdLength = 16u;
constexpr uint8_t mqttServerLength = 32u;
constexpr uint8_t mqttUserLength = 16u;
constexpr uint8_t mqttPasswordLength = 16u;
constexpr uint8_t mqttBaseTopicLength = 32u;
constexpr uint8_t mqttDeviceIdLength = 32u;

extern String gBaseTopic;
extern String gDeviceId;
extern String gMqttClientId;
extern String gMqttUser;
extern String gMqttPassword;
extern uint16_t gMqttPort;

#ifdef MQTT_ENABLE
consteval bool isMqttCompiled() { return true; }

void Mqtt_Init(void);
void Mqtt_Exit(void);
void Mqtt_OnWifiConnected(void);
bool Mqtt_IsEnabled(void);

void publishMqtt(const char *topic, const char *payload, bool retained);
void publishMqtt(const char *topic, int32_t payload, bool retained);
	#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR < 3))
void publishMqtt(const char *topic, unsigned long payload, bool retained);
	#endif
void publishMqtt(const char *topic, uint32_t payload, bool retained);

#else // if MQTT_ENABLE not defined, define dummy functions to avoid #ifdefs in code
consteval bool isMqttCompiled() { return false; }

inline void Mqtt_Init(void) {}
inline void Mqtt_Exit(void) {}
inline void Mqtt_OnWifiConnected(void) {}
inline bool Mqtt_IsEnabled(void) { return false; }
inline void publishMqtt(const char *topic, const char *payload, bool retained) {}
inline void publishMqtt(const char *topic, int32_t payload, bool retained) {}
	#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR < 3))
inline void publishMqtt(const char *topic, unsigned long payload, bool retained) {}
	#endif
inline void publishMqtt(const char *topic, uint32_t payload, bool retained) {}

#endif // MQTT_ENABLE
