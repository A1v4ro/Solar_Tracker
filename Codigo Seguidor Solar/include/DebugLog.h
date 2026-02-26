#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#define MODE_DEBUG      1   // Debug por Serial USB
#define MODE_OFF        2   // No imprimir nada
#define MODE_TX         3   // Solo enviar datos para Serial Plotter
#define MODE_RX         4   // Solo recibir por Serial

#define CURRENT_MODE MODE_DEBUG

#if CURRENT_MODE == MODE_DEBUG
  #define ENABLE_SERIAL_RX false
  #define DBG_BEGIN(x) Serial.begin(x)
  #define DBG_PRINT(x) Serial.print(x)
  #define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
  #define DBG_PRINTLN(x) Serial.println(x)
  #define DBG_DATA(...) 
  #define DBG_WRITE(x)
  #define DBG_AVAILABLE() 0
  #define DBG_READ() -1
#elif CURRENT_MODE == MODE_TX
  #define ENABLE_SERIAL_RX false
  #define DBG_BEGIN(x) Serial.begin(x)
  #define DBG_PRINT(x)
  #define DBG_PRINTF(...)
  #define DBG_PRINTLN(x)
  #define DBG_DATA(...) Serial.printf(__VA_ARGS__)  // Enviar datos para plotter
  #define DBG_WRITE(x)
  #define DBG_AVAILABLE() 0
  #define DBG_READ() -1
#elif CURRENT_MODE == MODE_RX
  #define ENABLE_SERIAL_RX true
  #define DBG_BEGIN(x) Serial.begin(x)
  #define DBG_PRINT(x)
  #define DBG_PRINTF(...)
  #define DBG_PRINTLN(x)
  #define DBG_DATA(...)
  #define DBG_WRITE(x) Serial.write(x);                              
  #define DBG_AVAILABLE() Serial.available()        // Revisar si hay datos entrantes
  #define DBG_READ() Serial.read()                  // Leer datos entrantes
#else 
  #define ENABLE_SERIAL_RX false
  #define DBG_BEGIN(x) 
  #define DBG_PRINT(x)
  #define DBG_PRINTF(...)
  #define DBG_PRINTLN(x)
  #define DBG_DATA(...)
  #define DBG_WRITE(x)
  #define DBG_AVAILABLE() 0
  #define DBG_READ() -1
#endif
  
#endif 