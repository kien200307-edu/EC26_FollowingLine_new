#include "robot_logic.h"

#include "core/config.h"

#include "actuators/motor/motor.h"
#include "actuators/servo/servo.h"

#include "sensors/qtr8a/qtr8a.h"
#include "sensors/hcsr04/hcsr04.h"
#include "sensors/ir_single/ir_single.h"

#include "comm/ble/ble_manager.h"

#include <Arduino.h>

// =====================================================
// Biến trạng thái BLE kết nối (extern từ ble_manager.cpp)
// =====================================================
extern volatile bool g_bleConnected;
extern volatile bool g_bleDisconnFlag;

// =====================================================
// Global instance
// =====================================================
RobotLogic robot;

namespace {
const char* missionCommandName(AutoMission mission) {
    switch (mission) {
        case AutoMission::AUTO_1: return "AUTO1";
        case AutoMission::AUTO_2: return "AUTO2";
        case AutoMission::AUTO_3: return "AUTO3";
        default: return "AUTO3";
    }
}
}

// =====================================================
// BEGIN
// =====================================================

void RobotLogic::begin() {
    pinMode(LED_MODE_PIN, OUTPUT);
    pinMode(BTN_MODE_PIN, INPUT_PULLUP);
    pinMode(BTN_AUTO3_PIN, INPUT_PULLUP);

    // Khởi tạo các giá trị PID & Tốc độ mặc định
    _kp = PID_KP_DEFAULT;
    _ki = PID_KI_DEFAULT;
    _kd = PID_KD_DEFAULT;
    _baseSpeed = PID_BASE_SPEED_DEFAULT;
    _turnSpeed = TURN_SPEED_DEFAULT;
    _sharpTurnSpeed = SHARP_TURN_SPEED_DEFAULT;

    // Đưa servo về vị trí mở ngay khi khởi động
    servoControl.open();
    _isGrabbed = false;

    // Mặc định Auto Mode khi mới khởi động
    setControlMode(MODE_AUTOMATION);

    Serial.println("[Robot] Logic initialized. Mode: AUTO_3. Default values loaded.");
}

// =====================================================
// SET AUTO MODE
// =====================================================

void RobotLogic::setControlMode(ControlMode mode) {
    if (currentControlMode == mode) return;
    currentControlMode = mode;

    Serial.println("=== CONTROL MODE CHANGED ===");
    motor.stop();
    
    // Reset PID
    _pVal = 0; _iVal = 0; _dVal = 0; _lastError = 0;
    
    _mission.reset();
    _robotRunning = false;
    _lastTurnDirection = 0;

    if (currentControlMode == MODE_AUTOMATION) {
        _fwd = false;
        _bwd = false;
        _left = false;
        _right = false;
        Serial.println("MODE: AUTOMATION");
        bleManager.send("MODE:AUTO\n");
    } else {
        Serial.println("MODE: MANUAL");
        bleManager.send("MODE:MANUAL\n");
    }

    sendRunState();
    updateLED();
}

// =====================================================
// LED INDICATOR
// =====================================================

void RobotLogic::updateLED() {
    digitalWrite(LED_MODE_PIN, (currentControlMode == MODE_AUTOMATION) ? HIGH : LOW);
}

// =====================================================
// SETTERS
// =====================================================

void RobotLogic::setBaseSpeed(int v) {
    _baseSpeed = constrain(v, 0, 255);
    Serial.printf("[PID] BaseSpeed = %d\n", _baseSpeed);
}

void RobotLogic::setPidKp(float v) {
    _kp = constrain(v, PID_PARAM_MIN, PID_PARAM_MAX);
    Serial.printf("[PID] Kp = %.2f\n", _kp);
}

void RobotLogic::setPidKi(float v) {
    _ki = constrain(v, PID_PARAM_MIN, PID_PARAM_MAX);
    Serial.printf("[PID] Ki = %.2f\n", _ki);
}

void RobotLogic::setPidKd(float v) {
    _kd = constrain(v, PID_PARAM_MIN, PID_PARAM_MAX);
    Serial.printf("[PID] Kd = %.2f\n", _kd);
}

