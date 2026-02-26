#include <Arduino.h>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <freertos/queue.h>

#include "DebugLog.h"
#include "MemoryData.h"
#include "DataLogger.h"
#include "WiFiManager.h"
#include "WebInterface.h"
#include "MotorsManager.h"
#include "SensorManager.h"
#include "OTAManager.h"
#include "fuzzyControl.h"

#define ADC_DOWN 35         // Pin analógico LDR inferior
#define ADC_RIGHT 36        // Pin analógico LDR derecho
#define ADC_UP 39           // Pin analógico LDR superior
#define ADC_LEFT 34         // Pin analógico LDR izquierdo

#define DIR_H 25            // Pin para dirección horizontal
#define SPEED_H 26          // Pin PWM horizontal
#define DIR_V  27           // Pin para dirección vertical
#define SPEED_V 14          // Pin PWM vertical
#define END_V 13            // Fin de carrera maximo vertical

#define CS_SD 5             // Pin de SD
#define END_H 17            // Fin de carrera maximo horizontal
#define ENABLE_HMI 16       // Pin para control del HMI (servidor)
#define START_V 4           // Fin de carrera inicio vertical
#define LED_PIN 0           // Piloto de estado
#define SET_MODE 15         // Pulsador de modo

#define TASK_START_SIGNAL  1
#define TASK_STOP_SIGNAL   2
#define TASK_UPDATE_INTERVAL 3
#define WEB_SERVER 80

volatile unsigned long newControlInterval;
enum LedMode {
    LED_AUTOMATIC,      // Parpadeo lento 
    LED_MANUAL,         // Parpadeo medio 
    LED_HOMING          // Parpadeo rápido 
};
volatile LedMode currentLedMode;                        // Modo de parpadeo de led
volatile bool azimutDirection = false;                   // ultima direccion registrada por el final de carrera en azimut (false = antihorario, true = horario)
volatile bool isHoming = false;

TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t controlTaskHandle = NULL;
TaskHandle_t loggingTaskHandle = NULL;
TaskHandle_t taskManagerHandle = NULL;
TaskHandle_t webControlHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t positionTaskHandle = NULL;

AsyncWebServer server(WEB_SERVER);
MemoryData memory;
MotorsManager motors(DIR_H, SPEED_H, DIR_V, SPEED_V);
SensorManager sensors(ADC_UP, ADC_DOWN, ADC_RIGHT, ADC_LEFT);
DataLogger logger;
WiFiManager wifiManager(memory);
WebInterface webInterface(server, logger, wifiManager, memory, motors, sensors);
OTAManager otaManager(&server);

// ************************
//  SINCRONIZACION DE TIEMPO (uint32_t para tiempo Unix se desbordará en 2038)
// ************************
uint32_t syncRTC() {
    uint32_t intervalSec = memory.logInterval / 1000;
    DateTime now = logger.getCurrentTime();
    uint32_t nowUnix = now.unixtime();

    // Calcular próximo intervalo exacto
    uint32_t nextLogUnix = ((nowUnix / intervalSec) + 1) * intervalSec;
    uint32_t remainingTimeMs = (nextLogUnix - nowUnix) * 1000;

    return remainingTimeMs;
};

// ************************
//  GESTION DEL HMI 
// ************************
void webControlTask(void *pvParameters) {
    static bool currentWebState = false;
    bool webState;                     // Estados de activacion de wifi
    uint32_t notifiedValue = 0;
    for (;;) {
        xTaskNotifyWait(0x00, 0xFFFFFFFF, &notifiedValue, portMAX_DELAY);

        // Notificación desde ISR 
        if (notifiedValue == 1) {
            DBG_PRINTLN("[HMI] Notificación desde ISR");
            webState = digitalRead(ENABLE_HMI); 
            if (webState && !currentWebState) {
                wifiManager.begin();
                webInterface.begin();
                otaManager.begin();
                currentWebState = true;
                DBG_PRINTLN("[HMI] WebServer ACTIVADO");
            } else if (!webState && currentWebState) {
                webInterface.stopWeb();
                wifiManager.disconnect();
                currentWebState = false;
                DBG_PRINTLN("[HMI] WebServer DESACTIVADO");
            }
        }
        // Notificación desde Web
        else if (notifiedValue == 2){
            DBG_PRINTLN("[HMI] Notificación desde Web");
            wifiManager.startClientMode(); 
        }
    }
}

