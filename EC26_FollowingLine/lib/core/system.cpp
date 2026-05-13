#include "system.h"
#include "config.h"

#include <Arduino.h>

#include "actuators/motor/motor.h"
#include "actuators/servo/servo.h"

#include "sensors/qtr8a/qtr8a.h"
#include "sensors/hcsr04/hcsr04.h"
#include "sensors/ir_single/ir_single.h"

#include "comm/ble/ble_manager.h"

#include "brain/logic/robot_logic.h"

// =====================================================
// SYSTEM INIT
// =====================================================

void systemInit() {

    Serial.begin(115200);
    delay(100);
    Serial.println("\n==============================");
    Serial.println("  ESP32 Robot Initializing...");
    Serial.println("==============================");

    // Actuators
    motor.begin();
    servoControl.begin();

    // Sensors
    qtr.begin();
    ultrasonic.begin();
    irSingle.begin();

    // Comm
    bleManager.begin();

    // Logic
    robot.begin();

    Serial.println("[System] All systems GO!");
}

void systemUpdate() {
    // Xử lý lệnh BLE nếu có
    if (bleManager.available()) {
        String cmd = bleManager.readCommand();
        robot.updateBLE(cmd);
    }

    // Cập nhật logic điều khiển robot
    robot.update();
    
    // Cập nhật trạng thái servo (non-blocking)
    servoControl.update();

    // Cho phép các tác vụ nền (BLE, Wi-Fi) chạy
    yield();
}