void RobotLogic::setTurnSpeed(int v) {
    _turnSpeed = constrain(v, 0, 255);
    Serial.printf("[PID] TurnSpeed = %d\n", _turnSpeed);
}

void RobotLogic::setSharpTurnSpeed(int v) {
    _sharpTurnSpeed = constrain(v, 0, 255);
    Serial.printf("[PID] SharpTurnSpeed = %d\n", _sharpTurnSpeed);
}

// =====================================================
// UPDATE – Gọi mỗi loop()
// =====================================================

void RobotLogic::update() {

    updateControlModeButton();
    
    // Gửi dữ liệu cảm biến định kỳ

    // Phát hiện mất kết nối BLE -> Không can thiệp vào quá trình chạy
    if (g_bleDisconnFlag) {
        g_bleDisconnFlag = false;
        _connected = false;
        
        Serial.println("[BLE] Disconnected -> Robot continues running normally.");
    }

    // Cập nhật trạng thái kết nối
    bool nowConn = g_bleConnected;
    if (nowConn && !_connected) {
        _connected = true;
        sendStatusSnapshot();
    }

    // Tích hợp logic tự động gắp thả
    autoGrabLogic();

    // Thực thi chế độ
    if (currentControlMode == MODE_AUTOMATION) {
        if (_robotRunning) {
            followLinePID();
        } else {
            motor.stop();
        }
    } else {
        manualDrive();
    }

    if (_isCalibrating) {
        if (millis() - _calibStartTime < CALIB_DURATION_MS) {
            if (millis() - _lastCalibLedTime > CALIB_LED_INTERVAL) {
                _lastCalibLedTime = millis();
                _calibLedState = !_calibLedState;
                digitalWrite(LED_MODE_PIN, _calibLedState ? HIGH : LOW);
            }
            
            // Robot đứng yên để người dùng tự di chuyển tay qua line
            motor.stop(); 

            for (int i = 0; i < 8; i++) {
                uint16_t val = analogRead(qtrPins[i]);
                if (val < _sensorMin[i]) _sensorMin[i] = val;
                if (val > _sensorMax[i]) _sensorMax[i] = val;
            }
        } else {
            _isCalibrating = false;
            motor.stop();
            updateLED();
            bleManager.send("CAL:MOVE_DONE\n");
            sendDebugLog("Calibration movement finished.");
        }
    }

    sendSensorData();
}

// =====================================================
// UPDATE BLE – Xử lý lệnh đến từ app
// =====================================================

