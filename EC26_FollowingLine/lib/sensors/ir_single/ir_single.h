#pragma once

class IrSingle {
public:
    void begin();
    bool isDetected(); // true = phát hiện vật cản
};

extern IrSingle irSingle;