// ************************
//  TAREA DE REGISTRO DE DATOS
// ************************
void loggingTask(void *pvParameters) {
    TickType_t logDelay;
    uint32_t notifiedValue = 0;
    TickType_t startTick = 0, endTick = 0, waitTicks = 0;

    for (;;) {
        DBG_PRINTF("[LOG] Esperando señal de inicio...\n");

        // Espera hasta recibir una señal START
        xTaskNotifyWait(0x00, 0xFFFFFFFF, &notifiedValue, portMAX_DELAY);

        if (notifiedValue != TASK_START_SIGNAL && notifiedValue != TASK_UPDATE_INTERVAL) continue;
        DBG_PRINTF("[LOG] Registro iniciado.\n");

        // Sincronización inicial con RTC
        logDelay = pdMS_TO_TICKS(syncRTC());
        DBG_PRINTF("[LOG] Sincronizando con RTC, esperando %lu ms\n", pdTICKS_TO_MS(logDelay));

        // Bucle de registro periódico
        for (;;) {
            BaseType_t notified = xTaskNotifyWait(0x00, 0xFFFFFFFF, &notifiedValue, logDelay);
            startTick = xTaskGetTickCount();
            if (notified == pdTRUE) {
                if (notifiedValue == TASK_STOP_SIGNAL) {
                    DBG_PRINTF("[LOG] Registro detenido.\n");
                    break; // vuelve a la espera del próximo START
                } else if (notifiedValue == TASK_UPDATE_INTERVAL) {
                    logDelay = pdMS_TO_TICKS(syncRTC());
                    DBG_PRINTF("[LOG] Sincronizando con RTC, esperando %lu ms\n", pdTICKS_TO_MS(logDelay));
                    continue; // finaliza la iteracion del for actual
                }
            }
            logDelay = pdMS_TO_TICKS(memory.logInterval);

            DateTime  currentTime = logger.getCurrentTime();                    // Obtener la hora actual
            std::vector<float> sensorData = sensors.readAllSensors();
            logger.saveSensorData(currentTime, sensorData);
            DBG_PRINTF("[LOG] Datos guardados: %02d/%02d %02d:%02d, Ecenit: %d, Eazimut: %d\n",
                        currentTime.day(), currentTime.month(),
                        currentTime.hour(), currentTime.minute(),
                        sensors.getEcenit(), sensors.getEazimut());
            endTick = xTaskGetTickCount();
            logDelay = logDelay - endTick + startTick;
        }
    }
}

// ************************
//  SINCRONIZACION PARA EL CONTROL 
// ************************
TickType_t controlSync(){
    const uint32_t controlTime = 10000;
    if (memory.operationType == "sync"){
        uint32_t syncDelayMs = syncRTC();
        return pdMS_TO_TICKS(syncDelayMs > controlTime ? syncDelayMs - controlTime : syncDelayMs + memory.logInterval - controlTime);
    } else return pdMS_TO_TICKS(memory.controlInterval);
}

