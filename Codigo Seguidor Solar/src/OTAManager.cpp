#include "OTAManager.h"
#include "DebugLog.h"

OTAManager::OTAManager(AsyncWebServer* server) : _server(server) {}

void OTAManager::begin() {
    _setupRoutes();
}

void OTAManager::setDevelopmentMode(bool enable) {
    _developmentMode = enable;
    if (enable) DBG_PRINTLN("[OTA] Modo desarrollo activado (sin autenticación)");
}

void OTAManager::_setupRoutes() {
    // Ruta para la página de login OTA
    _server->on("/ota", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String authHeader = request->hasHeader("Authorization") ? request->getHeader("Authorization")->value() : "";
        String tokenParam = request->hasParam("token") ? request->getParam("token")->value() : "";
        
        if (_developmentMode || (_otaToken.length() > 0 && (authHeader == _otaToken || tokenParam == _otaToken))) {
            request->send(200, "text/html", _getUploadPage());
        } else {
            request->send(200, "text/html", _getLoginPage());
        }
    });

    // Ruta para verificar credenciales
    _server->on("/ota/login", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("username", true) && request->hasParam("password", true)) {
            String username = request->getParam("username", true)->value();
            String password = request->getParam("password", true)->value();
            
            if (_checkCredentials(username, password)) {
                _otaToken = "OTA_AUTH_" + String(millis());
                request->send(200, "text/plain", _otaToken);
            } else {
                _otaToken = "";
                request->send(401, "text/plain", "Credenciales incorrectas");
            }
        } else {
            request->send(400, "text/plain", "Faltan parámetros");
        }
    });

    // Ruta para subir archivos OTA
    _server->on("/ota/upload", HTTP_POST, 
        [this](AsyncWebServerRequest *request) {
            if (!_developmentMode && (_otaToken.isEmpty() || !request->hasHeader("Authorization") || 
                request->getHeader("Authorization")->value() != _otaToken)) {
                return request->send(401, "text/plain", "Acceso no autorizado");
            }

            if (Update.hasError()) {
                StreamString error;
                Update.printError(error);
                DBG_PRINTLN("[OTA] Error: " + error);
                request->send(500, "text/plain", "Error: " + error);
            } else {
                request->send(200, "text/plain", "¡Actualización exitosa! Reiniciando...");
                DBG_PRINTLN("[OTA] Actualización finalizada, reiniciando en 2 segundos...");
                delay(2000);
                ESP.restart();
            }
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!_developmentMode && (_otaToken.isEmpty() || !request->hasHeader("Authorization") || 
                request->getHeader("Authorization")->value() != _otaToken)) {
                return;
            }

            if (!index) {
                DBG_PRINTLN("[OTA] Iniciando subida. Archivo: " + filename);
                
                // Detección automática del tipo de archivo
                int cmd = (filename.indexOf("spiffs") != -1 || filename.indexOf("littlefs") != -1) ? U_SPIFFS : U_FLASH;
                DBG_PRINTF("[OTA] Tipo detectado: %s\n", cmd == U_SPIFFS ? "SPIFFS" : "Firmware");

                if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
                    StreamString error;
                    Update.printError(error);
                    request->send(400, "text/plain", "Error al iniciar: " + error);
                    return;
                }
            }

            if (len) {
                if (Update.write(data, len) != len) {
                    Update.abort();
                    request->send(400, "text/plain", "Error al escribir datos");
                    return;
                }
            }

            if (final) {
                if (!Update.end(true)) {
                    StreamString error;
                    Update.printError(error);
                    request->send(400, "text/plain", "Error al finalizar: " + error);
                }
            }
        }
    );
}

bool OTAManager::_checkCredentials(const String& username, const String& password) {
    return (username == _username && password == _password);
}