void RobotLogic::updateBLE(const String& cmd) {
    String incoming = cmd;
    incoming.trim();

    if (incoming.isEmpty()) return;

    _connected = bleManager.isConnected();

    // --------------------------------------------------
    // Gói tin Control Pad: !Bxy
    //   x = '1'..'8' (nút)
    //   y = '1' (nhấn) / '0' (nhả)
    // --------------------------------------------------
    if (incoming.startsWith("!B") && incoming.length() >= 4) {
        char btn     = incoming[2];
        bool pressed = (incoming[3] == '1');
        handleControlPad(btn, pressed);
        return;
    }

    // --------------------------------------------------
    // Gói tin Slider: !Sxvvvv
    //   x = '1'..'4'
    //   vvvv = float 4 bytes little-endian
    // --------------------------------------------------
    if (incoming.startsWith("!S") && incoming.length() >= 7) {
        char sliderNum = incoming[2];
        float value;
        memcpy(&value, incoming.c_str() + 3, 4);
        handleSlider(sliderNum, value);
        return;
    }

    if (incoming == "CAL_WHITE") {
        bleManager.send("ACK:CAL_WHITE\n");
        runWhiteCalibration();
        return;
    }
    if (incoming == "CAL_BLACK") {
        bleManager.send("ACK:CAL_BLACK\n");
        runBlackCalibration();
        return;
    }
    if (incoming == "CAL_MOVE") {
        bleManager.send("ACK:CAL_MOVE\n");
        runCalibration();
        return;
    }
    if (incoming == "START") {
        bleManager.send("ACK:START\n");
        startRobot();
        return;
    }
    if (incoming == "STOP") {
        bleManager.send("ACK:STOP\n");
        stopRobot();
        return;
    }
    if (incoming == "SYNC") {
        bleManager.send("ACK:SYNC\n");
        sendStatusSnapshot();
        return;
    }

    if (incoming == "AUTO" || incoming == "AUTO3") {
        selectAutoMission(AutoMission::AUTO_3);
        return;
    }
    if (incoming == "AUTO1") {
        selectAutoMission(AutoMission::AUTO_1);
        return;
    }
    if (incoming == "AUTO2") {
        selectAutoMission(AutoMission::AUTO_2);
        return;
    }
    if (incoming == "MANUAL") {
        setControlMode(MODE_MANUAL);
        return;
    }

    if (incoming.length() == 3 &&
        (incoming[0] == '0' || incoming[0] == '1') &&
        (incoming[1] == '0' || incoming[1] == '1') &&
        (incoming[2] == '0' || incoming[2] == '1')) {
        _surfaceBits = incoming;
        bleManager.send("BITS:" + _surfaceBits + "\n");
        sendDebugLog("Surface bits updated: " + _surfaceBits);
        return;
    }

    if (incoming.startsWith("M:")) {
        int comma = incoming.indexOf(',');
        if (comma > 2) {
            int leftSpd = incoming.substring(2, comma).toInt();
            int rightSpd = incoming.substring(comma + 1).toInt();
            setControlMode(MODE_MANUAL);
            motor.setSpeed(leftSpd, rightSpd);
        }
        return;
    }

    if (incoming.startsWith("M2BIT:")) {
        _mission2Bit = (incoming.substring(6).toInt() == 1);
        bleManager.send("M2BIT:" + String(_mission2Bit ? "1" : "0") + "\n");
        sendDebugLog("Mission 2 Bit updated: " + String(_mission2Bit ? "Black" : "White"));
        return;
    }

    // --------------------------------------------------
    // UART text (PID tuning): P/I/D + số
    // --------------------------------------------------
    handleUARTTuning(incoming);
}

void RobotLogic::sendRunState() {
    bleManager.send(String("STATE:") + (_robotRunning ? "RUNNING\n" : "STOPPED\n"));
}

void RobotLogic::sendStatusSnapshot() {
    bleManager.send((currentControlMode == MODE_AUTOMATION) ? "MODE:AUTO\n" : "MODE:MANUAL\n");

    String cfg = "Kp=" + String(_kp, 2)
               + " Ki=" + String(_ki, 2)
               + " Kd=" + String(_kd, 2)
               + " Spd=" + String(_baseSpeed) + "\n";
    bleManager.send(cfg);

    sendRunState();
    bleManager.send("MISSION:" + String(missionCommandName(_selectedMission)) + "\n");
    bleManager.send("BITS:" + _surfaceBits + "\n");
    bleManager.send("M2BIT:" + String(_mission2Bit ? "1" : "0") + "\n");
    sendSensorData(true);
}

void RobotLogic::selectAutoMission(AutoMission mission) {
    _selectedMission = mission;

    if (currentControlMode != MODE_AUTOMATION) {
        setControlMode(MODE_AUTOMATION);
    } else {
        motor.stop();
        _pVal = 0; _iVal = 0; _dVal = 0; _lastError = 0;
        _mission.reset();
        _robotRunning = false;
        _lastTurnDirection = 0;
        _fwd = false;
        _bwd = false;
        _left = false;
        _right = false;
        sendRunState();
    }

    bleManager.send("MISSION:" + String(missionCommandName(_selectedMission)) + "\n");
    bleManager.send("ACK:MISSION_SELECTED\n");
    sendDebugLog("Selected " + String(missionCommandName(_selectedMission)) + ". Press START when ready.");
    sendStatusSnapshot();
}

void RobotLogic::startRobot() {
    if (currentControlMode != MODE_AUTOMATION) {
        setControlMode(MODE_AUTOMATION);
    }

    _fwd = false;
    _bwd = false;
    _left = false;
    _right = false;
    _pVal = 0; _iVal = 0; _dVal = 0; _lastError = 0;
    _mission.startMission(_selectedMission);
    _robotRunning = true;
    _lastTurnDirection = 0;
    sendRunState();
    bleManager.send("ACK:STARTED\n");
    sendDebugLog("Robot started with " + String(missionCommandName(_selectedMission)) + ".");
    sendStatusSnapshot();
}

