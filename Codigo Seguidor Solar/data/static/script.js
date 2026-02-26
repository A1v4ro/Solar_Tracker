// Variables globales
let maxVerticalSpeed = 50;
let maxHorizontalSpeed = 50;
let currentDirection = null;
let chartInstance = null;
let touchActive = false;
let currentChartData = null;
let serverOnline = true;
let isManualMode = false;
let wsClient = null;
let resizeTimeout;
let motorZeroMode = false;
let sensorZeroMode = false;
let currentOperationType = 'control';
let lastSentValues = { 
    h: 0, 
    v: 0, 
    speedH: 0, 
    speedV: 0 
};
let currentMotorOffsets = {
    h_right: 1,
    h_left: 1,
    v_up: 1,
    v_down: 1
};
let currentSensorOffsets = {
    azimut: 0,
    cenit: 0
};
const directions = {
    up:       { dirV: 1 },
    down:     { dirV: 2 },
    left:     { dirH: 2 },
    right:    { dirH: 1 },
    upleft:   { dirV: 1, dirH: 2 },
    upright:  { dirV: 1, dirH: 1 },
    downleft: { dirV: 2, dirH: 2 },
    downright:{ dirV: 2, dirH: 1 }
};

// Inicialización al cargar el DOM
document.addEventListener('DOMContentLoaded', function() {
    initMobileMenu();
    initTimeConfig();
    initMenuNavigation();
    initWiFiForm();
    initOperationType();
    initWebSocket();
    initModeSwitcher();
    initButtonControls();
    initHistorySection();
    checkServerConnection();
    showSection('config');              // pestaña inicial
    getConnectionInfo();
    updateOperationTypeDisplay();
});

// ====================== MENÚ MÓVIL ======================
function initMobileMenu() {
    const toggle = document.querySelector('.menu-toggle');
    const menu = document.querySelector('.mobile-nav');
    
    // Clonar menú del sidebar al móvil
    const desktopMenu = document.querySelector('.menu ul').cloneNode(true);
    menu.appendChild(desktopMenu);
    
    // Asegurar que los ítems del menú móvil tengan los mismos estilos
    menu.querySelectorAll('.menu-item').forEach(item => {
        item.classList.add('menu-item'); 
        const icon = item.querySelector('[data-icon]');
        if (icon) {
            const iconName = icon.getAttribute('data-icon');
            insertIcon(iconName, icon);
        }
    });
    
    toggle.addEventListener('click', function() {
        menu.classList.toggle('active');
        
        // Cambiar ícono
        const icon = toggle.querySelector('svg');
        if (menu.classList.contains('active')) {
            icon.innerHTML = icons.close;
        } else {
            icon.innerHTML = icons.menu;
        }
    });
}

// ====================== NAVEGACIÓN ======================
function initMenuNavigation() {
    const mobileNav = document.querySelector('.mobile-nav');
    const toggleIcon = document.querySelector('.menu-toggle svg');
    
    // Manejo común para todos los menu-item
    document.addEventListener('click', function (e) {
        const item = e.target.closest('.menu-item');
        if (!item) return;

        const section = item.getAttribute('data-section');
        if (!section) return;

        showSection(section);

        // Si el click vino de dentro del menú móvil, se cierra
        if (mobileNav.contains(item)) {
            mobileNav.classList.remove('active');
            if (toggleIcon) toggleIcon.innerHTML = icons.menu;
        }
    });
}

function showSection(sectionId) {
    // Ocultar todas las secciones y Desactivar ítems activos
    document.querySelectorAll('.content-section').forEach(section => {
        section.classList.remove('active');
    }); 
    document.querySelectorAll('.menu-item').forEach(item => {
        item.classList.remove('active');
    });
    
    // Mostrar sección seleccionada y Activar ítem del menú correspondiente
    document.getElementById(sectionId).classList.add('active');
    document.querySelector(`.menu-item[data-section="${sectionId}"]`).classList.add('active');
    
    // Cargar datos si es la sección de historial
    if (sectionId === 'history') {
        loadAvailableDates();
        loadAvailableSensors();
    }
}

