#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <ESPmDNS.h>  
#include "MemoryData.h"

class WiFiManager {
public:
    WiFiManager(MemoryData& memory);
    void begin();
    void startAPMode();
    void startClientMode();
    void getConnectionInfo(bool &conectionMode, String &IPAddress, String &SSID);
    void disconnect();

private:
    MemoryData& _memory;
    bool _wifiMode = false;                 // true AP, false STA
    String _ssidAp = "ESP32-Datalogger";
    String _passdAp = "config123";
    void _initMDNS();
};

#endif