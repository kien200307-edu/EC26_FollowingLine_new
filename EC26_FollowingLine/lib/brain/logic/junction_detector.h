#pragma once

#include <Arduino.h>
#include "core/config.h"

/**
 * @brief Enum định nghĩa các loại giao lộ robot có thể gặp
 */
enum class JunctionType {
    NONE,           // Không có giao lộ (đang follow line bình thường)
    LEFT_T,         // Ngã 3 rẽ trái (như chữ T quay sang trái)
    RIGHT_T,        // Ngã 3 rẽ phải
    T_JUNCTION,     // Ngã 3 chữ T (đường cụt phía trước, có thể rẽ trái và phải)
    CROSSROAD,      // Ngã 4 (thập tự)
    ALL_BLACK,      // Vùng đen toàn bộ (vạch dừng hoặc vạch đặc biệt)
    SHARP_LEFT,     // Góc vuông trái
    SHARP_RIGHT,    // Góc vuông phải
    FAKE_INTERSECTION // Vùng nhiễu hình tròn/blob (cần bỏ qua)
};

/**
 * @brief Lớp xử lý nhận diện giao lộ dựa trên pattern của sensor
 */
class JunctionDetector {
public:
    JunctionDetector();

    /**
     * @brief Cập nhật dữ liệu từ cảm biến và thực hiện nhận diện
     * @param rawSensorValues Mảng giá trị analog từ 8 sensor
     * @return Loại giao lộ vừa nhận diện được
     */
    JunctionType update(const uint16_t sensorValues[8]);

    /**
     * @brief Trả về loại giao lộ hiện tại
     */
    JunctionType getType() const { return _currentType; }

    /**
     * @brief Trả về bit pattern hiện tại (1 bit mỗi sensor)
     */
    uint8_t getPattern() const { return _currentPattern; }

    /**
     * @brief Reset trạng thái detector (khi vừa thực hiện xong một cú rẽ)
     */
    void reset();

    // Helper kiểm tra nhanh
    bool isIntersection() const { 
        return _currentType != JunctionType::NONE && _currentType != JunctionType::FAKE_INTERSECTION; 
    }
    const char* getTypeName() const;

private:
    /**
     * @brief Chuyển đổi giá trị analog sang bit pattern (0/1)
     */
    uint8_t calculatePattern(const uint16_t sensorValues[8]);

    /**
     * @brief Phân tích pattern hiện tại để tìm các đặc trưng vùng
     */
    void analyzePattern(uint8_t pattern);

    /**
     * @brief Logic chính để phân loại giao lộ dựa trên đặc trưng và lịch sử
     */
    JunctionType classify();

    /**
     * @brief Kiểm tra tính đối xứng của pattern (để nhận diện hình tròn/blob)
     */
    bool checkSymmetry(uint8_t pattern);

    /**
     * @brief Cập nhật và phân tích xu hướng tăng/giảm số lượng mắt đọc (Trend Analysis)
     */
    void updateTrend(uint8_t activeCount);

    // Cấu hình threshold
    static const uint16_t SENSOR_THRESHOLD = QTR_ACTIVE_THRESHOLD;
    static const uint8_t HISTORY_SIZE = 10;      // Tăng history để theo dõi sequence dài hơn
    static const uint8_t CONFIDENCE_THRESHOLD = 3; 

    JunctionType _currentType = JunctionType::NONE;
    uint8_t _currentPattern = 0;
    
    // History buffer
    uint8_t _patternHistory[HISTORY_SIZE];
    uint8_t _countHistory[HISTORY_SIZE];
    uint8_t _historyIndex = 0;

    // Feature flags cho pattern hiện tại
    bool _hasLeft = false;      
    bool _hasRight = false;     
    bool _hasCenter = false;    
    uint8_t _activeCount = 0;   
    
    // Trend analysis state
    int8_t _expansionTrend = 0; // > 0: đang mở rộng, < 0: đang thu hẹp
    bool _reachedPeak = false;
    uint8_t _maxActiveInSequence = 0;
    
    // Temporal flags
    unsigned long _lastDetectionTime = 0;
    JunctionType _pendingType = JunctionType::NONE;
    uint8_t _confidence = 0;

    // Persistence & Stability Logic
    uint8_t _leftPersistence = 0;
    uint8_t _rightPersistence = 0;
    uint8_t _centerPersistence = 0;
    
    static const uint8_t STABLE_THRESHOLD = 4; // Số frame liên tiếp cần để coi là nhánh ổn định
};
