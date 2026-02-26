#ifndef OTAMANAGER_H
#define OTAMANAGER_H

#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <StreamString.h>

class OTAManager {
public:
    OTAManager(AsyncWebServer* server);
    void begin();
    void setDevelopmentMode(bool enable);

private:
    AsyncWebServer* _server;
    String _username = "admin";
    String _password = "admin";
    String _otaToken;

    void _setupRoutes();
    bool _checkCredentials(const String& username, const String& password);
    String _getLoginPage();
    String _getUploadPage();
    bool _developmentMode = false;
};

#endif