// ************************
//  HTAREA DE CONTROL DIFUSO (MODO AUTOMATICO) 
// ************************
void controlTask(void *pvParameters) {
    TickType_t controlDelay;
    uint32_t notifiedValue = 0;
    const TickType_t sampleTime = pdMS_TO_TICKS(5);
    float Dcenit = 0, Dazimut = 0;
    float Ecenit = 0, Eazimut = 0;
    double outCenit = 0, outAzimut = 0;                           
    float Ecenit_old = 0, Eazimut_old = 0;
    TickType_t startTick = 0, endTick = 0, waitTicks = 0;
    float dt, currentTime = 0, lastTime;
    uint8_t speed_cenit, speed_azimut, dir_cenit, dir_azimut;
    const float deadZone = 1.0f;      // rango de -1 a 1
    const uint8_t deadZoneLimit = 7;  // lecturas consecutivas para parar
    uint8_t deadZoneCounter = 0;

    for (;;) {
        DBG_PRINTF("[CTRL] Esperando señal de inicio...\n");

        // Espera hasta recibir una señal START
        xTaskNotifyWait(0x00, 0xFFFFFFFF, &notifiedValue, portMAX_DELAY);

        if (notifiedValue != TASK_START_SIGNAL) continue;
        DBG_PRINTF("[CTRL] Control iniciado.\n");

        controlDelay = controlSync();

        for (;;) {
            startTick = xTaskGetTickCount();
            deadZoneCounter = 0;
            lastTime = millis();

            while (deadZoneCounter <= deadZoneLimit) {
                if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notifiedValue, 0) == pdTRUE) {
                    if (notifiedValue == TASK_STOP_SIGNAL) {
                        motors.stop();
                        DBG_PRINTF("[CTRL] Paro de Emergencia\n");
                        goto wait_for_start;
                    }
                }
                sensors.updateSolarSensor();
                Ecenit = sensors.getEcenit() - sensors.offsetCenit;
                Eazimut = sensors.getEazimut() - sensors.offsetAzimut;
                DBG_PRINTF("Ecenit: %f, Eazimut: %f\n", Ecenit, Eazimut); 
                currentTime = millis();
                dt = (currentTime - lastTime) / 1000.0f;
                lastTime = currentTime;

                Dcenit = (Ecenit - Ecenit_old) / dt;
                Dazimut = (Eazimut - Eazimut_old) / dt;
                Ecenit_old = Ecenit;
                Eazimut_old = Eazimut;

                fuzzyControlInferenceEngine(Ecenit, Dcenit, Eazimut, Dazimut, &outCenit, &outAzimut);

                speed_cenit  = (uint8_t) round(fabs(outCenit));
                speed_azimut = (uint8_t) round(fabs(outAzimut));
                dir_cenit  = (outCenit >= 0) ? 2 : 1;
                dir_azimut = (outAzimut >= 0) ? 1 : 2;
                DBG_DATA("%.2f,%.2f,%.2f,%.2f,%u,%u,%.6f,%.6f\n", Ecenit, Eazimut,Dcenit,Dazimut,speed_azimut,speed_cenit, outAzimut, outCenit);

                motors.move(dir_azimut, dir_cenit, speed_azimut, speed_cenit);
                vTaskDelay(sampleTime);
                deadZoneCounter = (fabs(Ecenit) <= deadZone && fabs(Eazimut) <= deadZone) ? deadZoneCounter + 1 : 0;
            }
            motors.stop();
            DBG_PRINTF("[CTRL] Zona muerta encontrada\n");
            endTick = xTaskGetTickCount();
            waitTicks = controlDelay - endTick + startTick;
            DBG_PRINTF("[CTRL] Ciclo completo: ejecucion = %lu ms, espera = %lu ms\n",
                    pdTICKS_TO_MS(endTick - startTick), pdTICKS_TO_MS(waitTicks));
            
            BaseType_t notified = xTaskNotifyWait(0x00, 0xFFFFFFFF, &notifiedValue, waitTicks);
            if (notified == pdTRUE) {
                if (notifiedValue == TASK_STOP_SIGNAL) {
                    DBG_PRINTF("[CTRL] Cambio de modo detectado.\n");
                    motors.stop();
                    goto wait_for_start; // vuelve a la espera del próximo START
                } else if (notifiedValue == TASK_UPDATE_INTERVAL) {
                    controlDelay = controlSync();
                    DBG_PRINTF("[CTRL] Nuevo intervalo: %lu ms\n", pdTICKS_TO_MS(controlDelay));
                }
            } else controlDelay = newControlInterval;
        }
        wait_for_start:;
    }
}

