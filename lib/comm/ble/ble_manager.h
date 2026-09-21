#pragma once

#include <Arduino.h>

// =====================================================
// BLE Manager (NimBLE-Arduino)
// Giao thức Bluefruit Connect:
//   Control Pad : !Bxy  (x = nút 1-8, y = '1' nhấn / '0' nhả)
//   Slider      : !Sxvvvv (x = slider 1-4, v = float 4 bytes LE)
//   UART        : chuỗi text thông thường
// =====================================================

class BleManager {
public:
    void begin();

    // Gọi trong loop() – xử lý callback NimBLE
    // (NimBLE dùng task riêng, hàm này dùng để flush buffer nếu cần)
    bool available();
    String readCommand();

    // Gửi dữ liệu về app
    void send(const String& msg);

    bool isConnected() const;

private:
    friend class RxCallbacks;
};

extern BleManager bleManager;
