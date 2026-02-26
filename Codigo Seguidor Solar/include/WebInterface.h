#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include "DataLogger.h"
#include "WiFiManager.h"
#include "MotorsManager.h"
#include "SensorManager.h" 

class WebInterface {
public:
    WebInterface(AsyncWebServer& server, DataLogger& logger, WiFiManager& wifi, MemoryData& memory, MotorsManager& motors, SensorManager& sensor);
    void begin();
    void stopWeb();
    void broadcastMode(bool mode);
    void broadcastOffsetMotor(const String& motor, uint8_t value);
    void broadcastOffsetSensor(const String& sensor, uint16_t value);
    void setTaskHandle(TaskHandle_t type, TaskHandle_t button, TaskHandle_t web);

private:
    TaskHandle_t _taskManagerHandle = NULL;
    TaskHandle_t _buttonTaskHandle = NULL;
    TaskHandle_t _webControlHandle = NULL;
    
    AsyncWebServer& _server;
    AsyncWebSocket _ws;
    DataLogger& _logger;
    WiFiManager& _wifi;
    MemoryData& _memory;
    MotorsManager& _motors;
    SensorManager& _sensor;
    bool _serverStarted = false;
    
    void _setupRoutes();
    void _webSocketMessage(void *arg, uint8_t *data, size_t len);  
    void _webSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);  
    String _sensorChart(const String& date, int sensorIndex);
};

#endif