void RobotLogic::stopRobot() {
    motor.stop();
    _pVal = 0; _iVal = 0; _dVal = 0; _lastError = 0;
    _mission.reset();
    _robotRunning = false;
    _lastTurnDirection = 0;
    _fwd = false;
    _bwd = false;
    _left = false;
    _right = false;
    sendRunState();
    bleManager.send("ACK:STOPPED\n");
    sendDebugLog("Robot stopped.");
    sendStatusSnapshot();
}

void RobotLogic::runWhiteCalibration() {
    motor.stop();
    _robotRunning = false;
    _mission.reset();
    _pVal = 0; _iVal = 0; _dVal = 0; _lastError = 0;
    sendRunState();
    bleManager.send("CAL:WHITE_BEGIN\n");
    sendDebugLog("White calibration started. Place sensors on white surface.");

    if (qtr.calibrateWhite()) {
        bleManager.send("CAL:WHITE_DONE\n");
        bleManager.send("ACK:CAL_WHITE_DONE\n");
        sendDebugLog("White calibration complete. Place robot on black line and run CAL BLACK.");
        sendStatusSnapshot();
    }
}

void RobotLogic::runBlackCalibration() {
    motor.stop();
    _robotRunning = false;
    _mission.reset();
    _pVal = 0; _iVal = 0; _dVal = 0; _lastError = 0;
    sendRunState();
    bleManager.send("CAL:BLACK_BEGIN\n");
    sendDebugLog("Black calibration started. Place sensors on black line.");

    if (qtr.calibrateBlack()) {
        bleManager.send(String("CAL:") + (qtr.isCalibrated() ? "READY\n" : "BLACK_DONE\n"));
        bleManager.send(String("ACK:") + (qtr.isCalibrated() ? "CAL_READY\n" : "CAL_BLACK_DONE\n"));
        sendDebugLog(qtr.isCalibrated()
            ? "Black calibration complete. Sensors ready."
            : "Black calibration saved, but white calibration is still missing.");
        sendStatusSnapshot();
    }
}

// =====================================================
// HANDLE CONTROL PAD
// =====================================================

void RobotLogic::handleControlPad(char btn, bool pressed) {
    switch (btn) {

        // ── Mũi tên: Momentary drive ─────────────────
        case '5': // UP (Tiến)
            _fwd = pressed;
            // Nhấn nút hướng → tắt Auto Mode
            if (pressed) setControlMode(MODE_MANUAL);
            break;

        case '6': // DOWN (Lùi)
            _bwd = pressed;
            if (pressed) setControlMode(MODE_MANUAL);
            break;

        case '7': // LEFT
            _left = pressed;
            if (pressed) setControlMode(MODE_MANUAL);
            break;

        case '8': // RIGHT
            _right = pressed;
            if (pressed) setControlMode(MODE_MANUAL);
            break;

        // Thêm các phím gắp vật nếu cần (1-4)
        case '1': 
            if (pressed) { 
                setControlMode(MODE_MANUAL); 
                servoControl.close(); 
                _isGrabbed = true; 
            } 
            break;
        case '2': 
            if (pressed) { 
                setControlMode(MODE_MANUAL); 
                servoControl.open(); 
                _isGrabbed = false; 
            } 
            break;
    }
}

void RobotLogic::handleSlider(char num, float value) {
    switch (num) {
        case '1': setPidKp(value); break;
        case '2': setPidKi(value); break;
        case '3': setPidKd(value); break;
        case '4': setBaseSpeed((int)value); break;
        case '5': setTurnSpeed((int)value); break;
        case '6': setSharpTurnSpeed((int)value); break;
    }
}

void RobotLogic::handleUARTTuning(const String& cmd) {
    if (cmd.length() < 2) return;
    
    // Fallback format (nếu dùng terminal thủ công)
    char type = cmd[0];
    float val = cmd.substring(1).toFloat();
    if (type == 'P') setPidKp(val);
    else if (type == 'I') setPidKi(val);
    else if (type == 'D') setPidKd(val);
    else if (type == 'B') setBaseSpeed((int)val);
}

