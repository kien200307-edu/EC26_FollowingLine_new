#include "motor.h"
#include "core/config.h"
#include <Arduino.h>

Motor motor;

void Motor::begin() {

    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_IN3, OUTPUT);
    pinMode(MOTOR_IN4, OUTPUT);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcAttach(MOTOR_ENA, PWM_FREQ, PWM_RESOLUTION);
    ledcAttach(MOTOR_ENB, PWM_FREQ, PWM_RESOLUTION);
#else
    ledcSetup(PWM_CHANNEL_LEFT,  PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(MOTOR_ENA, PWM_CHANNEL_LEFT);
    ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(MOTOR_ENB, PWM_CHANNEL_RIGHT);
#endif

    stop();
}

void Motor::setSpeed(int left, int right) {

    left  = constrain(left,  -255, 255);
    right = constrain(right, -255, 255);

    // LEFT direction
    if (left >= 0) {
        digitalWrite(MOTOR_IN1, HIGH);
        digitalWrite(MOTOR_IN2, LOW);
    } else {
        digitalWrite(MOTOR_IN1, LOW);
        digitalWrite(MOTOR_IN2, HIGH);
    }

    // RIGHT direction
    if (right >= 0) {
        digitalWrite(MOTOR_IN3, HIGH);
        digitalWrite(MOTOR_IN4, LOW);
    } else {
        digitalWrite(MOTOR_IN3, LOW);
        digitalWrite(MOTOR_IN4, HIGH);
    }

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(MOTOR_ENA, abs(left));
    ledcWrite(MOTOR_ENB, abs(right));
#else
    ledcWrite(PWM_CHANNEL_LEFT,  abs(left));
    ledcWrite(PWM_CHANNEL_RIGHT, abs(right));
#endif
}

void Motor::stop() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(MOTOR_ENA, 0);
    ledcWrite(MOTOR_ENB, 0);
#else
    ledcWrite(PWM_CHANNEL_LEFT,  0);
    ledcWrite(PWM_CHANNEL_RIGHT, 0);
#endif

    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_IN3, LOW);
    digitalWrite(MOTOR_IN4, LOW);
}