String OTAManager::_getLoginPage() {
    return R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OTA Update - Login</title>
    <style>
        :root {
            --primary: #4361ee;
            --primary-dark: #3a0ca3;
            --error: #ef233c;
            --background: #f8f9fa;
            --card: #ffffff;
            --text: #2b2d42;
            --border: #e9ecef;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        body {
            background: var(--background);
            color: var(--text);
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            padding: 20px;
        }
        .login-container {
            background: var(--card);
            width: 100%;
            max-width: 400px;
            padding: 2rem;
            border-radius: 12px;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.1);
            text-align: center;
            animation: fadeIn 0.5s ease;
        }
        h1 {
            margin-bottom: 1.5rem;
            font-size: 1.8rem;
            color: var(--primary);
        }
        .form-group {
            margin-bottom: 1.2rem;
            text-align: left;
        }
        label {
            display: block;
            margin-bottom: 0.5rem;
            font-weight: 600;
            color: var(--text);
        }
        input {
            width: 100%;
            padding: 12px;
            border: 2px solid var(--border);
            border-radius: 8px;
            font-size: 1rem;
            transition: border 0.3s;
        }
        input:focus {
            outline: none;
            border-color: var(--primary);
        }
        button {
            width: 100%;
            padding: 12px;
            background: var(--primary);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.3s;
            margin-top: 0.5rem;
        }
        button:hover {
            background: var(--primary-dark);
        }
        #status {
            margin-top: 1.5rem;
            padding: 12px;
            border-radius: 8px;
            display: none;
            font-size: 0.9rem;
        }
        .error {
            background: #fff5f5;
            color: var(--error);
            border-left: 4px solid var(--error);
        }
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(-10px); }
            to { opacity: 1; transform: translateY(0); }
        }
    </style>
</head>
<body>
    <div class="login-container">
        <h1>🔒 OTA Login</h1>
        <div class="form-group">
            <label for="username">Usuario</label>
            <input type="text" id="username" placeholder="Ingresa tu usuario" required>
        </div>
        <div class="form-group">
            <label for="password">Contraseña</label>
            <input type="password" id="password" placeholder="Ingresa tu contraseña" required>
        </div>
        <button onclick="login()">Acceder</button>
        <div id="status"></div>
    </div>
    <script>
        function login() {
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;
            const status = document.getElementById('status');
            
            if (!username || !password) {
                status.textContent = 'Ingrese usuario y contraseña';
                status.className = 'error';
                status.style.display = 'block';
                return;
            }
            
            fetch('/ota/login', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `username=${encodeURIComponent(username)}&password=${encodeURIComponent(password)}`
            })
            .then(response => {
                if (response.status === 200) return response.text();
                throw new Error('Credenciales incorrectas');
            })
            .then(token => {
                localStorage.setItem('otaToken', token);
                window.location.href = '/ota?token=' + encodeURIComponent(token);
            })
            .catch(error => {
                status.textContent = error.message;
                status.className = 'error';
                status.style.display = 'block';
            });
        }
    </script>
</body>
</html>
)=====";
}