// ====================== CONFIGURACIÓN WIFI ======================
function initWiFiForm() {
    const form = document.getElementById('wifi-form');
    if (!form) return;

    form.addEventListener('submit', function(e) {
        e.preventDefault();
        const ssid = document.getElementById('ssid').value;
        const password = document.getElementById('password').value;

        // Verificacion de que el formulario no este vacio
        if (!ssid || !password) {
            showToast('Debe ingresar SSID y contraseña', 'warning');
            return;
        }
        
        showLoading('wifi-submit-btn');

        const data = new URLSearchParams();
        data.append('ssid', ssid);
        data.append('password', password);

        // Envia informacion de la red WiFi al servidor
        fetch('/config-wifi', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: data.toString()
        })
        .then(response => {
            if (!response.ok) throw new Error("Error en la respuesta");
            return response.text();
        })
        .then(message => {
            hideLoading('wifi-submit-btn');
            showToast(message, 'success');  
        })
        .catch(error => {
            hideLoading('wifi-submit-btn');
            showToast('Error de conexión', 'error');
            console.error('Error:', error);
        });
    });
}

// ====================== TIPO DE OPERACION ======================
function initOperationType() {
    const typeSelect = document.getElementById('operation-type-select');
    const controlIntervalContainer = document.getElementById('control-interval-container');
    const logIntervalContainer = document.getElementById('log-interval-container');
    const saveButton = document.getElementById('save-operation-btn');
    
    // Actualiza el tipo de operacion
    function updateUI() {
        const type = typeSelect.value;
        console.log('modo de operacion cambiado a:', type);
        
        switch(type) {
            case 'control':
                controlIntervalContainer.style.display = 'block';
                logIntervalContainer.style.display = 'none';
                syncType = false;
                break;
            case 'control-log':
                controlIntervalContainer.style.display = 'block';
                logIntervalContainer.style.display = 'block';
                syncType = false;
                break;
            case 'sync':
                controlIntervalContainer.style.display = 'none';
                logIntervalContainer.style.display = 'block';
                syncType = true;
                break;
        }
    }
    
    typeSelect.addEventListener('change', updateUI);
    
    // Configurar el botón de guardar
    saveButton.addEventListener('click', function() {
        const selectedType  = typeSelect.value;
        let controlIntervalMs = 0;
        let logIntervalMs = 0;
        
        try {
            // Calcular intervalos según el tipo de operacion
            if (selectedType !== 'sync') {
                const controlValue = parseInt(document.getElementById('control-interval-value').value);
                const controlUnit = document.getElementById('control-interval-unit').value;
                controlIntervalMs = valueAndUnitToMs(controlValue, controlUnit);
                console.log('Control - Valor:', controlValue, 'Unidad:', controlUnit, 'MS:', controlIntervalMs);
                if (controlIntervalMs < 1000 || controlIntervalMs > 3600000) {
                    throw new Error("Intervalo de control debe estar entre 1 segundo y 1 hora");
                }
            }
            
            if (selectedType !== 'control') {
                const logValue = parseInt(document.getElementById('log-interval-value').value);
                const logUnit = document.getElementById('log-interval-unit').value;
                logIntervalMs = valueAndUnitToMs(logValue, logUnit);
                console.log('Log - Valor:', logValue, 'Unidad:', logUnit, 'MS:', logIntervalMs);
                if (logIntervalMs < 10000 || logIntervalMs > 3600000) {
                    throw new Error("Intervalo de registro debe estar entre 10 segundos y 1 hora");
                }
            }
        } catch (error) {
            showToast(error.message, "error");
            console.error('Error en validación:', error.message);
            return;
        }
        
        showLoading('save-operation-btn');
        
        // Enviar al servidor
        const data = new URLSearchParams();
        data.append('type', selectedType);
        data.append('controlInterval', controlIntervalMs);
        data.append('logInterval', logIntervalMs);

        fetch('/set-operation-type', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: data.toString()
        })
        .then(response => {
            if (!response.ok) throw new Error("Error en la respuesta");
            return response.text();
        })
        .then(text => {
            console.log('Servidor respondió:', text);
            hideLoading('save-operation-btn');
            showToast("Configuración guardada", "success");
            
            // Actualizar panel de estado
            currentOperationType = selectedType;
            updateOperationTypeDisplay();
        })
        .catch(error => {
            hideLoading('save-operation-btn');
            showToast("Error al guardar configuración", "error");
            console.error("Error:", error);
        });
    });
    
    updateUI();
    loadCurrentOperationType();
}

