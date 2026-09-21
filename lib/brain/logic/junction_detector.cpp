#include "junction_detector.h"

JunctionDetector::JunctionDetector() {
    reset();
}

void JunctionDetector::reset() {
    _currentType = JunctionType::NONE;
    _pendingType = JunctionType::NONE;
    _confidence = 0;
    _currentPattern = 0;
    _expansionTrend = 0;
    _reachedPeak = false;
    _maxActiveInSequence = 0;
    _leftPersistence = 0;
    _rightPersistence = 0;
    _centerPersistence = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) {
        _patternHistory[i] = 0;
        _countHistory[i] = 0;
    }
}

JunctionType JunctionDetector::update(const uint16_t sensorValues[8]) {
    // 1. Chuyển đổi sang bit pattern
    _currentPattern = calculatePattern(sensorValues);
    
    // 2. Lưu vào history
    _patternHistory[_historyIndex] = _currentPattern;
    _countHistory[_historyIndex] = _activeCount;
    _historyIndex = (_historyIndex + 1) % HISTORY_SIZE;

    // 3. Cập nhật xu hướng (Trend) và Đối xứng (Symmetry)
    updateTrend(_activeCount);

    // 4. Phân tích đặc trưng vùng
    analyzePattern(_currentPattern);

    // 5. Cập nhật Persistence (Độ ổn định vùng)
    if (_hasLeft) _leftPersistence++; else _leftPersistence = 0;
    if (_hasRight) _rightPersistence++; else _rightPersistence = 0;
    if (_hasCenter) _centerPersistence++; else _centerPersistence = 0;

    // Giới hạn persistence để tránh tràn
    if (_leftPersistence > 20) _leftPersistence = 20;
    if (_rightPersistence > 20) _rightPersistence = 20;
    if (_centerPersistence > 20) _centerPersistence = 20;

    // 6. Phân loại
    JunctionType detected = classify();

    // 5. Confidence Logic (Anti-noise)
    if (detected == JunctionType::NONE) {
        _confidence = 0;
        _pendingType = JunctionType::NONE;
        // Nếu đang trong trạng thái NONE thì giữ nguyên NONE
        _currentType = JunctionType::NONE;
    } 
    else if (detected == _pendingType) {
        _confidence++;
        if (_confidence >= CONFIDENCE_THRESHOLD) {
            _currentType = detected; // Xác nhận giao lộ
        }
    } 
    else {
        // Thay đổi loại giao lộ đang track -> reset confidence
        _pendingType = detected;
        _confidence = 1;
    }

    return _currentType;
}

uint8_t JunctionDetector::calculatePattern(const uint16_t sensorValues[8]) {
    uint8_t pattern = 0;
    _activeCount = 0;
    for (int i = 0; i < 8; i++) {
        if (sensorValues[i] > SENSOR_THRESHOLD) {
            pattern |= (1 << i);
            _activeCount++;
        }
    }
    return pattern;
}

void JunctionDetector::analyzePattern(uint8_t pattern) {
    // Vùng trái: Sensor 0, 1
    _hasLeft = (pattern & 0b00000011) != 0;
    
    // Vùng phải: Sensor 6, 7
    _hasRight = (pattern & 0b11000000) != 0;
    
    // Vùng giữa: Sensor 2, 3, 4, 5
    _hasCenter = (pattern & 0b00111100) != 0;
}

JunctionType JunctionDetector::classify() {
    // Kiểm tra tính đối xứng và xu hướng để phát hiện Fake Blob trước
    bool isSymmetric = checkSymmetry(_currentPattern);

    // Nếu đang thấy xu hướng mở rộng -> thu hẹp đối xứng mạnh mẽ
    if (_expansionTrend < 0 && _reachedPeak && isSymmetric) {
        return JunctionType::FAKE_INTERSECTION;
    }

    // Các cờ ổn định (Stable Flags)
    bool stableLeft   = (_leftPersistence >= STABLE_THRESHOLD);
    bool stableRight  = (_rightPersistence >= STABLE_THRESHOLD);
    bool stableCenter = (_centerPersistence >= STABLE_THRESHOLD);

    // Trường hợp 1: Tất cả đen
    if (_activeCount >= 7) {
        if (_expansionTrend > 2) return JunctionType::ALL_BLACK; 
        else return JunctionType::NONE; 
    }

    // Trường hợp 2: Ngã 4 hoặc T-Junction
    if (stableLeft && stableRight) {
        if (stableCenter) return JunctionType::CROSSROAD;
        else return JunctionType::T_JUNCTION;
    }

    // Trường hợp 3: Nhánh trái (Ngã 3 hoặc Góc vuông)
    if (stableLeft) {
        // Nếu line giữa vẫn ổn định -> Ngã 3 trái
        if (stableCenter) return JunctionType::LEFT_T;
        // Nếu mất line giữa -> Góc vuông rẽ trái (Sharp turn)
        else return JunctionType::SHARP_LEFT;
    }

    // Trường hợp 4: Nhánh phải
    if (stableRight) {
        if (stableCenter) return JunctionType::RIGHT_T;
        else return JunctionType::SHARP_RIGHT;
    }

    return JunctionType::NONE;
}

bool JunctionDetector::checkSymmetry(uint8_t pattern) {
    // So sánh bit 0-3 với gương của bit 4-7
    // Pattern: 7 6 5 4 | 3 2 1 0
    uint8_t left = (pattern >> 4) & 0x0F;
    uint8_t right = pattern & 0x0F;
    
    // Đảo ngược 4 bit của right để so sánh với left
    uint8_t mirroredRight = 0;
    if (right & 0x01) mirroredRight |= 0x08;
    if (right & 0x02) mirroredRight |= 0x04;
    if (right & 0x04) mirroredRight |= 0x02;
    if (right & 0x08) mirroredRight |= 0x01;

    return (left == mirroredRight);
}

void JunctionDetector::updateTrend(uint8_t activeCount) {
    uint8_t lastIndex = (_historyIndex == 0) ? (HISTORY_SIZE - 1) : (_historyIndex - 1);
    uint8_t prevCount = _countHistory[lastIndex];

    if (activeCount > prevCount) {
        _expansionTrend++;
        if (_expansionTrend > 5) _expansionTrend = 5;
    } else if (activeCount < prevCount) {
        _expansionTrend--;
        if (_expansionTrend < -5) _expansionTrend = -5;
    }

    if (activeCount >= 7) {
        _reachedPeak = true;
        _maxActiveInSequence = activeCount;
    }

    // Nếu số mắt đọc về mức bình thường (2-3 mắt) -> Reset trạng thái sequence
    if (activeCount <= 3) {
        _reachedPeak = false;
        _expansionTrend = 0;
    }
}

const char* JunctionDetector::getTypeName() const {
    switch (_currentType) {
        case JunctionType::LEFT_T:      return "LEFT_T";
        case JunctionType::RIGHT_T:     return "RIGHT_T";
        case JunctionType::T_JUNCTION:  return "T_JUNCTION";
        case JunctionType::CROSSROAD:   return "CROSSROAD";
        case JunctionType::ALL_BLACK:   return "ALL_BLACK";
        case JunctionType::SHARP_LEFT:  return "SHARP_LEFT";
        case JunctionType::SHARP_RIGHT: return "SHARP_RIGHT";
        case JunctionType::FAKE_INTERSECTION: return "FAKE_BLOB";
        default:                        return "NONE";
    }
}
