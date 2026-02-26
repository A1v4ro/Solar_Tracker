#include "WebInterface.h"
#include "DebugLog.h"

WebInterface::WebInterface(AsyncWebServer& server, DataLogger& logger, WiFiManager& wifi, MemoryData& memory, MotorsManager& motors, SensorManager& sensor) 
    : _server(server), _ws("/ws"), _logger(logger), _wifi(wifi), _memory(memory), _motors(motors), _sensor(sensor) {}

void WebInterface::begin() {
    if (_serverStarted) {
        DBG_PRINTLN("[WEB] Servidor ya iniciado, omitiendo...");
        return;
    }

    if (!LittleFS.begin(true)) {
        DBG_PRINTLN("[WEB] Error al montar LITTLEFS.");
        return;
    }
    DBG_PRINTLN("[WEB] LittleFS montado correctamente");

    // Configurar WebSocket
    _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->_webSocketEvent(server, client, type, arg, data, len);
    });
    _server.addHandler(&_ws);

    _setupRoutes();
    _server.begin();
    _serverStarted = true;
    DBG_PRINTLN("[WEB] Servidor HTTP iniciado");
}

void WebInterface::setTaskHandle(TaskHandle_t type, TaskHandle_t button, TaskHandle_t web) {
    _taskManagerHandle = type;
    _buttonTaskHandle = button;
    _webControlHandle = web;
}

void WebInterface::stopWeb(){
    _ws.closeAll();
    // _server.end();
    _serverStarted = false;
}

void WebInterface::_webSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch(type) {
        case WS_EVT_CONNECT:
            DBG_PRINTF("[WS] Cliente WebSocket #%u conectado desde %s\n", client->id(), client->remoteIP().toString().c_str());
            // Limitar número máximo de clientes WebSocket
            if (_ws.count() > 3) {  // Máximo 3 clientes
                client->close();
                return;
            }
            break;
        case WS_EVT_DISCONNECT:
            DBG_PRINTF("[WS] Cliente WebSocket #%u desconectado\n", client->id());
            break;
        case WS_EVT_DATA:
            _webSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebInterface::broadcastMode(bool mode) {
    JsonDocument doc;
    doc["type"] = "mode_ack";
    doc["mode"] = mode ? "manual" : "auto";

    String json;
    serializeJson(doc, json);
    _ws.textAll(json);  // <-- Notifica a todos los clientes WebSocket
}

void WebInterface::broadcastOffsetMotor(const String& motor, uint8_t value) {
    JsonDocument doc;
    doc["type"] = "offset_motor_update";
    doc["motor"] = motor;
    doc["value"] = value;

    String json;
    serializeJson(doc, json);
    _ws.textAll(json);  // Enviar a todos los clientes
}

void WebInterface::broadcastOffsetSensor(const String& sensor, uint16_t value) {
    JsonDocument doc;
    doc["type"] = "offset_sensor_update";
    doc["sensor"] = sensor;
    doc["value"] = value;

    String json;
    serializeJson(doc, json);
    _ws.textAll(json);  // Enviar a todos los clientes
}

void WebInterface::_webSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        String message = (char*)data;
        
        // Parsear mensaje JSON
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
            DBG_PRINT("[WS] Error parsing JSON: ");
            DBG_PRINTLN(error.c_str());
            return;
        }
        
        if (doc["type"] == "motor") {
            uint8_t dirH = doc["dirH"] | 0;
            uint8_t dirV = doc["dirV"] | 0;
            uint8_t speedH = doc["speedH"] | 0;
            uint8_t speedV = doc["speedV"] | 0;
            
            _motors.move(dirH, dirV, speedH, speedV);
            
            JsonDocument response;
            response["type"] = "motor_ack";
            response["status"] = "ok";
            String json;
            serializeJson(response, json);
            _ws.textAll(json);
        }
        else if (doc["type"] == "get_mode") { 
            bool mode = _motors.mode; 
            broadcastMode(mode);
        }
        else if (doc["type"] == "set_mode") {
            xTaskNotify(_buttonTaskHandle, 2, eSetValueWithOverwrite);
        }
    }
}

