#include "MemoryData.h"
#include "DebugLog.h"

MemoryData::MemoryData() 
    : controlInterval(_defaultTime), 
      logInterval(_defaultTime), 
      operationType(_defaultType) {}

void MemoryData::begin() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    _loadOperationConfig();
}

bool MemoryData::saveWiFiCredentials(const String &ssid, const String &pass) {
    _prefs.begin("wifi-creds", false);
    bool success = _prefs.putString("ssid", ssid) && _prefs.putString("password", pass);
    _prefs.end();
    return success;
}

bool MemoryData::loadWiFiCredentials(String &ssid, String &pass) {
    _prefs.begin("wifi-creds", true);
    ssid = _prefs.getString("ssid", "RED-WIFI");
    pass = _prefs.getString("password", "123456");
    _prefs.end();
    return !ssid.isEmpty() && !pass.isEmpty();
}

bool MemoryData::savePosition(bool direction){
    _prefs.begin("position", false);
    _prefs.putBool("azimutDir", direction);
    _prefs.end();

    return direction;
}

bool MemoryData::loadPosition(){
    _prefs.begin("position", true);
    bool currentPosition = _prefs.getBool("azimutDir", false);
    _prefs.end();

    return currentPosition;
}

void MemoryData::saveOperationInterval(const String& intervalName, unsigned long newInterval) {
    _prefs.begin("Intervals", false);
    _prefs.putULong(intervalName.c_str(), newInterval);  
    _prefs.end();

    if (intervalName == "control") {
        controlInterval = newInterval;
    } else if (intervalName == "logger") {
        logInterval = newInterval;
    }
}

void MemoryData::_loadOperationConfig() {
    _prefs.begin("Intervals", true);
    controlInterval = _prefs.getULong("control", _defaultTime);
    logInterval = _prefs.getULong("logger", _defaultTime);
    _prefs.end();

    _prefs.begin("Op-type", true);
    operationType = _prefs.getString("type", _defaultType);
    _prefs.end();
}

void MemoryData::saveOperationType(const String &newType) {
    _prefs.begin("Op-type", false);
    _prefs.putString("type", newType);
    _prefs.end();

    operationType = newType;
}

bool MemoryData::saveOffsetMotor(const String& motor, uint8_t value){
    _prefs.begin("M-offsets", false);
    bool success = _prefs.putUChar(motor.c_str(), value);  // Guarda con clave igual al nombre del motor
    _prefs.end();
    return success;
}   

uint8_t MemoryData::loadOffsetMotor(const String& motor, uint8_t defaultValue) {
    _prefs.begin("M-offsets", true);  // solo lectura
    uint8_t offset = _prefs.getUChar(motor.c_str(), defaultValue);
    _prefs.end();
    return offset;
}

bool MemoryData::saveOffsetSensor(const String& sensor, int16_t value){
    _prefs.begin("S-offsets", false);
    bool success = _prefs.putShort(sensor.c_str(), value);  // Guarda con clave igual al nombre del motor
    _prefs.end();
    return success;
}   

int16_t MemoryData::loadOffsetSensor(const String& sensor, int16_t defaultValue) {
    _prefs.begin("S-offsets", true);  // solo lectura
    int16_t offset = _prefs.getShort(sensor.c_str(), defaultValue);
    _prefs.end();
    return offset;
}