void RobotLogic::manualDrive() {
    int manualSpeed = MANUAL_SPEED_DEFAULT; // Tốc độ mặc định cho điều khiển tay (150)
    int left = 0, right = 0;
    if (_fwd) {
        left = manualSpeed; right = manualSpeed;
    } else if (_bwd) {
        left = -manualSpeed; right = -manualSpeed;
    } else if (_left) {
        left = -manualSpeed; right = manualSpeed;
    } else if (_right) {
        left = manualSpeed; right = -manualSpeed;
    }
    motor.setSpeed(left, right);
}

void RobotLogic::sampleLineTelemetry(long* weightedSum, long* sum) {
    long localWeightedSum = 0;
    long localSum = 0;

    for (int i = 0; i < 8; i++) {
        const uint16_t val = analogRead(qtrPins[i]);
        _sensorRaw[i] = val;
        _sensorDigital[i] = (val > QTR_ACTIVE_THRESHOLD) ? 1 : 0;
        localWeightedSum += (long)val * (i * 1000);
        localSum += val;
    }

    if (localSum > 400) {
        _position = (float)localWeightedSum / (float)localSum;
    }

    if (weightedSum) *weightedSum = localWeightedSum;
    if (sum) *sum = localSum;
}

void RobotLogic::followLinePID() {
    // 1. ĐỌC CẢM BIẾN
    long weightedSum = 0;
    long sum = 0;
    sampleLineTelemetry(&weightedSum, &sum);

    // 2. TÍNH ERROR
    float error;
    if (sum > 400) { 
        _position = (float)weightedSum / (float)sum;
        error = 3500.0f - _position; 
    } else {
        // Mất line hoàn toàn: Giữ lastError để tiếp tục cua theo quán tính
        error = _lastError;
    }

    // 3. PID THUẦN
    _pVal = error;
    _iVal += error;
    // Chống integral windup
    _iVal = constrain(_iVal, -10000.0f, 10000.0f);
    _dVal = error - _lastError;
    _lastError = error;

    float output = (_kp * _pVal) + (_ki * _iVal) + (_kd * _dVal);

    // 4. MOTOR CONTROL (Cho phép âm để xoay nhanh)
    // Lệch trái (pos < 3500) -> error > 0 -> Cần rẽ trái -> Bánh phải mạnh hơn
    int leftMotor  = _baseSpeed - (int)output;
    int rightMotor = _baseSpeed + (int)output;

    // Giới hạn motor trong khoảng [-255, 255]
    leftMotor = constrain(leftMotor, -255, 255);
    rightMotor = constrain(rightMotor, -255, 255);

    motor.setSpeed(leftMotor, rightMotor);

    // 5. TELEMETRY & DEBUG
    _currentTurnMode = (abs(error) < 500) ? TURN_STRAIGHT : 
                       (abs(error) < 2000) ? TURN_LIGHT : TURN_SHARP;

    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > 50) {
        lastDebugTime = millis();
        Serial.printf("POS:%0.1f ERR:%d OUT:%d L:%d R:%d\n", 
                      _position, (int)error, (int)output, leftMotor, rightMotor);
    }
}

void RobotLogic::runCalibration() {
    motor.stop();
    _robotRunning = false;
    _isCalibrating = true;
    _calibStartTime = millis();
    _lastCalibLedTime = 0;
    
    // Reset min/max
    for (int i = 0; i < 8; i++) {
        _sensorMin[i] = 4095;
        _sensorMax[i] = 0;
    }

    bleManager.send("CALIB:START\n");
    Serial.println("[CALIB] STARTING (5-8 seconds)...");
    sendDebugLog("Calibration started. Move robot across the line.");
}

// Các hàm PID/Turn90 cũ được bỏ trống vì logic đã gộp vào followLinePID()
void RobotLogic::normalPID() { }
void RobotLogic::turnLeft90() { }
void RobotLogic::turnRight90() { }

void RobotLogic::autoGrabLogic() {
    // Đã tắt tự động gắp bằng cảm biến siêu âm theo yêu cầu.
    // (Bấm nút trên điều khiển web để gắp/thả thủ công).
}