void WebInterface::_setupRoutes() {
    // Manejador para archivos estáticos
    auto staticHandler = [](const String& path, const String& contentType) {
        return [path, contentType](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/static/" + path, contentType);
        };
    };

    // Archivos estáticos
    _server.on("/", HTTP_GET, staticHandler("index.html", "text/html; charset=utf-8"));
    _server.on("/styles.css", HTTP_GET, staticHandler("styles.css", "text/css"));
    _server.on("/script.js", HTTP_GET, staticHandler("script.js", "text/javascript"));
    _server.on("/icons.js", HTTP_GET, staticHandler("icons.js", "text/javascript"));
    _server.on("/chart.min.js", HTTP_GET, staticHandler("chart.min.js", "text/javascript"));
    _server.on("/luxon.min.js", HTTP_GET, staticHandler("luxon.min.js", "text/javascript"));
    _server.on("/chartjs-adapter-luxon.min.js", HTTP_GET, staticHandler("chartjs-adapter-luxon.min.js", "text/javascript"));

    // SERVICIOS
    // Configurar WiFi
    _server.on("/config-wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String password = request->getParam("password", true)->value();
            
            if (ssid.length() == 0) {
                request->send(400, "text/plain", "El SSID no puede estar vacío");
                return;
            }
            
            if (password.length() < 8) {  
                request->send(400, "text/plain", "La contraseña debe tener al menos 8 caracteres");
                return;
            }
            
            if (_memory.saveWiFiCredentials(ssid, password)) {
                request->send(200, "text/plain", "Conexión exitosa");
                xTaskNotify(_webControlHandle, 2, eSetValueWithOverwrite);
            } else {
                request->send(500, "text/plain", "Error al guardar las credenciales");
            }
        } else {
            request->send(400, "text/plain", "Faltan parámetros (ssid o password)");
        }
    });

    // Obtener información de conexión (IP y modo)
    _server.on("/connection-info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        bool modeInfo;
        String ipInfo, ssidInfo;
        _wifi.getConnectionInfo(modeInfo, ipInfo, ssidInfo);
        JsonDocument doc;
        
        doc["wifi_mode"] = modeInfo ? "AP" : "STA";
        doc["ip"] = ipInfo;
        doc["ssid"] = ssidInfo;
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Ruta de referencia para el ping
    _server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "pong");
    });

    // Obtener fechas disponibles
    _server.on("/dates", HTTP_GET, [this](AsyncWebServerRequest *request) {
        std::vector<String> dates = _logger.getSensorDates();
        JsonDocument doc;
        JsonArray originalDates = doc["original"].to<JsonArray>(); 
        JsonArray displayDates = doc["display"].to<JsonArray>();   
        
        for (const String& date : dates) {
            originalDates.add(date);        // Formato YYYYMMDD
            if (date.length() == 8) {       // Convertir a DD/MM/YYYY
                String formatted = date.substring(6, 8) + "/" + 
                                date.substring(4, 6) + "/" + 
                                date.substring(0, 4);
                displayDates.add(formatted);
            } else {
                displayDates.add(date); 
            }
        }
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json; charset=utf-8", json);
    });

    // Mostrar lista de sensores
    _server.on("/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
        std::vector<String> sensors = _sensor.getSensorNames();
        JsonDocument doc;
        JsonArray sensorArray = doc["sensors"].to<JsonArray>();
        
        for (size_t i = 0; i < sensors.size(); i++) {
            JsonObject sensor = sensorArray.add<JsonObject>();
            sensor["id"] = i;
            sensor["name"] = sensors[i];
        }
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Obtener datos del sensor
    _server.on("/sensor-data", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("date") && request->hasParam("sensor")) {
            String date = request->getParam("date")->value();
            int sensorIndex = request->getParam("sensor")->value().toInt() + 1;     // Se suma 1 para obtener el índice real descontando la hora
            std::vector<String> sensorNames = _sensor.getSensorNames();
            if (sensorIndex < 0 || sensorIndex > sensorNames.size()) {
                request->send(400, "text/plain", "Índice de sensor no válido");
                return;
            }
            
            String json = _sensorChart(date, sensorIndex);
            request->send(200, "application/json; charset=utf-8", json);
        } else {
            request->send(400, "text/plain", "Parámetros faltantes");
        }
    });

    // Obtener modo de operación actual
    _server.on("/get-operation-type", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        
        doc["type"] = _memory.operationType;
        doc["controlInterval"] = _memory.controlInterval;
        doc["logInterval"] = _memory.logInterval;
            
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Configurar tipo de operación
    _server.on("/set-operation-type", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("type", true) && request->hasParam("controlInterval", true && request->hasParam("logInterval", true))) {
            String type = request->getParam("type", true)->value();
            unsigned long controlInterval = request->getParam("controlInterval", true)->value().toInt();
            unsigned long logInterval = request->getParam("logInterval", true)->value().toInt(); 

            DBG_PRINTF("[WEB] Configurando - Tipo de operacion: %s, Control: %lu, Log: %lu\n", type.c_str(), controlInterval, logInterval);

            // Validar y guardar en memoria
            if (type == "control" || type == "control-log") {
                _memory.saveOperationInterval("control", controlInterval);
            } 
            if (type == "sync" || type == "control-log"){
                _memory.saveOperationInterval("logger", logInterval);
            }
            _memory.saveOperationType(type);
            xTaskNotifyGive(_taskManagerHandle);
            
            request->send(200, "text/plain", "Configuración guardada");
            
        } else {
            request->send(400, "text/plain", "Content-Type debe ser application/json");
        }
    });

    // Obtener offsets de los motores
    _server.on("/get-motors-offsets", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["h_right"] = _motors.offsetH_right;
        doc["h_left"] = _motors.offsetH_left;
        doc["v_up"] = _motors.offsetV_up;
        doc["v_down"] = _motors.offsetV_down;
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Configurar offsets de los motores
    _server.on("/set-motors-offset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("motor", true) && request->hasParam("value", true)) {
            String motor = request->getParam("motor", true)->value();
            uint8_t value = request->getParam("value", true)->value().toInt();
            
            // Validar valor (1-100)
            if (value < 1 || value > 100) {
                request->send(400, "text/plain", "Valor debe estar entre 1-100");
                return;
            }
            
            // Actualizar offset correspondiente
            if (motor == "h_right") _motors.offsetH_right = value;
            else if (motor == "h_left") _motors.offsetH_left = value;
            else if (motor == "v_up") _motors.offsetV_up = value;
            else if (motor == "v_down") _motors.offsetV_down = value;
            else {
                request->send(400, "text/plain", "Motor no válido");
                return;
            }
            broadcastOffsetMotor(motor, value);
            if (_memory.saveOffsetMotor(motor, value)){
                request->send(200, "text/plain", "Offset actualizado");
            } else {
                request->send(400, "text/plain", "Error al guardar en memoria");
            }
        } else {
            request->send(400, "text/plain", "Faltan parámetros");
        }
    });

    // Obtener offsets de sensores 
    _server.on("/get-sensor-offsets", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;

        doc["azimut"] = _sensor.offsetAzimut;
        doc["cenit"] = _sensor.offsetCenit;
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Leer offsets de sensores 
    _server.on("/read-sensor-offsets", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
       
        _sensor.updateSolarSensor();
        _sensor.offsetAzimut = _sensor.getEazimut();
        _sensor.offsetCenit = _sensor.getEcenit();

        doc["azimut"] = _sensor.offsetAzimut;
        doc["cenit"] = _sensor.offsetCenit;
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Configurar offsets de sensores
    _server.on("/set-sensor-offset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("sensor", true) && request->hasParam("value", true)) {
            String sensor = request->getParam("sensor", true)->value();
            uint8_t value = request->getParam("value", true)->value().toInt();
            
            // Actualizar offset correspondiente
            if (sensor == "azimut") _sensor.offsetAzimut = value;
            else if (sensor == "cenit") _sensor.offsetCenit = value;
            else {
                request->send(400, "text/plain", "Sensor no válido");
                return;
            }
            broadcastOffsetSensor(sensor,value);
            if (_memory.saveOffsetSensor(sensor, value)){
                request->send(200, "text/plain", "Offset de sensor actualizado");
            } else {
                request->send(400, "text/plain", "Error al guardar en memoria");
            }
        } else {
            request->send(400, "text/plain", "Faltan parámetros");
        }
    });

    // Configurar tiempo del sistema
    _server.on("/set-time", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("datetime", true)) {
            time_t unixTime = request->getParam("datetime", true)->value().toInt();
            // Ajustar por zona horaria (ej: UTC-4 --> restar 4 horas)
            const int timezoneOffset = 4 * 3600; // 4 horas en segundos
            unixTime -= timezoneOffset; // Compensar el offset
            if (_logger.setTimeFromWeb(unixTime)){
                struct timeval now = { .tv_sec = unixTime };
                settimeofday(&now, NULL);
                request->send(200, "text/plain", "Hora actualizada");
            } else {
                request->send(400, "text/plain", "Error al guardar en memoria");
            }
        } else {
            request->send(400, "text/plain", "Falta parámetro 'datetime'");
        }
    });
}

String WebInterface::_sensorChart(const String& date, int sensorIndex) {
    JsonDocument doc;
    JsonArray labels = doc["labels"].to<JsonArray>();
    JsonArray values = doc["values"].to<JsonArray>();

    std::vector<String> stringData = _logger.getSensorData(date, sensorIndex, true);

    char timeBuffer[6] = {0};                       // HH:MM + null terminator
    for (const String& item : stringData) {
        const char* itemStr = item.c_str();
        const char* commaPos = strchr(itemStr, ',');
        
        if (commaPos && (commaPos - itemStr) >= 5) {
            strncpy(timeBuffer, itemStr, 5);        // Extraer HH:MM directamente
            labels.add(timeBuffer);
            values.add(atof(commaPos + 1));
        }
    }
    String json;
    serializeJson(doc, json);
    return json;
}