// Convertir milisegundos a valor y unidad
function msToValueAndUnit(ms) {
    if (ms >= 3600000) { 
        return { value: ms / 3600000, unit: 'hours' };
    } else if (ms >= 60000) { 
        return { value: ms / 60000, unit: 'minutes' };
    } else { 
        return { value: ms / 1000, unit: 'seconds' };
    }
}

// Convertir valor y unidad a milisegundos
function valueAndUnitToMs(value, unit) {
    switch(unit) {
        case 'seconds': return value * 1000;
        case 'minutes': return value * 60000;
        case 'hours': return value * 3600000;
        default: throw new Error("Unidad de tiempo no válida");
    }
}

// Actualizar UI con valores cargados
function updateIntervalUI(controlMs, logMs) {
    if (controlMs > 0) {
        const control = msToValueAndUnit(controlMs);
        document.getElementById('control-interval-value').value = Math.round(control.value);
        document.getElementById('control-interval-unit').value = control.unit;
    }
    
    if (logMs > 0) {
        const log = msToValueAndUnit(logMs);
        document.getElementById('log-interval-value').value = Math.round(log.value);
        document.getElementById('log-interval-unit').value = log.unit;
    }
}

// Carga las configuraciones del tipo de operacion
function loadCurrentOperationType() {
    // Solicita informacion
    fetch('/get-operation-type')
        .then(response => {
            if (!response.ok) throw new Error("Error en la respuesta");
            return response.json();
        })
        .then(data => {
            console.log('Datos recibidos del servidor:', data);
            document.getElementById('operation-type-select').value = data.type;
            
            // Actualizar UI y el panel de estado
            updateIntervalUI(data.controlInterval, data.logInterval);
            currentOperationType = data.type;
            updateOperationTypeDisplay();
            
            // Actualizar UI de los contenedores
            const event = new Event('change');
            document.getElementById('operation-type-select').dispatchEvent(event);
        })
        .catch(error => {
            console.error("Error cargando tipo de operación:", error);
            showToast("Error al cargar configuración", "error");
        });
}

// ====================== CONFIGURACION HORARIA ======================
function initTimeConfig() {
    const updateBtn = document.getElementById('update-time-btn');
    if (!updateBtn) return;

    // Mostrar fecha y hora actual del dispositivo
    updateLocalDateTime();

    // Configurar evento para el botón de actualización
    updateBtn.addEventListener('click', function() {
        const datetimeInput = document.getElementById('datetime-local').value;
        
        if (!datetimeInput) {
            showToast('Selecciona una fecha y hora válida', 'warning');
            return;
        }

        // Formatear para enviar al servidor (formato: YYYY-MM-DDTHH:MM:SS)
        const datetime = new Date(datetimeInput);
        const timestamp = Math.floor(datetime.getTime() / 1000);

        showLoading('update-time-btn');

        // Envia informacion al servidor
        fetch('/set-time', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: `datetime=${encodeURIComponent(timestamp)}`
        })
        .then(response => {
            if (!response.ok) throw new Error("Error en la respuesta");
            return response.text();
        })
        .then(text => {
            hideLoading('update-time-btn');
            showToast('Hora actualizada correctamente', 'success');
            document.getElementById('time-status-message').textContent = 
                `Hora actualizada: ${formatDateTimeForDisplay(datetime)}`;
        })
        .catch(error => {
            hideLoading('update-time-btn');
            showToast('Error al actualizar la hora', 'error');
            console.error('Error:', error);
        });
    });

    // Actualizar cada segundo para mantener la hora actual
    setInterval(updateLocalDateTime, 1000);
}

// Mostrar la hora local directamente (el navegador ya usa la zona horaria del usuario)
function updateLocalDateTime() {
    const now = new Date();
    const year = now.getFullYear();
    const month = String(now.getMonth() + 1).padStart(2, '0');
    const day = String(now.getDate()).padStart(2, '0');
    const hours = String(now.getHours()).padStart(2, '0');
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const seconds = String(now.getSeconds()).padStart(2, '0');
    
    document.getElementById('datetime-local').value = `${year}-${month}-${day}T${hours}:${minutes}:${seconds}`;
}

