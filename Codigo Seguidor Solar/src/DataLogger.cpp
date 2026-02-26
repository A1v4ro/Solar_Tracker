#include "DataLogger.h"
#include "DebugLog.h"

DataLogger::DataLogger(const char* basePath) : _basePath(basePath) {}

void DataLogger::begin(uint8_t csPin) {
    Wire.begin(21, 22);
    
    if (!_rtc.begin()) {
        _rtcInitialized = false;
        DBG_PRINTLN("Error al iniciar RTC");
    } else _rtcInitialized = true;
    
    // Si el RTC perdió energía, establecer la fecha y hora
    if (_rtc.lostPower()) {
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Guardar la hora inicial y el valor de millis()
    _initialTime = _rtc.now();
    _startMillis = millis();

    if (!SD.begin(csPin)) {
        DBG_PRINTLN("Error al iniciar SD");
    }
    if (!SD.exists(_basePath.c_str())) { 
        SD.mkdir(_basePath.c_str());
    }
}

bool DataLogger::setTimeFromWeb(const DateTime& now) {
    _initialTime = now;
    _startMillis = millis();

    // Actualizar RTC
    if (_rtcInitialized) {
        _rtc.adjust(now);
        // Debug: Imprime la nueva hora configurada
        DBG_PRINTF("[RTC] Hora actualizado correctamente desde Web: %04d-%02d-%02d %02d:%02d:%02d\n",
                    now.year(), now.month(), now.day(),
                    now.hour(), now.minute(), now.second());
        return true;
    } else {
        DBG_PRINTLN("[RTC] Error al actualizar RTC");
        return false;
    }
}


DateTime DataLogger::getCurrentTime() {
    // Calcular el tiempo transcurrido en segundos
    unsigned long seconds = (millis() - _startMillis) / 1000;
    // Sumar al tiempo inicial
    return _initialTime + TimeSpan(seconds);
}

bool DataLogger::saveSensorData(DateTime  timestamp, const std::vector<float>& sensorValues) {
    char dateBuf[11], timeBuf[9];
    sprintf(dateBuf, "%04d%02d%02d", timestamp.year(), timestamp.month(), timestamp.day());
    sprintf(timeBuf, "%02d:%02d:%02d", timestamp.hour(), timestamp.minute(), timestamp.second());
    
    String csvPath = _basePath + "/" + String(dateBuf) + ".csv";
    File file = SD.open(csvPath.c_str(), FILE_APPEND);
    if (!file) return false;

    String line = String(timeBuf);
    for (float value : sensorValues) {
        line += "," + String(value, 2);
    }
    file.println(line);
    file.close();
    return true;
}

std::vector<String> DataLogger::getSensorDates() {
    std::vector<String> dates;
    File dir = SD.open(_basePath.c_str());
    
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        
        if (!entry.isDirectory() && String(entry.name()).endsWith(".csv")) {
            String filename = entry.name();
            dates.push_back(filename.substring(0, filename.lastIndexOf('.')));
        }
        entry.close();
    }
    return dates;
}

std::vector<String> DataLogger::getSensorData(const String& date, int sensorIndex, bool includeTime) {
    std::vector<String> data;
    String csvPath = _basePath + "/" + date + ".csv";
    
    File file = SD.open(csvPath.c_str(), FILE_READ);
    if (!file) return data;
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() == 0) continue;
        
        int commaCount = 0;
        int lastComma = 0;
        int sensorStart = 0;
        int sensorEnd = 0;
        
        // Contamos las comas para encontrar el sensor correcto
        for (int i = 0; i < line.length(); i++) {
            if (line.charAt(i) == ',') {
                commaCount++;
                if (commaCount == sensorIndex) {
                    sensorStart = i + 1;
                }
                if (commaCount == sensorIndex + 1) {
                    sensorEnd = i;
                    break;
                }
                lastComma = i;
            }
        }
        
        if (sensorEnd == 0) sensorEnd = line.length();
        
        String value;
        if (includeTime) {
            // Incluimos el tiempo y el valor
            int timeEnd = line.indexOf(',');
            String time = line.substring(0, timeEnd);
            value = time + "," + line.substring(sensorStart, sensorEnd);
        } else {
            // Solo el valor
            value = line.substring(sensorStart, sensorEnd);
        }
        
        data.push_back(value);
    }
    
    file.close();
    return data;
}