// *****************************************
//  TAREA ADMINISTRADOR DE TAREAS
// *****************************************
void taskManager(void *pvParameters) {
    uint32_t notifiedValue = 0;

    for (;;) {
        // Espera notificación para reevaluar el modo actual
        xTaskNotifyWait(0x00, 0xFFFFFFFF, &notifiedValue, portMAX_DELAY);

        DBG_PRINTF("[TYPE] Evaluando tipo de operación: %s\n", memory.operationType.c_str());
        newControlInterval = memory.controlInterval;

        // --- Control de tipos de operacion ---
        if (memory.operationType == "control") {
            // Solo control (sin sincronización)
            xTaskNotify(loggingTaskHandle, TASK_STOP_SIGNAL, eSetValueWithOverwrite);
            xTaskNotify(controlTaskHandle, TASK_UPDATE_INTERVAL, eSetValueWithOverwrite);
            DBG_PRINTF("[TYPE] Tipo CONTROL activado.\n");
        }
        else if (memory.operationType == "control-log") {
            // Log + control (con sincronización)
            xTaskNotify(loggingTaskHandle, TASK_UPDATE_INTERVAL, eSetValueWithOverwrite);
            xTaskNotify(controlTaskHandle, TASK_UPDATE_INTERVAL, eSetValueWithOverwrite);
            DBG_PRINTF("[TYPE] Tipo CONTROL + LOG activado.\n");
        }
        else if (memory.operationType == "sync") {
            // Corrección antes del registro (sincronizada)
            newControlInterval = memory.logInterval;
            xTaskNotify(loggingTaskHandle, TASK_UPDATE_INTERVAL, eSetValueWithOverwrite);
            xTaskNotify(controlTaskHandle, TASK_UPDATE_INTERVAL, eSetValueWithOverwrite);
            DBG_PRINTF("[TYPE] Tipo SYNC ejecutado.\n");
        }
        else {
            DBG_PRINTF("[TYPE] Tipo no reconocido: %s\n", memory.operationType.c_str());
        }
    }
}

// *****************************************
//  FUNCION PARA REPOSICIONAR EL SISTEMA
// *****************************************
void positionTask(void* pvParameters) {
    bool activeDetected = false;

    for (;;) {
        // Espera notificación desde la ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        bool currentState = digitalRead(END_H);

        // Detectar flanco de subida (activación del final)
        if (currentState == HIGH && !activeDetected) {
            activeDetected = true;
            DBG_PRINTLN("Final de carrera horizontal ACTIVADO");
        }

        // Detectar flanco de bajada (ya pasó el límite)
        else if (currentState == LOW && activeDetected) {
            activeDetected = false;
            DBG_PRINTLN("Final de carrera horizontal DESACTIVADO -> pasó el límite");

            // Ajusta la dirección
            azimutDirection = memory.savePosition(!azimutDirection);

            DBG_PRINT("Dirección azimutal ajustada a: ");
            DBG_PRINTLN(azimutDirection ? "antihoraria" : "horaria");
        }
    }
}

