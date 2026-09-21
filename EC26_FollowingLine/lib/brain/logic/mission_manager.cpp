#include "mission_manager.h"

MissionManager::MissionManager() {
    reset();
}

void MissionManager::reset() {
    _currentMission = AutoMission::NONE;
    _currentStage = MissionStage::IDLE;
    _stageStartTime = 0;
}

void MissionManager::startMission(AutoMission mission) {
    _currentMission = mission;
    _stageStartTime = millis();

    switch (mission) {
        case AutoMission::AUTO_1:
            _currentStage = MissionStage::TASK_1;
            break;
        case AutoMission::AUTO_2:
            _currentStage = MissionStage::TASK_2;
            break;
        case AutoMission::AUTO_3:
            _currentStage = MissionStage::TASK_3;
            break;
        default:
            reset();
            return;
    }
    Serial.printf("[Mission] Started %s at Stage: %s\n", getMissionName(), getStageName());
}

void MissionManager::nextStage() {
    if (_currentMission == AutoMission::NONE) return;

    MissionStage oldStage = _currentStage;

    switch (_currentStage) {
        case MissionStage::TASK_1:
            _currentStage = MissionStage::TASK_2;
            break;
        case MissionStage::TASK_2:
            _currentStage = MissionStage::TASK_3;
            break;
        case MissionStage::TASK_3:
            _currentStage = MissionStage::FINISHING;
            break;
        case MissionStage::FINISHING:
            _currentStage = MissionStage::COMPLETED;
            break;
        default:
            // Đã xong hoặc IDLE
            return;
    }

    _stageStartTime = millis();
    Serial.printf("[Mission] Stage Changed: %s -> %s\n", 
                  oldStage == MissionStage::IDLE ? "IDLE" : 
                  (oldStage == MissionStage::TASK_1 ? "TASK_1" : 
                  (oldStage == MissionStage::TASK_2 ? "TASK_2" : "TASK_3")),
                  getStageName());
}

void MissionManager::update() {
    // Placeholder cho các logic giám sát toàn cục nếu cần
    if (_currentMission == AutoMission::NONE) return;
}

const char* MissionManager::getMissionName() const {
    switch (_currentMission) {
        case AutoMission::AUTO_1: return "AUTO_1 (Full)";
        case AutoMission::AUTO_2: return "AUTO_2 (CP1 -> End)";
        case AutoMission::AUTO_3: return "AUTO_3 (CP2 -> End)";
        default: return "NONE";
    }
}

const char* MissionManager::getStageName() const {
    switch (_currentStage) {
        case MissionStage::TASK_1:    return "TASK_1";
        case MissionStage::TASK_2:    return "TASK_2";
        case MissionStage::TASK_3:    return "TASK_3";
        case MissionStage::FINISHING: return "FINISHING";
        case MissionStage::COMPLETED: return "COMPLETED";
        default:                      return "IDLE";
    }
}