// Dar formato a la hora
function formatDateTimeForDisplay(date) {
    return date.toLocaleString('es-ES', {
        day: '2-digit',
        month: '2-digit',
        year: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
}

// ====================== CONTROL DE MODOS ======================
function initModeSwitcher() {
    document.querySelectorAll('input[name="operation-mode"]').forEach(radio => {
        radio.addEventListener('change', function() {
            const selectedMode = this.value === 'manual' ? 'manual' : 'auto';
            isManualMode = selectedMode === 'manual';

            updateModeDisplay(isManualMode);
            
            if (wsClient && wsClient.readyState === WebSocket.OPEN) {
                wsClient.send(JSON.stringify({
                    type: "set_mode",
                    mode: selectedMode
                }));
            }
        });
    });
    updateModeDisplay(isManualMode);
}

// ====================== INICIALIZAR WEBSOCKET ======================
function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
    const wsUrl = protocol + window.location.hostname + '/ws';
    
    wsClient = new WebSocket(wsUrl);
    
    wsClient.onopen = function() {
        console.log('WebSocket conectado');
        serverOnline = true;
        updateConnectionStatus();
        wsClient.send(JSON.stringify({
            type: "get_mode"
        }));
    };
    
    wsClient.onclose = function() {
        console.log('WebSocket desconectado');
        serverOnline = false;
        updateConnectionStatus();
        // Reconectar después de 2 segundos
        setTimeout(initWebSocket, 2000);
    };
    
    wsClient.onerror = function(error) {
        console.error('Error en WebSocket:', error);
    };
    
    // Procesa el tipo de mensaje recibido
    wsClient.onmessage = function(event) {
        console.log("Mensaje recibido del servidor:", event.data);
        const data = JSON.parse(event.data);
        if (data.type === "motor_ack") {
            console.log("Motor confirmado por el servidor");
        } 
        else if (data.type === "mode_ack") {
            const isManual = data.mode === "manual";
            updateModeDisplay(isManual);
        }
        else if (data.type === "offset_motor_update") {
            const motor = data.motor;
            const value = data.value;
            currentMotorOffsets[motor] = value;
            updateMotorOffsetDisplays();
            console.log(`Offset de motor actualizado por otro cliente: ${motor} = ${value}%`);
        }
        else if (data.type === "offset_sensor_update") {
            const sensor = data.sensor;
            const value = data.value;
            currentSensorOffsets[sensor] = value;
            updateSensorOffsetDisplays();
            console.log(`Offset de sensor actualizado por otro cliente: ${sensor} = ${value}%`);
        }
    };
}

function updateModeDisplay(isManual) {
    isManualMode = isManual;
    
    // Actualizar controles UI
    const manualControls = document.getElementById('manual-controls');
    manualControls.style.display = isManual ? 'block' : 'none';

    enableOffsetEditing(isManual);
    
    document.getElementById('system-mode').textContent = isManual ? 'Manual' : 'Automático';
    
    // Marcar el radio button correspondiente
    const radio = document.querySelector(`input[name="operation-mode"][value="${isManual ? 'manual' : 'auto'}"]`);
    if (radio) radio.checked = true;
    
    // Si estamos en automático, asegurarse de que los motores están detenidos
    if (!isManual && currentDirection) {
        currentDirection = null;
        sendMotorCommand();
    }
}

function enableOffsetEditing(enable) {
    const motorSelect = document.getElementById('motor-select-offset');
    const sensorSelect = document.getElementById('sensor-select-offset');
    const motorZeroCheckbox = document.getElementById('motor-zero-checkbox');
    const sensorZeroCheckbox = document.getElementById('sensor-zero-checkbox');
    const saveMotorOffsetBtn = document.getElementById('save-motor-offset-btn');
    const saveSensorOffsetBtn = document.getElementById('save-sensor-offset-btn');
    
    // Elementos a ocultar/mostrar
    const formGroups = document.querySelectorAll('.offset-controls .form-group');
    const checkboxGroups = document.querySelectorAll('.offset-controls .checkbox-group');
    const offsetButtons = document.querySelectorAll('#save-motor-offset-btn, #save-sensor-offset-btn');
    const statusMessages = document.querySelectorAll('#motor-offset-status-message, #sensor-offset-status-message');
    
    if (enable) {
        // Modo manual: mostrar todos los controles de edición
        formGroups.forEach(el => el.style.display = 'block');
        checkboxGroups.forEach(el => el.style.display = 'block');
        offsetButtons.forEach(el => el.style.display = 'inline-flex');
        statusMessages.forEach(el => el.style.display = 'block');
        
        // Habilitar elementos
        if (motorSelect) motorSelect.disabled = false;
        if (sensorSelect) sensorSelect.disabled = false;
        if (motorZeroCheckbox) motorZeroCheckbox.disabled = false;
        if (sensorZeroCheckbox) sensorZeroCheckbox.disabled = false;
        if (saveMotorOffsetBtn) saveMotorOffsetBtn.disabled = false;
        if (saveSensorOffsetBtn) saveSensorOffsetBtn.disabled = false;
        
    } else {
        // Modo automático: ocultar controles de edición
        formGroups.forEach(el => el.style.display = 'none');
        checkboxGroups.forEach(el => el.style.display = 'none');
        offsetButtons.forEach(el => el.style.display = 'none');
        statusMessages.forEach(el => el.style.display = 'none');
        
        // Deshabilitar elementos (por seguridad)
        if (motorSelect) motorSelect.disabled = true;
        if (sensorSelect) sensorSelect.disabled = true;
        if (motorZeroCheckbox) motorZeroCheckbox.disabled = true;
        if (sensorZeroCheckbox) sensorZeroCheckbox.disabled = true;
        if (saveMotorOffsetBtn) saveMotorOffsetBtn.disabled = true;
        if (saveSensorOffsetBtn) saveSensorOffsetBtn.disabled = true;
    }
    
    // Añadir clase visual para indicar estado de solo lectura
    const offsetSections = document.querySelectorAll('.offset-controls');
    offsetSections.forEach(section => {
        if (enable) {
            section.classList.remove('readonly-mode');
        } else {
            section.classList.add('readonly-mode');
        }
    });
}

// ====================== JOYSTICK Y CONTROL DE MOTORES ======================
function initButtonControls() {
    const verticalSpeedSlider = document.getElementById('vertical-speed');
    const horizontalSpeedSlider = document.getElementById('horizontal-speed');
    
    // Mostrar valores iniciales
    updateSpeedDisplays();

    // Eventos para los sliders de velocidad
    verticalSpeedSlider.addEventListener('input', function() {
        maxVerticalSpeed = parseInt(this.value);
        updateSpeedDisplays();
        if (currentDirection) sendMotorCommand();
    });
    
    horizontalSpeedSlider.addEventListener('input', function() {
        maxHorizontalSpeed = parseInt(this.value);
        updateSpeedDisplays();
        if (currentDirection) sendMotorCommand();
    });

    // Eventos táctiles y de ratón 
    document.querySelectorAll('.dpad-btn').forEach(btn => {
        const direction = btn.getAttribute('data-action');
        
        // Eventos para ratón
        btn.addEventListener('mousedown', (e) => {
            if (touchActive) return;            
            handleDirectionStart(direction, btn);
        });
        
        btn.addEventListener('mouseup', () => {
            if (touchActive) return;
            handleDirectionEnd(direction, btn);
        });
        
        btn.addEventListener('mouseleave', () => {
            if (touchActive) return;
            handleDirectionEnd(direction, btn);
        });

        // Eventos para pantalla táctil
        btn.addEventListener('touchstart', (e) => {
            e.preventDefault();
            touchActive = true;
            handleDirectionStart(direction, btn);
        }, { passive: false });
        
        btn.addEventListener('touchend', () => {
            touchActive = false;
            handleDirectionEnd(direction, btn);
        });
        
        btn.addEventListener('touchcancel', () => {
            touchActive = false;
            handleDirectionEnd(direction, btn);
        });
    });

    // Detener motores si la ventana pierde foco
    window.addEventListener('blur', () => {
        if (currentDirection) {
            currentDirection = null;
            sendMotorCommand();
        }
    });

    // Configurar botón de guardar offset del motor
    document.getElementById('save-motor-offset-btn').addEventListener('click', function() {
        const motorDir = document.getElementById('motor-select-offset').value;

        // Determinar el valor basado en el checkbox
        let valueToSave;
        if (motorZeroMode) {
            valueToSave = 1; // Modo cero activado
        } else {
            // Valor normal basado en la velocidad actual
            valueToSave = motorDir.includes('h') ? maxHorizontalSpeed : maxVerticalSpeed;
        }
        
        showLoading('save-motor-offset-btn');
        
        // Actualizar valor local
        currentMotorOffsets[motorDir] = valueToSave;
        updateMotorOffsetDisplays();
        
        // Enviar al servidor
        fetch('/set-motors-offset', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: `motor=${motorDir}&value=${valueToSave}`
        })
        .then(response => {
            if (!response.ok) throw new Error("Error en la respuesta");
            return response.text();
        })
        .then(text => {
            hideLoading('save-motor-offset-btn');

            let message = `Offset ${motorDir} guardado: ${valueToSave}%`;
            if (motorZeroMode) {
                message += " Offset Borrado";
            }

            showToast(message, "success");
            document.getElementById('motor-offset-status-message').textContent = 
                `Offset guardado: ${valueToSave}%`;
        })
        .catch(error => {
            hideLoading('save-motor-offset-btn');
            showToast("Error al guardar offset", "error");
            console.error("Error:", error);
        });
    });

    // Configurar botón de guardar offset de sensor
    document.getElementById('save-sensor-offset-btn').addEventListener('click', function() {
        const sensor = document.getElementById('sensor-select-offset').value;
        
        fetch('/read-sensor-offsets')
        .then(response => response.json())
        .then(sensorData => {
            let valueToSave;
            if (sensorZeroMode) {
                valueToSave = 0; // Modo cero activado
            } else {
                // Usar el valor actual del sensor seleccionado
                if (sensor === "azimut") {
                    valueToSave = sensorData.azimut;
                } else if (sensor === "cenit") {
                    valueToSave = sensorData.cenit;
                } else {
                    valueToSave = 0; // Valor por defecto si no se encuentra
                }
            }
            
            showLoading('save-sensor-offset-btn');
            
            // Actualizar valor local
            currentSensorOffsets[sensor] = valueToSave;
            updateSensorOffsetDisplays();
            
            // Enviar al servidor
            return fetch('/set-sensor-offset', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `sensor=${sensor}&value=${valueToSave}`
            });
        })
        .then(text => {
            hideLoading('save-sensor-offset-btn');

            let message = `Offset ${sensor} guardado: ${currentSensorOffsets[sensor]}`;
            if (sensorZeroMode) {
                message += " Offset Borrado";
            }

            showToast(message, "success");
            document.getElementById('sensor-offset-status-message').textContent = 
                `Offset guardado: ${currentSensorOffsets[sensor]}`;
        })
        .catch(error => {
            hideLoading('save-sensor-offset-btn');
            showToast("Error al guardar offset del sensor", "error");
            console.error("Error:", error);
        });
    });

    // Inicializar checkboxes
    initCheckboxes();
    
    // Cargar offsets actuales al iniciar
    loadCurrentMotorOffsets();
    loadCurrentSensorOffsets();
}

