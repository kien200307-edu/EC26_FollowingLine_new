#include "ir_single.h"
#include "core/config.h"
#include <Arduino.h>

IrSingle irSingle;

void IrSingle::begin() {
    pinMode(IR_SINGLE_PIN, INPUT);
}

bool IrSingle::isDetected() {
    return digitalRead(IR_SINGLE_PIN) == LOW; // Active LOW thông thường
}
