#include "ble_manager.h"
#include "core/config.h"

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

// =====================================================
// Bluefruit UART Service UUIDs (Nordic UART / Adafruit)
// =====================================================
#define UART_SERVICE_UUID    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_RX_CHAR_UUID    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // App -> ESP32
#define UART_TX_CHAR_UUID    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // ESP32 -> App

// =====================================================
// Globals được chia sẻ với robot_logic
// =====================================================
volatile bool g_bleConnected   = false;
volatile bool g_bleDisconnFlag = false; // set true khi mất kết nối

// =====================================================
// Ring buffer nhỏ cho lệnh đến
// =====================================================
static const int CMD_QUEUE_SIZE = 8;
static String    cmdQueue[CMD_QUEUE_SIZE];
static volatile int cmdHead = 0;
static volatile int cmdTail = 0;

static void pushCmd(const String& s) {
    int next = (cmdHead + 1) % CMD_QUEUE_SIZE;
    if (next != cmdTail) {
        cmdQueue[cmdHead] = s;
        cmdHead = next;
    }
}

// =====================================================
// NimBLE Callbacks
// =====================================================
static NimBLECharacteristic* pTxChar = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        g_bleConnected   = true;
        g_bleDisconnFlag = false;
        Serial.println("[BLE] Connected");
    }

    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        g_bleConnected   = true;
        g_bleDisconnFlag = false;
        // Tối ưu: Yêu cầu tham số kết nối cực nhanh (7.5ms - 15ms)
        pServer->updateConnParams(desc->conn_handle, 12, 24, 0, 120);
        Serial.printf("[BLE] Connected to %s\n", NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    }

    void onDisconnect(NimBLEServer* pServer) override {
        g_bleConnected   = false;
        g_bleDisconnFlag = true;
        Serial.println("[BLE] Disconnected – restarting advertising");
        NimBLEDevice::startAdvertising();
    }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        std::string val = pChar->getValue();
        if (!val.empty()) {
            pushCmd(String(val.c_str()));
        }
    }
};

// =====================================================
// BleManager
// =====================================================
BleManager bleManager;

void BleManager::begin() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setMTU(185);
    // Tối ưu: Đặt công suất phát tối đa để kết nối ổn định hơn ở tầm xa
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); 

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(UART_SERVICE_UUID);

    // TX characteristic (notify, ESP32 -> App)
    pTxChar = pService->createCharacteristic(
        UART_TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // RX characteristic (write, App -> ESP32)
    NimBLECharacteristic* pRxChar = pService->createCharacteristic(
        UART_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxChar->setCallbacks(new RxCallbacks());

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(UART_SERVICE_UUID);
    pAdv->setScanResponse(true);
    
    // Tối ưu: Quảng bá nhanh hơn (20ms - 40ms) giúp điện thoại tìm thấy ngay lập tức
    pAdv->setMinInterval(32); 
    pAdv->setMaxInterval(64); 
    // Tối ưu: Gợi ý tham số kết nối cho iOS/Android để giảm độ trễ
    pAdv->setMinPreferred(0x06); // 7.5ms
    pAdv->setMaxPreferred(0x12); // 22.5ms
    
    NimBLEDevice::startAdvertising();
    Serial.println("[BLE] Advertising as: " BLE_DEVICE_NAME);
}

bool BleManager::available() {
    return cmdHead != cmdTail;
}

String BleManager::readCommand() {
    if (cmdHead == cmdTail) return "";
    String cmd = cmdQueue[cmdTail];
    cmdTail = (cmdTail + 1) % CMD_QUEUE_SIZE;
    return cmd;
}

void BleManager::send(const String& msg) {
    if (pTxChar && g_bleConnected && msg.length() > 0) {
        pTxChar->setValue(msg.c_str());
        pTxChar->notify();
        // Độ trễ nhỏ để tránh nghẽn buffer trên một số điện thoại
        delay(1); 
    }
}

bool BleManager::isConnected() const {
    return g_bleConnected;
}
