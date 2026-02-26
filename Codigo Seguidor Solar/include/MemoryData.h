#ifndef MEMORY_DATA_H
#define MEMORY_DATA_H

#include <Preferences.h>
#include <nvs_flash.h>

class MemoryData {
public:
    MemoryData();
    void begin();
    // Credenciales WiFi
    bool saveWiFiCredentials(const String &ssid, const String &pass);
    bool loadWiFiCredentials(String &ssid, String &pass);
    // Ultimo estado del final de carrera azimutal
    bool savePosition(bool direction);
    bool loadPosition();
    // Operation Mode
    void saveOperationInterval(const String &intervalName, unsigned long newInterval);
    void saveOperationType(const String &newType);
    // Offsets motores
    bool saveOffsetMotor(const String& motor, uint8_t value);
    uint8_t loadOffsetMotor(const String& motor, uint8_t defaultValue = 1);
    // Offsets sensor
    bool saveOffsetSensor(const String& sensor, int16_t value);
    int16_t loadOffsetSensor(const String& sensor, int16_t defaultValue = 0);
    unsigned long logInterval, controlInterval;
    String operationType;

private:
    Preferences _prefs;
    void _loadOperationConfig();
    static const uint32_t _defaultTime = 300000;         // Tiempo por defecto del intervalo de registro
    const String _defaultType = "control";
};

#endif