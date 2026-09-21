#include "pid.h"
#include <Arduino.h>

PID::PID(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
    integral = 0.0f;
    previousError = 0.0f;
}

void PID::setTunings(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void PID::setKp(float value) { _kp = value; }
void PID::setKi(float value) { _ki = value; }
void PID::setKd(float value) { _kd = value; }

void PID::reset() {
    integral = 0.0f;
    previousError = 0.0f;
}

float PID::compute(float error) {
    float proportional = error;

    integral += error;
    integral = constrain(integral, -1000.0f, 1000.0f);

    float derivative = error - previousError;
    previousError = error;

    float output = (_kp * proportional) + (_ki * integral) + (_kd * derivative);
    return constrain(output, -255.0f, 255.0f);
}

void PID::leakIntegral(float factor) {
    factor = constrain(factor, 0.0f, 1.0f);
    integral *= factor;
}