String OTAManager::_getUploadPage() {
    return R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OTA Update</title>
    <style>
        :root {
            --primary: #4361ee;
            --primary-dark: #3a0ca3;
            --success: #4cc9f0;
            --error: #ef233c;
            --background: #f8f9fa;
            --card: #ffffff;
            --text: #2b2d42;
            --border: #e9ecef;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        body {
            background: var(--background);
            color: var(--text);
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            padding: 20px;
        }
        .upload-container {
            background: var(--card);
            width: 100%;
            max-width: 500px;
            padding: 2rem;
            border-radius: 12px;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.1);
            animation: fadeIn 0.5s ease;
        }
        h1 {
            margin-bottom: 1.5rem;
            font-size: 1.8rem;
            color: var(--primary);
            text-align: center;
        }
        .form-group {
            margin-bottom: 1.2rem;
        }
        label {
            display: block;
            margin-bottom: 0.5rem;
            font-weight: 600;
            color: var(--text);
        }
        input[type="file"] {
            width: 100%;
            padding: 12px;
            border: 2px dashed var(--border);
            border-radius: 8px;
            background: #f8f9fa;
            cursor: pointer;
            transition: border 0.3s;
        }
        input[type="file"]:hover {
            border-color: var(--primary);
        }
        .progress {
            height: 24px;
            background: var(--border);
            border-radius: 8px;
            margin: 1.5rem 0;
            overflow: hidden;
        }
        .progress-bar {
            height: 100%;
            background: var(--primary);
            color: white;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 0.8rem;
            font-weight: 600;
            transition: width 0.5s ease;
        }
        .button-group {
            display: flex;
            gap: 10px;
            margin-top: 1.5rem;
        }
        button {
            flex: 1;
            padding: 12px;
            border: none;
            border-radius: 8px;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
        }
        button.primary {
            background: var(--primary);
            color: white;
        }
        button.primary:hover {
            background: var(--primary-dark);
        }
        button.secondary {
            background: var(--border);
            color: var(--text);
        }
        button.secondary:hover {
            background: #dee2e6;
        }
        #status {
            margin-top: 1.5rem;
            padding: 12px;
            border-radius: 8px;
            display: none;
            font-size: 0.9rem;
        }
        .success {
            background: #f0fff4;
            color: #2e7d32;
            border-left: 4px solid #4caf50;
        }
        .error {
            background: #fff5f5;
            color: var(--error);
            border-left: 4px solid var(--error);
        }
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(-10px); }
            to { opacity: 1; transform: translateY(0); }
        }
    </style>
</head>
<body>
    <div class="upload-container">
        <h1>🚀 OTA Update</h1>
        <div class="form-group">
            <label for="ota-file">Selecciona el archivo:</label>
            <input type="file" id="ota-file" accept=".bin">
        </div>
        <div class="progress">
            <div id="ota-progress" class="progress-bar" style="width: 0%">0%</div>
        </div>
        <div class="button-group">
            <button class="primary" onclick="uploadFile()">Subir Archivo</button>
            <button class="secondary" onclick="window.location.href='/'">Cancelar</button>
        </div>
        <div id="status"></div>
    </div>
    <script>
        function uploadFile() {
            const fileInput = document.getElementById('ota-file');
            const token = localStorage.getItem('otaToken');
            const status = document.getElementById('status');
            const progressBar = document.getElementById('ota-progress');
            
            if (!fileInput.files.length) {
                status.textContent = 'Seleccione un archivo';
                status.className = 'error';
                status.style.display = 'block';
                return;
            }
            
            const file = fileInput.files[0];
            const formData = new FormData();
            formData.append('file', file); // ¡El nombre del archivo se envía automáticamente!
            
            status.textContent = 'Preparando actualización...';
            status.className = '';
            status.style.display = 'block';
            
            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/ota/upload', true);
            xhr.setRequestHeader('Authorization', token);
            
            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    progressBar.style.width = percent + '%';
                    progressBar.textContent = percent + '%';
                    status.textContent = 'Subiendo... ' + percent + '%';
                }
            };
            
            xhr.onload = function() {
                if (xhr.status === 200) {
                    status.textContent = '¡Actualización completada! Reiniciando...';
                    status.className = 'success';
                    progressBar.style.width = '100%';
                    progressBar.textContent = '100%';
                    setTimeout(() => {
                        if (progressBar.textContent === "100%") {
                            status.textContent = 'Firmware subido, el dispositivo se reiniciará...';
                            status.className = 'success';
                        }
                    }, 3000);
                } else {
                    status.textContent = 'Error: ' + xhr.responseText;
                    status.className = 'error';
                }
            };
            
            xhr.onerror = function() {
                status.textContent = 'Error de conexión';
                status.className = 'error';
            };
            
            xhr.send(formData);
        }
    </script>
</body>
</html>
)=====";
}