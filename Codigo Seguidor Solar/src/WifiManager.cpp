#include "WiFiManager.h"
#include "DebugLog.h"

WiFiManager::WiFiManager(MemoryData& memory) : _memory(memory) {}

void WiFiManager::begin() {
    String ssid, pass;
    if (_memory.loadWiFiCredentials(ssid, pass)) {
        startClientMode();
        return;
    }
    startAPMode();
}

void WiFiManager::startAPMode() {
    _wifiMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_ssidAp, _passdAp);
    DBG_PRINTLN("[AP] Conectado! SSID: " + _ssidAp + " IP: " + WiFi.softAPIP().toString());
}

void WiFiManager::startClientMode() {
    String ssid, pass;
    if (!_memory.loadWiFiCredentials(ssid, pass)) {
        DBG_PRINTLN("[STA] No hay credenciales guardadas → modo AP.");
        startAPMode(); // No hay credenciales → Modo AP
        return;
    }

    const uint32_t timeoutMs = 10000;       // 10 segundos máx de espera
    const uint32_t feedbackInterval = 500;

    _wifiMode = false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    DBG_PRINTLN("\n[STA] Intentando conectar a: " + ssid);

    uint32_t startTime = millis();
    uint32_t lastFeedback = 0;

    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeoutMs) {
        if (millis() - lastFeedback >= feedbackInterval) {
            DBG_PRINT(".");
            lastFeedback = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }

    if (WiFi.status() == WL_CONNECTED) {
        DBG_PRINTLN("\n[STA] ¡Conectado! IP: " + WiFi.localIP().toString());

        // Inicializar mDNS
        _initMDNS();

    } else {
        DBG_PRINTLN("\n[STA] No se logró conectar dentro del tiempo límite.");
        startAPMode();          // pasa a modo AP
    }
}

void WiFiManager::_initMDNS() {
    MDNS.end();                     // Finalizar instancia previa
    vTaskDelay(pdMS_TO_TICKS(100));
    if (MDNS.begin("SolarTracker")) {
        MDNS.addService("http", "tcp", 80);
        DBG_PRINTLN("[mDNS] Activo en: http://SolarTracker.local");
    } else {
        DBG_PRINTLN("[mDNS] Error al iniciar servicio mDNS");
    }
}

void WiFiManager::disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    DBG_PRINTLN("[WiFi] Desconectado completamente");
}

void WiFiManager::getConnectionInfo(bool &conectionMode, String &IPAddress, String &SSID) {
    conectionMode = _wifiMode;
    IPAddress = _wifiMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    SSID = _wifiMode ? _ssidAp : WiFi.SSID();
}