void RobotLogic::updateControlModeButton() {
    int reading = digitalRead(BTN_MODE_PIN);

    if (reading != _lastBtnState) {
        _lastBtnDebounceTime = millis();
    }

    if ((millis() - _lastBtnDebounceTime) > 150) {
        if (reading != _btnState) {
            _btnState = reading;

            if (_btnState == LOW) {
                startRobot();
            }
        }
    }

    _lastBtnState = reading;

    int reading3 = digitalRead(BTN_AUTO3_PIN);
    if (reading3 != _lastBtnAuto3State) {
        _lastBtnAuto3DebounceTime = millis();
    }
    if ((millis() - _lastBtnAuto3DebounceTime) > 150) {
        if (reading3 != _btnAuto3State) {
            _btnAuto3State = reading3;
            if (_btnAuto3State == LOW) {
                selectAutoMission(AutoMission::AUTO_3);
                startRobot();
                Serial.println("[Robot] Button 15 pressed: AUTO 3 selected and started");
            }
        }
    }
    _lastBtnAuto3State = reading3;
}

void RobotLogic::rescueLogic() {
    int forwardRescueSpeed = 80;
    int leftSpeed = forwardRescueSpeed;
    int rightSpeed = forwardRescueSpeed;

    if (_lastTurnDirection == -1) {
        leftSpeed = forwardRescueSpeed + 20; 
        rightSpeed = forwardRescueSpeed - 20; 
    } else if (_lastTurnDirection == 1) {
        leftSpeed = forwardRescueSpeed - 20; 
        rightSpeed = forwardRescueSpeed + 20; 
    }

    motor.setSpeed(leftSpeed, rightSpeed); 
}
// =====================================================
// SENSOR DATA GETTERS
// =====================================================

const uint16_t* RobotLogic::getSensorValues() const {
    return qtr.getSensorValues();
}

float RobotLogic::getDistance() const {
    return ultrasonic.getDistance();
}

bool RobotLogic::getIRDetection() const {
    return irSingle.isDetected();
}

// =====================================================
// TELEMETRY FUNCTIONS
// =====================================================

static unsigned long _lastSensorSendTime = 0;
static const unsigned long SENSOR_SEND_INTERVAL = 100; // Gửi 10 lần/giây

void RobotLogic::sendSensorData(bool force) {
    if (!force && millis() - _lastSensorSendTime < 200UL) {
        return;
    }
    _lastSensorSendTime = millis();
    
    if (!bleManager.isConnected()) return;

    if (currentControlMode != MODE_AUTOMATION || !_robotRunning) {
        sampleLineTelemetry();
    }

    for (int base = 0; base < 8; base += 2) {
        String aStr = "A" + String(base) + ":";
        aStr += String(_sensorRaw[base]);
        aStr += ",";
        aStr += String(_sensorRaw[base + 1]);
        aStr += "\n";
        bleManager.send(aStr);
    }

    String dStr = "D:";
    for (int i = 0; i < 8; i++) {
        dStr += String(_sensorDigital[i]);
    }
    dStr += "\n";
    bleManager.send(dStr);

    float error = 3500.0f - _position;
    float output = (currentControlMode == MODE_AUTOMATION && _robotRunning)
        ? (_kp * _pVal + _ki * _iVal + _kd * _dVal)
        : 0.0f;
    
    bleManager.send("POS:" + String(_position, 0) + "\n");
    bleManager.send("ERR:" + String(error, 0) + "\n");
    bleManager.send("OUT:" + String(output, 0) + "\n");
    
    const char* modeStr = (_currentTurnMode == TURN_SHARP) ? "SHARP" : 
                          (_currentTurnMode == TURN_MEDIUM) ? "MEDIUM" : 
                          (_currentTurnMode == TURN_LIGHT) ? "LIGHT" : "STRAIGHT";
    bleManager.send("TURN:" + String(modeStr) + "\n");
}

void RobotLogic::sendDebugLog(const String& msg) {
    if (bleManager.isConnected()) {
        bleManager.send("LOG:" + msg + "\n");
    }
    Serial.println("[LOG] " + msg);
}
