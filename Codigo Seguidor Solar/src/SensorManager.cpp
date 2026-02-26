#include "SensorManager.h"

SensorManager::SensorManager(uint8_t upPin, uint8_t downPin, uint8_t rightPin, uint8_t leftPin){
    _pins[0] = upPin;    // Sup
    _pins[1] = downPin;  // Inf
    _pins[2] = rightPin; // Der
    _pins[3] = leftPin;  // Izq
}

std::vector<float> SensorManager::readAllSensors() {
    updateSolarSensor();
    std::vector<float> readings;
    readings.push_back(getEcenit());
    readings.push_back(getEazimut());
    if (_readSerial){
        DBG_WRITE('R');                             
        _checkSerialData();
        readings.insert(readings.end(), _serialData.begin(), _serialData.end());
    }
    
    return readings;
}

std::vector<String> SensorManager::getSensorNames() {
    return {
        "Corriente 0",
        "Voltaje 0",
        "Potencia 0",
        "Corriente 1",
        "Voltaje 1",
        "Potencia 1",
        "Cenit",
        "Azimut"
    };
}

// Método para actualizar las mediciones de los sensores
void SensorManager::updateSolarSensor() {

    for (int i = 0; i < 4; i++) {
        analogReadRaw(_pins[i]);
    }

    for (int i = 0; i < 4; i++) {
        _accumulator[i] = 0;
    }

    // Realizar el promedio de lecturas
    for (int i = 0; i < _samples; i++) {
        for (int j = 0; j < 4; j++) {
            _accumulator[j] += analogReadRaw(_pins[j]);
        }
    }

    // Calcular los errores
    _newEcenit = (_accumulator[0] - _accumulator[1]) / (_samples * 2.0f);
    _newEazimut = (_accumulator[2] - _accumulator[3]) / (_samples * 2.0f);

    // Filtro exponencial 
    _Ecenit = _alpha * _newEcenit + (1.0f - _alpha) * _Ecenit;
    _Eazimut = _alpha * _newEazimut + (1.0f - _alpha) * _Eazimut;
}

// Método para obtener el error Cenit
int SensorManager::getEcenit() {
    return _Ecenit;
}

// Método para obtener el error Azimut
int SensorManager::getEazimut() {
    return _Eazimut;
}

// Método para leer por serial
void SensorManager::_parseData(const String& line) {
    _serialData.clear();  // Limpia los datos previos

    String trimmed = line;
    trimmed.trim();

    int start = 0;
    while (true) {
        int commaIndex = trimmed.indexOf(',', start);
        String token;
        if (commaIndex == -1) {
            token = trimmed.substring(start);  // último valor
        } else {
            token = trimmed.substring(start, commaIndex);
        }

        token.trim();
        if (token.length() > 0) {
            _serialData.push_back(token.toFloat() / 100.0f); // convierte a float y escala
        }

        if (commaIndex == -1) break;
        start = commaIndex + 1;
    }
}

// Método para parsear datos recibidos
void SensorManager::_checkSerialData() {
    static String receivedLine = "";
    vTaskDelay(pdMS_TO_TICKS(20));  // esperamos que el buffer se llene

    while (DBG_AVAILABLE()) {
        char c = DBG_READ();
        if (c == '\n') {
            _parseData(receivedLine);  
            receivedLine = "";
        } else {
            receivedLine += c;
        }
    }
}