// ************************
//  MODO HOMING
// ************************
void homingSequence() {
    xTaskNotify(ledTaskHandle, LED_HOMING, eSetValueWithOverwrite);
    isHoming = true;
    DBG_PRINTLN("Iniciando secuencia de homing...");
    
    // Mover motores hacia los finales de carrera
    while(digitalRead(START_V) == LOW) {
        motors.move(0, 2, 0, 80);       // Mover solo vertical hacia END_V
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    motors.stop();
    DBG_PRINTLN("Final de carrera vertical alcanzado");
    
    if (azimutDirection) {
        while(digitalRead(END_H) == LOW) {
            motors.move(2, 0, 80, 0);       // Mover solo horizontal hacia END_H en sentido horario
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else {
        while(digitalRead(END_H) == LOW) {
            motors.move(1, 0, 80, 0);       // Mover solo horizontal hacia END_H en sentido antihorario
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    motors.stop();
    DBG_PRINTLN("Final de carrera horizontal alcanzado");
    
    // Mover a posición inicial 
    if (azimutDirection) {
        motors.move(1, 0, 90, 0);
    } else {
        motors.move(2, 0, 90, 0);
    }          
    vTaskDelay(5000);                   // Tiempo suficiente para llegar horizontal
    motors.stop();
    motors.move(0, 1, 0, 90);           
    vTaskDelay(5000);                   // Tiempo suficiente para llegar vertical
    motors.stop();
    DBG_PRINTLN("Secuencia de homing completada");
    motors.mode = true;                 // Cambio a modo manual
    xTaskNotify(ledTaskHandle, LED_MANUAL, eSetValueWithOverwrite);
    DBG_PRINTLN("Modo cambiado a Manual después de homing");
    isHoming = false;
}

// ************************
//  NOTIFICACION DE MODO DE OPERACION
// ************************
void notifyMode(bool fromHardware) {
    motors.mode = !motors.mode;
    xTaskNotify(ledTaskHandle, motors.mode ? LED_MANUAL : LED_AUTOMATIC, eSetValueWithOverwrite);
    webInterface.broadcastMode(motors.mode);

    if (motors.mode) {
        xTaskNotify(controlTaskHandle, TASK_STOP_SIGNAL, eSetValueWithOverwrite);
        DBG_PRINTLN(fromHardware 
            ? "[BOTÓN] Pulsación corta (HW) → CAMBIO DE MODO A MANUAL" 
            : "[BOTÓN] Pulsación corta (SW) → CAMBIO DE MODO A MANUAL");
    } else {
        xTaskNotify(controlTaskHandle, TASK_START_SIGNAL, eSetValueWithOverwrite);
        DBG_PRINTLN(fromHardware 
            ? "[BOTÓN] Pulsación corta (HW) → CAMBIO DE MODO A AUTOMÁTICO" 
            : "[BOTÓN] Pulsación corta (SW) → CAMBIO DE MODO A AUTOMÁTICO");
    }
}

// *****************************************
//  GESTION DEL BOTON DE CAMBIO DE MODO
// *****************************************
void buttonTask(void *pvParameters) {
    const uint16_t longPressThreshold = 3000;     
    uint32_t notifyValue = 0;  

    for (;;) {
        // Espera evento (ya sea ISR o notificación desde otra tarea)
        xTaskNotifyWait(0x00, 0xFFFFFFFF, &notifyValue, portMAX_DELAY);

        // --- Pulsación (hardware físico) ---
        if (notifyValue == 1 && digitalRead(SET_MODE) == HIGH) {
            unsigned long pressStart = millis();

            // Esperar hasta que se suelte el boton
            while (digitalRead(SET_MODE) == HIGH) {
                if (millis() - pressStart >= longPressThreshold) {
                    DBG_PRINTLN("[BOTÓN] Pulsación larga → HOMING");
                    xTaskNotify(controlTaskHandle, TASK_STOP_SIGNAL, eSetValueWithOverwrite);
                    homingSequence();
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            // Si fue corta, cambiar modo
            if (millis() - pressStart < longPressThreshold) {
                notifyMode(true);
            }
        }
        // --- Pulsación corta (software) ---
        else if (notifyValue == 2){
            notifyMode(false);
        }
    }
}

// ************************
//  GESTION DEL PILOTO INDICADOR
// ************************
void ledTask(void *pvParameters){
    uint32_t notifiedValue = 0;
    TickType_t onTime = pdMS_TO_TICKS(1000);
    TickType_t offTime = pdMS_TO_TICKS(4000);

    for (;;) {
        // Espera notificación para actualizar el modo
        if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notifiedValue, 0) == pdTRUE) {
            switch (notifiedValue) {
                case LED_AUTOMATIC: offTime = pdMS_TO_TICKS(4000); break;
                case LED_MANUAL:    offTime = pdMS_TO_TICKS(2000); break;
                case LED_HOMING:    offTime = pdMS_TO_TICKS(100); break; // parpadeo rápido
                default:            offTime = 0; digitalWrite(LED_PIN, LOW); break;
            }
        }

        // Encender LED
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(onTime);

        // Apagar LED
        digitalWrite(LED_PIN, LOW);
        if (offTime > 0) vTaskDelay(offTime);
    }
}


// ************************
//  INTERRUPCION HMI
// ************************
void IRAM_ATTR webServerISR() {
    static uint32_t lastInterruptTime = 0;
    uint32_t currentTime = millis();

    // Enviar notificación a la tarea
    if (currentTime - lastInterruptTime > 500) {
        lastInterruptTime = currentTime;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(webControlHandle, 1, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        
    }
}

// ************************
//  INTERRUPCION DEL BOTON DE CAMBIO DE MODO 
// ************************
void IRAM_ATTR modeButtonISR() {
    static uint32_t lastInterruptTime = 0;
    uint32_t currentTime = millis();

    // Enviar notificación a la tarea
    if (currentTime - lastInterruptTime > 100) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(buttonTaskHandle, 1, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        lastInterruptTime = currentTime;
    }
}

// ************************
//  INTERRUPCIONES DE LOS FINALES DE CARRERA
// ************************
void IRAM_ATTR endCenitISR() {
    xTaskNotify(controlTaskHandle, TASK_STOP_SIGNAL, eSetValueWithOverwrite);
}

void IRAM_ATTR endAzimutISR() {
    static uint32_t lastInterruptTime = 0;
    uint32_t currentTime = millis();

    if (!isHoming && currentTime - lastInterruptTime > 1000){
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(positionTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        lastInterruptTime = currentTime;
    }
}

void setup() {
    DBG_BEGIN(115200);

    // Entradas
    pinMode(ENABLE_HMI, INPUT);
    pinMode(SET_MODE, INPUT);
    pinMode(END_V, INPUT);
    pinMode(END_H, INPUT);
    pinMode(START_V, INPUT);

    // Salidas
    pinMode(LED_PIN, OUTPUT);

    // Inicialización de sistema
    memory.begin();

    // Recuperar datos de memoria
    motors.offsetH_right = memory.loadOffsetMotor("h_right");
    motors.offsetH_left  = memory.loadOffsetMotor("h_left");
    motors.offsetV_up    = memory.loadOffsetMotor("v_up");
    motors.offsetV_down  = memory.loadOffsetMotor("v_down");
    sensors.offsetAzimut = memory.loadOffsetSensor("azimut");
    sensors.offsetCenit  = memory.loadOffsetSensor("cenit");
    azimutDirection      = memory.loadPosition();

    // Inicialización de módulos
    logger.begin(CS_SD);
    motors.begin();
    
    otaManager.setDevelopmentMode(false);

    // Interrupciones
    attachInterrupt(digitalPinToInterrupt(END_V),     endCenitISR,      CHANGE);
    attachInterrupt(digitalPinToInterrupt(END_H),     endAzimutISR,     CHANGE);
    attachInterrupt(digitalPinToInterrupt(SET_MODE),  modeButtonISR,    RISING);
    attachInterrupt(digitalPinToInterrupt(ENABLE_HMI),webServerISR,     CHANGE);

    // Crear tareas
    xTaskCreate(controlTask,   "Control",    4096, NULL,       4, &controlTaskHandle);
    xTaskCreate(webControlTask,"WebControl", 8192, NULL,       3, &webControlHandle);
    xTaskCreate(taskManager, "TaskManager",  4096, NULL,       3, &taskManagerHandle); 
    xTaskCreate(buttonTask,    "ButtonTask", 2048, NULL,       3, &buttonTaskHandle);
    xTaskCreate(positionTask,  "position",   4096, NULL,       2, &positionTaskHandle);
    xTaskCreate(loggingTask,   "Logging",    4096, NULL,       2, &loggingTaskHandle);
    xTaskCreate(ledTask,       "ledTask",    4096, NULL,       1, &ledTaskHandle);
    
    webInterface.setTaskHandle(taskManagerHandle, buttonTaskHandle, webControlHandle);

    // Homing
    homingSequence();
    
    // Estado al inicio
    xTaskNotifyGive(taskManagerHandle);
    xTaskNotify(webControlHandle, 1, eSetValueWithoutOverwrite);
}

void loop() {

    delay(2000);
}