function updateMotorOffsetDisplays() {
    document.getElementById('current-h-right').textContent = currentMotorOffsets.h_right;
    document.getElementById('current-h-left').textContent = currentMotorOffsets.h_left;
    document.getElementById('current-v-up').textContent = currentMotorOffsets.v_up;
    document.getElementById('current-v-down').textContent = currentMotorOffsets.v_down;
}

function updateSensorOffsetDisplays() {
    document.getElementById('current-sensor-azimut').textContent = currentSensorOffsets.azimut;
    document.getElementById('current-sensor-cenit').textContent = currentSensorOffsets.cenit;
}

function loadCurrentMotorOffsets() {
    fetch('/get-motors-offsets')
        .then(response => response.json())
        .then(data => {
            currentMotorOffsets = {
                h_right: data.h_right,
                h_left: data.h_left,
                v_up: data.v_up,
                v_down: data.v_down
            };
            updateMotorOffsetDisplays();
        })
        .catch(error => {
            console.error("Error cargando offsets:", error);
        });
}

function loadCurrentSensorOffsets() {
    fetch('/get-sensor-offsets')
        .then(response => response.json())
        .then(data => {
            currentSensorOffsets = {
                azimut: data.azimut,
                cenit: data.cenit
            };
            updateSensorOffsetDisplays();
        })
        .catch(error => {
            console.error("Error cargando offsets de sensores:", error);
            showToast("Error al cargar offsets de sensores", "error");
        });
}

