#include <Arduino.h>
#include "core/system.h"

void setup() {
    systemInit();
}

void loop() {
    systemUpdate();
}
