#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <String.h>
#include "DebugLog.h"

class SensorManager {
public:
    SensorManager(uint8_t upPin, uint8_t downPin, uint8_t rightPin, uint8_t leftPin);
    void updateSolarSensor();
    int getEcenit();
    int getEazimut();
    std::vector<float> readAllSensors();
    std::vector<String> getSensorNames();
    int16_t offsetAzimut, offsetCenit;

private:
    uint8_t _pins[4];
    float _alpha = 0.1f;
    const int _samples = 30;                            // Número de lecturas para promediar
    float _Ecenit, _Eazimut, _newEcenit, _newEazimut;   // Errores calculados
    int32_t _accumulator[4];                        
    bool _readSerial = ENABLE_SERIAL_RX;                // existencia de datos por serial
    std::vector<float> _serialData;                     // datos de serial procesados (de enteros a decimales)
    void _parseData(const String& line);     
    void _checkSerialData();           
};

#endif