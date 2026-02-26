#ifndef MOTORS_MANAGER_H
#define MOTORS_MANAGER_H

#include <Arduino.h>

class MotorsManager {
public:
    MotorsManager(uint8_t pinH, uint8_t pwmH, uint8_t pinV, uint8_t pwmV);
    void begin();
    void move(uint8_t dirH, uint8_t dirV, uint8_t speedH, uint8_t speedV); // Movimiento independiente por eje
    void stop();
    bool mode = true;             // true: manual, false: automatico
    uint8_t offsetH_right, offsetH_left, offsetV_up, offsetV_down;

private:
    uint8_t _pinH, _pwmH;  // Eje horizontal
    uint8_t _pinV, _pwmV;  // Eje vertical
    const uint8_t _channelH = 0, _channelV = 1;
    const uint8_t _resolution = 8;
    const uint16_t _freq = 10000;

    void _setMotor(uint8_t pin1, uint8_t channel, uint8_t dir, uint8_t speed, uint8_t offset);
};

#endif