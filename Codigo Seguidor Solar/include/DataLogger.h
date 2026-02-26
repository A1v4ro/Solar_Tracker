#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <time.h>
#include <RTClib.h>

class DataLogger {
public:
    DataLogger(const char* basePath = "/DATA");
    void begin(uint8_t csPin = 5);
    DateTime getCurrentTime();
    bool saveSensorData(DateTime timestamp, const std::vector<float>& sensorValues);
    bool setTimeFromWeb(const DateTime& now);
    std::vector<String> getSensorDates();
    std::vector<String> getSensorData(const String& date, int sensorIndex, bool includeTime = false);
    

private:
    String _basePath;
    RTC_DS3231 _rtc;
    bool _rtcInitialized;
    unsigned long _startMillis;  // Almacena el valor de millis() al inicio
    DateTime _initialTime;  // Almacena la hora inicial
};

#endif