function updateSpeedDisplays() {
    document.getElementById('vertical-speed-value').textContent = `${maxVerticalSpeed}%`;
    document.getElementById('horizontal-speed-value').textContent = `${maxHorizontalSpeed}%`;
    if (currentDirection) sendMotorCommand();
}

function initCheckboxes() {
    // Checkbox para motor zero mode
    document.getElementById('motor-zero-checkbox').addEventListener('change', function() {
        motorZeroMode = this.checked;
        console.log("Motor zero mode:", motorZeroMode);
    });
    
    // Checkbox para sensor zero mode
    document.getElementById('sensor-zero-checkbox').addEventListener('change', function() {
        sensorZeroMode = this.checked;
        console.log("Sensor zero mode:", sensorZeroMode);
    });
}

// Nuevas funciones auxiliares para manejo de direcciones
function handleDirectionStart(direction, btn) {
    currentDirection = direction;
    btn.classList.add('active');
    sendMotorCommand();
}

function handleDirectionEnd(direction, btn) {
    if (currentDirection === direction) {
        currentDirection = null;
        btn.classList.remove('active');
        sendMotorCommand();
    }
}

// Envio de comandos de accionamiento de motores por webSocket
function sendMotorCommand() {
    if (!isManualMode || !wsClient || wsClient.readyState !== WebSocket.OPEN) return;

    const d = directions[currentDirection] || {};
    const command = {
        type: "motor",
        dirH: d.dirH || 0,
        dirV: d.dirV || 0,
        speedH: (d.dirH ? maxHorizontalSpeed : 0),
        speedV: (d.dirV ? maxVerticalSpeed : 0)
    };

    if (JSON.stringify(command) !== JSON.stringify(lastSentValues)) {
        lastSentValues = {...command};
        console.log("Enviando comando:", command);
        wsClient.send(JSON.stringify(command));
    }
}

