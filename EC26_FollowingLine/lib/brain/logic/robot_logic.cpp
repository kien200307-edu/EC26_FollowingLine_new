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


    _pid = new PID(PID_KP_DEFAULT, PID_KI_DEFAULT, PID_KD_DEFAULT);

    // Đưa servo về vị trí mở ngay khi khởi động
    servoControl.open();
    _isGrabbed = false;

    // Mặc định Auto Mode khi mới khởi động
    setControlMode(MODE_AUTOMATION);

    Serial.println("[Robot] Logic initialized. Default: AUTOMATION (stopped)");
}

// =====================================================
// SET AUTO MODE
// =====================================================

void RobotLogic::setControlMode(ControlMode mode) {
    if (currentControlMode == mode) return;
    currentControlMode = mode;

    Serial.println("=== CONTROL MODE CHANGED ===");
    motor.stop();
    _pid->reset();
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

void RobotLogic::setBaseSpeed(int speed) {
    _baseSpeed = constrain(speed, 0, 255);
    Serial.printf("[PID] BaseSpeed = %d\n", _baseSpeed);
}

void RobotLogic::setPidKp(float v) {
    v = constrain(v, PID_PARAM_MIN, PID_PARAM_MAX);
    _pid->setKp(v);
    Serial.printf("[PID] Kp = %.2f\n", v);
    bleManager.send("Kp=" + String(v, 2) + "\n");
}

void RobotLogic::setPidKi(float v) {
    v = constrain(v, PID_PARAM_MIN, PID_PARAM_MAX);
    _pid->setKi(v);
    Serial.printf("[PID] Ki = %.2f\n", v);
    bleManager.send("Ki=" + String(v, 2) + "\n");
}

void RobotLogic::setPidKd(float v) {
    v = constrain(v, PID_PARAM_MIN, PID_PARAM_MAX);
    _pid->setKd(v);
    Serial.printf("[PID] Kd = %.2f\n", v);
    bleManager.send("Kd=" + String(v, 2) + "\n");
}

void RobotLogic::applyParams(float p, float i, float d, int speed) {
    setPidKp(p);
    setPidKi(i);
    setPidKd(d);
    setBaseSpeed(speed);
}

// =====================================================
// UPDATE – Gọi mỗi loop()
// =====================================================

void RobotLogic::update() {

    updateControlModeButton();
    
    // Gửi dữ liệu cảm biến định kỳ
    sendSensorData();

    // Phát hiện mất kết nối BLE → bật lại Auto
    if (g_bleDisconnFlag) {
        g_bleDisconnFlag = false;
        _connected = false;
        // Chỉ chuyển mode nếu chưa ở Auto
        if (currentControlMode != MODE_AUTOMATION) {
            setControlMode(MODE_AUTOMATION);
        } else {
            Serial.println("[BLE] Disconnected, continuing in AUTO...");
        }
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

    String cfg = "Kp=" + String(_pid->getKp(), 2)
               + " Ki=" + String(_pid->getKi(), 2)
               + " Kd=" + String(_pid->getKd(), 2)
               + " Spd=" + String(_baseSpeed) + "\n";
    bleManager.send(cfg);

    sendRunState();
    bleManager.send(String("CAL:") + (qtr.isCalibrated() ? "READY\n" : "PENDING\n"));
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
        _pid->reset();
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

    if (!qtr.isCalibrated()) {
        bleManager.send("ERR:NOT_CALIBRATED\n");
        sendDebugLog("Cannot start: run CAL WHITE then CAL BLACK first.");
        return;
    }

    _fwd = false;
    _bwd = false;
    _left = false;
    _right = false;
    _pid->reset();
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
    _pid->reset();
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
    _pid->reset();
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
    _pid->reset();
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
    }
}

void RobotLogic::handleUARTTuning(const String& cmd) {
    if (cmd.length() < 2) return;
    
    // Xử lý định dạng mới: PID:kp,ki,kd,speed
    if (cmd.startsWith("PID:")) {
        String params = cmd.substring(4);
        int comma1 = params.indexOf(',');
        int comma2 = params.indexOf(',', comma1 + 1);
        int comma3 = params.indexOf(',', comma2 + 1);
        
        if (comma1 > 0 && comma2 > comma1 && comma3 > comma2) {
            float kp = params.substring(0, comma1).toFloat();
            float ki = params.substring(comma1 + 1, comma2).toFloat();
            float kd = params.substring(comma2 + 1, comma3).toFloat();
            int spd = params.substring(comma3 + 1).toInt();
            
            applyParams(kp, ki, kd, spd);
            sendDebugLog("PID Updated: Kp=" + String(kp, 2) + " Ki=" + String(ki, 2) + " Kd=" + String(kd, 2) + " Spd=" + String(spd));
        }
        return;
    }
    
    // Xử lý định dạng cũ: P/I/D/B + số
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

void RobotLogic::followLinePID() {
    if (!qtr.isCalibrated()) {
        motor.stop();
        return;
    }

    // 1. Kiểm tra vật cản trước tiên
    float dist = ultrasonic.getDistance();
    if (dist < 10 && dist > 0) {
        motor.stop();
        return; // Tạm dừng không đi tiếp
    }

    // 2. Đọc cảm biến
    qtr.readLine(); // Cập nhật _onLine và _sensorValues

    uint16_t s0 = qtr.getSensor(0);
    uint16_t s1 = qtr.getSensor(1);
    uint16_t s2 = qtr.getSensor(2);
    uint16_t s3 = qtr.getSensor(3);
    uint16_t s4 = qtr.getSensor(4);
    uint16_t s5 = qtr.getSensor(5);
    uint16_t s6 = qtr.getSensor(6);
    uint16_t s7 = qtr.getSensor(7);

    // 3. Nhận diện giao lộ (Junction Detection)
    JunctionType junction = _detector.update(qtr.getSensorValues());
    
    // In log nếu phát hiện giao lộ (chỉ in khi thay đổi hoặc có giao lộ)
    static JunctionType lastJunction = JunctionType::NONE;
    if (junction != lastJunction && junction != JunctionType::NONE) {
        Serial.printf("[Detector] Intersection Detected: %s | Pattern: 0x%02X\n", 
                      _detector.getTypeName(), _detector.getPattern());
        lastJunction = junction;
    } else if (junction == JunctionType::NONE) {
        lastJunction = JunctionType::NONE;
    }

    // 4. Phân tích các góc (Legacy logic - sẽ được thay thế bởi junction detector)
    bool allBlack = (junction == JunctionType::ALL_BLACK);
    bool left90  = (junction == JunctionType::LEFT_T || junction == JunctionType::SHARP_LEFT || junction == JunctionType::CROSSROAD);
    bool right90 = (junction == JunctionType::RIGHT_T || junction == JunctionType::SHARP_RIGHT || junction == JunctionType::CROSSROAD);

    // 5. Xử lý trạng thái
    // 5. Thực thi nhiệm vụ dựa trên Mission Stage
    switch (_mission.getStage()) {
        case MissionStage::TASK_1:
            // TODO: Thực hiện Bài 1 (Start -> CP1)
            // if (isAtCP1) _mission.nextStage();
            if (qtr.isOnLine()) normalPID();
            else rescueLogic(); 
            break;

        case MissionStage::TASK_2:
            // TODO: Thực hiện Bài 2 (CP1 -> CP2)
            // if (isAtCP2) _mission.nextStage();
            if (qtr.isOnLine()) normalPID();
            else rescueLogic();
            break;

        case MissionStage::TASK_3:
            // LOGIC HIỆN TẠI: Thực hiện Bài 3 (CP2 -> Finish)
            if (allBlack) {
                motor.stop();
                if (_isGrabbed) {
                    Serial.println("[Robot] >>> Diem dung (Task 3). Tha do...");
                    servoControl.open();
                    _isGrabbed = false;
                }
                _mission.nextStage(); // Chuyển sang kết thúc
                return;
            }
            
            if (!qtr.isOnLine()) {
                rescueLogic();
            } else if (left90 && right90) {
                normalPID();
            } else if (left90) {
                turnLeft90();
                _lastTurnDirection = -1;
            } else if (right90) {
                turnRight90();
                _lastTurnDirection = 1;
            } else {
                normalPID();
            }
            break;

        case MissionStage::FINISHING:
            motor.stop();
            Serial.println("[Mission] FINISHED.");
            _mission.nextStage();
            break;

        case MissionStage::COMPLETED:
            motor.stop();
            break;

        default:
            // IDLE hoặc trạng thái không xác định -> Follow line cơ bản
            if (qtr.isOnLine()) normalPID();
            else rescueLogic();
            break;
    }
}

void RobotLogic::normalPID() {
    float position = qtr.readLine();
    int error = (int)(position - QTR_LINE_CENTER);

    // Ghi nhớ hướng lệch để cứu hộ khi lọt vạch
    if (error < -QTR_POSITION_STEP) _lastTurnDirection = -1;
    else if (error > QTR_POSITION_STEP) _lastTurnDirection = 1;
    else _lastTurnDirection = 0;
    
    int adjustment = (int)_pid->compute((float)error);
    
    // ========== SPEED OUTPUT WITH SAFETY ==========
    // Ensure adjustment doesn't exceed baseSpeed
    adjustment = constrain(adjustment, -_baseSpeed, _baseSpeed);
    
    int leftSpeed = _baseSpeed + adjustment;
    int rightSpeed = _baseSpeed - adjustment;
    
    // Prevent motor reversal at high adjustment
    leftSpeed = constrain(leftSpeed, -255, 255);
    rightSpeed = constrain(rightSpeed, -255, 255);
    
    motor.setSpeed(leftSpeed, rightSpeed);
}

void RobotLogic::turnLeft90() {
    // Tốc độ rẽ dựa trên baseSpeed (70% of baseSpeed)
    int turnSpeed = (_baseSpeed * 70) / 100;
    turnSpeed = constrain(turnSpeed, 80, 200);  // Đảm bảo có đủ lực rẽ
    motor.setSpeed(-turnSpeed, turnSpeed);
}

void RobotLogic::turnRight90() {
    // Tốc độ rẽ dựa trên baseSpeed (70% of baseSpeed)
    int turnSpeed = (_baseSpeed * 70) / 100;
    turnSpeed = constrain(turnSpeed, 80, 200);  // Đảm bảo có đủ lực rẽ
    motor.setSpeed(turnSpeed, -turnSpeed);
}

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

    // --- BUTTON AUTO 3 (GPIO 15) ---
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
    // Mất vạch -> Cứu hộ dựa trên hướng rẽ cuối cùng
    int rescueTurnSpeed = 100;
    if (_lastTurnDirection == -1) {
        motor.setSpeed(-rescueTurnSpeed, rescueTurnSpeed); // Quay trái tìm vạch
    } else if (_lastTurnDirection == 1) {
        motor.setSpeed(rescueTurnSpeed, -rescueTurnSpeed); // Quay phải tìm vạch
    } else {
        // Ưu tiên rẽ trái khi mất line mà không rõ hướng (đặc biệt cho Auto 3)
        motor.setSpeed(-rescueTurnSpeed, rescueTurnSpeed); 
    }
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
    // Gửi dữ liệu cảm biến định kỳ (mỗi 100ms)
    if (!force && millis() - _lastSensorSendTime < SENSOR_SEND_INTERVAL) {
        return;
    }
    _lastSensorSendTime = millis();
    
    if (!bleManager.isConnected()) return;

    qtr.readLine();
    
    // Gửi dữ liệu QTR8A (8 cảm biến line)
    String sensorData = "SENSOR:";
    const uint16_t* sensors = qtr.getSensorValues();
    for (int i = 0; i < 8; i++) {
        sensorData += String(sensors[i]);
        if (i < 7) sensorData += ",";
    }
    sensorData += "\n";
    bleManager.send(sensorData);
    
    // Gửi dữ liệu siêu âm
    float dist = ultrasonic.getDistance();
    bleManager.send("DIST:" + String(dist, 1) + "\n");
    
    // Gửi dữ liệu hồng ngoại
    int ir = irSingle.isDetected() ? 1 : 0;
    bleManager.send("IR:" + String(ir) + "\n");
}

void RobotLogic::sendDebugLog(const String& msg) {
    if (bleManager.isConnected()) {
        bleManager.send("LOG:" + msg + "\n");
    }
    Serial.println("[LOG] " + msg);
}
