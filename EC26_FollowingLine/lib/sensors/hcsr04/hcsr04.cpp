#include "hcsr04.h"
#include "core/config.h"
#include <Arduino.h>

HcSr04 ultrasonic;

void HcSr04::begin() {
    pinMode(ULTRASONIC_TRIG, OUTPUT);
    pinMode(ULTRASONIC_ECHO, INPUT);
    digitalWrite(ULTRASONIC_TRIG, LOW);
}

float HcSr04::getDistance() {
    // Throttle: tối đa 20 lần/giây để không block loop
    unsigned long now = millis();
    if (now - _lastPing < 50) return _lastDist;
    _lastPing = now;

    digitalWrite(ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG, LOW);

    long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 6000); // timeout 6ms (~100cm max)
    _lastDist = (duration == 0) ? 999.0f : duration * 0.034f / 2.0f;
    return _lastDist;
}