// ====================== SECCIÓN DE HISTORIAL ======================
function initHistorySection() {
    const loadBtn = document.getElementById('load-data');
    if (!loadBtn) return;

    loadBtn.addEventListener('click', function() {
        const date = document.getElementById('date-select').value;
        const sensor = document.getElementById('sensor-select').value;
        
        if (!date || !sensor) {
            showToast('Selecciona fecha y sensor', 'warning');
            return;
        }

        showLoading('load-data');
        
        fetch(`/sensor-data?date=${date}&sensor=${sensor}`)
            .then(response => response.json())
            .then(data => {
                currentChartData = data;
                renderChart(data);
                hideLoading('load-data');
            })
            .catch(error => {
                hideLoading('load-data');
                showToast('Error al cargar datos', 'error');
                console.error('Error:', error);
            });
    });
}

// Cargar fechas disponibles
function loadAvailableDates() {
    fetch('/dates')
        .then(response => response.json())
        .then(data => {
            const select = document.getElementById('date-select');
            select.innerHTML = '';
            
            if (data.original.length === 0) {
                select.innerHTML = '<option value="">No hay datos</option>';
                return;
            }
            
            // Función para parsear YYYYMMDD a Date
            const fragment = document.createDocumentFragment();

            const dates = data.original.map((dateStr, index) => {
                const year = +dateStr.slice(0, 4);
                const month = +dateStr.slice(4, 6) - 1;
                const day = +dateStr.slice(6, 8);
                return {
                    original: dateStr,
                    display: data.display[index],
                    timestamp: new Date(year, month, day).getTime()
                };
            }).sort((a, b) => b.timestamp - a.timestamp); // Orden descendente

            // Agregar al menu de opciones
            dates.forEach(date => {
                const option = document.createElement('option');
                option.value = date.original;
                option.textContent = date.display;
                fragment.appendChild(option);
            });
            select.appendChild(fragment);
        })
        .catch(error => {
            console.error('Error:', error);
            document.getElementById('date-select').innerHTML = '<option value="">Error al cargar</option>';
        });
}

// Cargar las etiquetas de los sensores
function loadAvailableSensors() {
    fetch('/sensors')
        .then(response => response.json())
        .then(data => {
            const select = document.getElementById('sensor-select');
            select.innerHTML = '';
            
            const fragment = document.createDocumentFragment();

            data.sensors.forEach(sensor => {
                const option = document.createElement('option');
                option.value = sensor.id;
                option.textContent = sensor.name;
                fragment.appendChild(option);
            });

            select.appendChild(fragment);
        });
}

