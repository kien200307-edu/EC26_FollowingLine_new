#include "servo.h"
#include "core/config.h"
#include <Arduino.h>

#define CHAN_R 2
#define CHAN_L 3
#define S_FREQ 50
#define S_RES 10

// ==========================================
// CẤU HÌNH GÓC SERVO (Dễ dàng tinh chỉnh tại đây)
// ==========================================
const int ANGLE_OPEN_LEFT  = 90;
const int ANGLE_OPEN_RIGHT = 0;

const int ANGLE_CLOSE_LEFT  = 45; // Trái: 90 -> 45 (giảm 45 độ)
const int ANGLE_CLOSE_RIGHT = 45; // Phải: 0 -> 45 (tăng 45 độ)

ServoControl servoControl;

static void moveServo(int channel, int angle) {
    int duty = map(angle, 0, 180, 20, 130);
    ledcWrite(channel, duty);
}

void ServoControl::begin() {
    ledcSetup(CHAN_L, S_FREQ, S_RES);
    ledcAttachPin(SERVO_LEFT_PIN, CHAN_L);

    ledcSetup(CHAN_R, S_FREQ, S_RES);
    ledcAttachPin(SERVO_RIGHT_PIN, CHAN_R);

    open();
}

void ServoControl::setLeft(int angle) {
    angle = constrain(angle, 0, 180);
    leftAngle = angle;
    moveServo(CHAN_L, angle);
}

void ServoControl::setRight(int angle) {
    angle = constrain(angle, 0, 180);
    rightAngle = angle;
    moveServo(CHAN_R, angle);
}

void ServoControl::open() {
    int startL = leftAngle;
    int startR = rightAngle;
    int targetL = ANGLE_OPEN_LEFT;
    int targetR = ANGLE_OPEN_RIGHT;

    int steps = 100;
    int stepDelay = 700 / steps; // Tổng thời gian ~0.7 giây

    for (int i = 1; i <= steps; i++) {
        int curL = startL + (targetL - startL) * i / steps;
        int curR = startR + (targetR - startR) * i / steps;
        moveServo(CHAN_L, curL);
        moveServo(CHAN_R, curR);
        delay(stepDelay);
    }

    leftAngle = targetL;
    rightAngle = targetR;
}

void ServoControl::close() {
    int startL = leftAngle;
    int startR = rightAngle;
    int targetL = ANGLE_CLOSE_LEFT;
    int targetR = ANGLE_CLOSE_RIGHT;

    int steps = 100;
    int stepDelay = 700 / steps; // Tổng thời gian ~0.7 giây

    for (int i = 1; i <= steps; i++) {
        int curL = startL + (targetL - startL) * i / steps;
        int curR = startR + (targetR - startR) * i / steps;
        moveServo(CHAN_L, curL);
        moveServo(CHAN_R, curR);
        delay(stepDelay);
    }

    leftAngle = targetL;
    rightAngle = targetR;
}
