#pragma once

#include <Arduino.h>

/**
 * @brief Định nghĩa các loại nhiệm vụ Auto
 */
enum class AutoMission {
    NONE,
    AUTO_1, // Bài 1 -> Bài 2 -> Bài 3
    AUTO_2, // CP1 -> Bài 2 -> Bài 3
    AUTO_3  // CP2 -> Bài 3 (Hiện tại)
};

/**
 * @brief Các giai đoạn của một nhiệm vụ
 */
enum class MissionStage {
    IDLE,
    TASK_1,    // Bài 1
    TASK_2,    // Bài 2
    TASK_3,    // Bài 3 (Giai đoạn gắp vật / về đích hiện tại)
    FINISHING, // Về đích
    COMPLETED  // Hoàn thành
};

/**
 * @brief Lớp quản lý tiến trình thực hiện nhiệm vụ (Mission Manager)
 */
class MissionManager {
public:
    MissionManager();

    /**
     * @brief Bắt đầu một nhiệm vụ Auto cụ thể
     */
    void startMission(AutoMission mission);

    /**
     * @brief Cập nhật trạng thái nhiệm vụ (gọi trong loop)
     */
    void update();

    /**
     * @brief Chuyển sang giai đoạn tiếp theo
     */
    void nextStage();

    /**
     * @brief Reset toàn bộ trạng thái
     */
    void reset();

    // Getters
    AutoMission getMission() const { return _currentMission; }
    MissionStage getStage() const { return _currentStage; }
    bool isRunning() const { return _currentMission != AutoMission::NONE && _currentStage != MissionStage::COMPLETED; }
    const char* getMissionName() const;
    const char* getStageName() const;

private:
    AutoMission _currentMission = AutoMission::NONE;
    MissionStage _currentStage = MissionStage::IDLE;

    unsigned long _stageStartTime = 0;
};