// Graficar los datos de una fecha
function renderChart(data) {
    const chartCanvas = document.getElementById('sensor-chart');
    const ctx = chartCanvas.getContext('2d');
    
    // Verifica si hay datos anteriores
    if (!chartInstance) {
        chartInstance = new Chart(ctx, {
            type: 'line',
            data: {
                labels: data.labels,
                datasets: [{
                    label: 'Valor del Sensor',
                    data: data.values,
                    borderColor: 'rgb(192, 75, 120)',
                    tension: 0.1,
                    fill: false
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false, 
                animation: {duration: 1000, easing: 'easeInOutQuart'},
                plugins: { legend: { display: false } },
                scales: {
                    x: { title: { display: true, text: 'Hora' } },
                    y: { title: { display: true, text: 'Valor' } }
                }
            }
        });
    } else {
        // Actualiza datos sin destruir
        chartInstance.data.labels = data.labels;
        chartInstance.data.datasets[0].data = data.values;
        chartInstance.update();
    }
}
// ====================== FUNCIONES AUXILIARES ======================

// Función para obtener información de conexión
function getConnectionInfo() {
    fetch('/connection-info')
        .then(response => response.json())
        .then(data => {
            updateConnectionDisplay(data);
        })
        .catch(error => {
            console.error('Error obteniendo información de conexión:', error);
        });
}

// Función para actualizar la visualización
function updateConnectionDisplay(data) {
    // Actualizar configuraciones de coneccion
    document.getElementById('ip-address').textContent = data.ip || '-';
    document.getElementById('network-ssid').textContent = data.ssid || '-';
    const wifiStatus = document.getElementById('wifi-status');
    if (data.mode === 'AP') {
        wifiStatus.textContent = 'Modo AP';
        wifiStatus.style.color = 'orange';
    } else {
        wifiStatus.textContent = 'Conectado';
        wifiStatus.style.color = 'green';
    }
}
// Actualizar estado de conexion 
function updateConnectionStatus() {
    const statusElement = document.getElementById('wifi-status');
    
    if (serverOnline) {
        statusElement.textContent = 'Conectado';
        statusElement.style.color = 'green';
    } else {
        statusElement.textContent = 'Desconectado';
        statusElement.style.color = 'red';
    }
}
// Actualizar tipo de operacion 
function updateOperationTypeDisplay() {
    const operationTypeElement = document.getElementById('operation-type');
    
    switch(currentOperationType) {
        case 'control':
            operationTypeElement.textContent = 'Solo Control';
            operationTypeElement.style.color = 'green'; 
            break;
        case 'control-log':
            operationTypeElement.textContent = 'Control + Log';
            operationTypeElement.style.color = 'blue'; 
            break;
        case 'sync':
            operationTypeElement.textContent = 'Sincronización';
            operationTypeElement.style.color = 'orange'; 
            break;
        default:
            operationTypeElement.textContent = 'Desconocido';
            operationTypeElement.style.color = 'yelow'; 
    }
}

// Función para verificar conexión
function checkServerConnection() {
    fetch('/ping')
    .then(response => {
        if (!response.ok) throw new Error("Servidor no responde");
        serverOnline = true;
        updateConnectionStatus();
        return true;
    })
    .catch(error => {
        serverOnline = false;
        updateConnectionStatus();
        return false;
    });
}

// Icono de spinner (al enviar o solicitar informacion)
function showLoading(buttonId) {
    const btn = document.getElementById(buttonId);
    if (!btn) return;
    
    btn.disabled = true;
    const originalHTML = btn.innerHTML;
    btn.setAttribute('data-original-html', originalHTML);
    btn.innerHTML = `<span data-icon="spinner"></span> ${btn.textContent.trim()}`;
    updateIconsInContainer(btn);
}

// Icono original (luego de recibir o procesar)
function hideLoading(buttonId) {
    const btn = document.getElementById(buttonId);
    if (!btn) return;
    
    btn.disabled = false;
    const originalHTML = btn.getAttribute('data-original-html');
    if (originalHTML) {
        btn.innerHTML = originalHTML;
        updateIconsInContainer(btn);
    }
}

// Mensajes visuales
function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    document.body.appendChild(toast);
    
    setTimeout(() => toast.classList.add('show'), 10);
    setTimeout(() => {
        toast.classList.remove('show');
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

// Redibujar grafica
function handleChartResize() {
    clearTimeout(resizeTimeout);
    resizeTimeout = setTimeout(() => {
        if (chartInstance && currentChartData) {
            chartInstance.resize();
        }
    }, 200);
}

// ====================== EVENT LISTENERS GLOBALES ======================
window.addEventListener('resize', handleChartResize);
window.addEventListener('orientationchange', handleChartResize);
setInterval(checkServerConnection, 10000);