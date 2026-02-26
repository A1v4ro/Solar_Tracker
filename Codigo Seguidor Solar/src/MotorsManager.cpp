#include "MotorsManager.h"

MotorsManager::MotorsManager(uint8_t pinH, uint8_t pwmH, uint8_t pinV, uint8_t pwmV)
    : _pinH(pinH), _pwmH(pwmH), _pinV(pinV), _pwmV(pwmV) {}

void MotorsManager::begin() {
    pinMode(_pinH, OUTPUT); 
    pinMode(_pinV, OUTPUT);
    ledcSetup(_channelH, _freq, _resolution); 
    ledcAttachPin(_pwmH, _channelH);
    ledcSetup(_channelV, _freq, _resolution); 
    ledcAttachPin(_pwmV, _channelV);
    stop();
}

void MotorsManager::move(uint8_t dirH, uint8_t dirV, uint8_t speedH, uint8_t speedV) {
    // dir = 1 FORWARD, dir = 2 BACKWARD, dir = 0 STOP
    _setMotor(_pinH, _channelH, dirH, speedH, dirH == 1 ? offsetH_right : offsetH_left); // Eje horizontal
    _setMotor(_pinV, _channelV, dirV, speedV, dirV == 1 ? offsetV_up : offsetV_down); // Eje vertical
}

void MotorsManager::_setMotor(uint8_t pin1, uint8_t channel, uint8_t dir, uint8_t speed, uint8_t offset) {
    digitalWrite(pin1, dir == 1 ? HIGH : LOW);
    if (speed != 0){
        ledcWrite(channel, map(speed, 1, 100, offset*2.55, 255));
    } else ledcWrite(channel, 0);
}

void MotorsManager::stop(){
    move(0, 0, 0, 0); 
}
