# Cập nhật hệ thống điều khiển Robot - Sửa lỗi bánh quay ngược & Thêm hệ thống Cua + FIX WEB BLE

## 🆘 FIX WEB BLE - Giải quyết nút "Kết nối Bluetooth" không hoạt động

### ❌ **Lỗi chính:**
Hàm `handleNotifications()` không được định nghĩa, nhưng lại được gọi trong `connectToRobot()`. 
Điều này khiến tất cả JavaScript không chạy được, và nút Connect BLE bị block.

### ✅ **Cách sửa:**

**File:** `index.html`

**Vấn đề:** Dòng 1055 gọi `handleNotifications` nhưng hàm không tồn tại
```js
txCharacteristic.addEventListener('characteristicvaluechanged', handleNotifications);
```

**Giải pháp:** Thêm định nghĩa hàm đầy đủ
```js
function handleNotifications(event) {
    const data = new Uint8Array(event.target.value);
    const text = new TextDecoder().decode(data);
    const lines = text.split('\n');

    lines.forEach((line) => {
        line = line.trim();
        if (!line) return;

        // Parse sensor data, PID, mode, state, etc.
        if (line.startsWith('A:')) { /* ... */ }
        if (line.startsWith('D:')) { /* ... */ }
        // ... more parsing ...
    });
}
```

### 📊 **Thêm Debug Console Logging:**
- ✅ Kiểm tra `navigator.bluetooth` support
- ✅ Log mỗi bước kết nối BLE
- ✅ Báo lỗi cụ thể (NotFoundError, NotSupportedError, NotAllowedError)
- ✅ Hỗ trợ troubleshooting trên console browser

**Ví dụ output console:**
```
🌐 Page loaded successfully
🔵 Web Bluetooth supported: true
🖱️  Connect button clicked, isConnected: false
🔵 Starting BLE connection...
📱 Navigator.bluetooth available: true
✅ Device found: ESP32_Robot
✅ GATT Server connected
✅ Service found
✅ Characteristics found
✅ Notifications enabled
🎉 BLE Connection successful
```

### 📱 **Hỗ trợ Browsers:**
- ✅ Chrome (desktop & mobile)
- ✅ Edge
- ✅ Opera
- ✅ Samsung Internet (Android)
- ❌ Safari (iOS)
- ❌ Firefox (partial support)

### 🔍 **Cách kiểm tra:**
1. Mở web: https://kien200307-edu.github.io/EC26_FollowingLine_new/
2. Mở DevTools: F12 → Console
3. Bấm "Kết nối Bluetooth"
4. Xem console để debug nếu có lỗi

---

## 📋 Tóm tắt các thay đổi

### 1. **SỬA LỖI BÁNH QUAY NGƯỢC CHIỀU** ⚠️

#### Nguyên nhân lỗi:
Trong hàm `normalPID()`, khi PID tính toán adjustment quá lớn, nó khiến một bánh quay backwards (âm) thay vì tất cả bánh cùng quay về một hướng.

**Mã cũ (có lỗi):**
```cpp
int leftSpeed = _baseSpeed + adjustment;
int rightSpeed = _baseSpeed - adjustment;

leftSpeed = constrain(leftSpeed, -255, 255);   // ❌ Cho phép đảo chiều
rightSpeed = constrain(rightSpeed, -255, 255); // ❌ Cho phép đảo chiều
```

**Mã mới (đã sửa):**
```cpp
adjustment = constrain(adjustment, -_baseSpeed, _baseSpeed);

int leftSpeed = _baseSpeed + adjustment;
int rightSpeed = _baseSpeed - adjustment;

// Đảm bảo cả 2 bánh luôn quay cùng chiều
leftSpeed = constrain(leftSpeed, -_baseSpeed, _baseSpeed);
rightSpeed = constrain(rightSpeed, -_baseSpeed, _baseSpeed);
```

**Giải thích:**
- Adjustment bị giới hạn không vượt quá `baseSpeed`
- Cả 2 bánh được constrain với giới hạn là `_baseSpeed` thay vì `255`
- **Kết quả:** Cả 2 bánh luôn quay về cùng chiều, chỉ khác tốc độ để thay đổi hướng

---

### 2. **THÊM HỆ THỐNG CUA GẮT, THƯỜNG, NHẸ** 🎛️

Bây giờ bạn có 3 bộ tham số PID riêng cho từng loại cua:

#### **Thêm vào `config.h`:**
```cpp
// Cua gắt (Sharp turn) - Phản ứng nhanh, xoay mạnh
#define PID_SHARP_KP_DEFAULT  2.5f
#define PID_SHARP_KI_DEFAULT  0.05f
#define PID_SHARP_KD_DEFAULT  8.0f

// Cua thường (Normal turn) - Cân bằng
#define PID_NORMAL_KP_DEFAULT 1.2f
#define PID_NORMAL_KI_DEFAULT 0.02f
#define PID_NORMAL_KD_DEFAULT 4.0f

// Cua nhẹ (Light turn) - Phản ứng chậm, mềm mại
#define PID_LIGHT_KP_DEFAULT  0.6f
#define PID_LIGHT_KI_DEFAULT  0.01f
#define PID_LIGHT_KD_DEFAULT  2.0f
```

#### **Thêm vào `robot_logic.h`:**
```cpp
enum TurnType {
  TURN_SHARP,
  TURN_NORMAL,
  TURN_LIGHT
};
```

#### **Thêm vào `robot_logic.cpp` - Các hàm mới:**

```cpp
// Chọn loại cua
void RobotLogic::setTurnType(TurnType type);

// Điều chỉnh PID cho cua gắt
void RobotLogic::setTurnSharpKp(float v);
void RobotLogic::setTurnSharpKi(float v);
void RobotLogic::setTurnSharpKd(float v);

// Điều chỉnh PID cho cua thường
void RobotLogic::setTurnNormalKp(float v);
void RobotLogic::setTurnNormalKi(float v);
void RobotLogic::setTurnNormalKd(float v);

// Điều chỉnh PID cho cua nhẹ
void RobotLogic::setTurnLightKp(float v);
void RobotLogic::setTurnLightKi(float v);
void RobotLogic::setTurnLightKd(float v);
```

---

## 🎮 HƯỚNG SỬ DỤNG

### **Cách 1: Thay đổi loại cua qua Serial**

```
// Chuyển sang cua gắt
TURNTYPE:SHARP

// Chuyển sang cua thường
TURNTYPE:NORMAL

// Chuyển sang cua nhẹ
TURNTYPE:LIGHT
```

### **Cách 2: Điều chỉnh tham số PID cho từng loại cua**

```
// Cua gắt
SHARP_P2.5
SHARP_I0.05
SHARP_D8.0

// Cua thường
NORMAL_P1.2
NORMAL_I0.02
NORMAL_D4.0

// Cua nhẹ
LIGHT_P0.6
LIGHT_I0.01
LIGHT_D2.0
```

### **Cách 3: Thử nghiệm nhanh**

```
// Kiểm tra loại cua hiện tại
// (Sẽ in ra console và qua BLE)

// Thay đổi tốc độ cơ sở
B180
```

---

## 📊 Bảng so sánh các loại cua

| Loại Cua | Kp    | Ki    | Kd   | Đặc điểm |
|----------|-------|-------|------|----------|
| **SHARP** | 2.5   | 0.05  | 8.0  | Phản ứng nhanh, xoay gắt (90° sharp turns) |
| **NORMAL** | 1.2   | 0.02  | 4.0  | Cân bằng (90° normal turns) |
| **LIGHT** | 0.6   | 0.01  | 2.0  | Phản ứng chậm, mềm mại (smooth curves) |

---

## 🔧 Cách sử dụng hiệu quả

1. **Ban đầu:** Bắt đầu với `TURN_NORMAL` (mặc định)

2. **Nếu robot quay quá mạnh:** Chuyển sang `TURN_LIGHT`
   ```
   TURNTYPE:LIGHT
   LIGHT_P0.4
   LIGHT_I0.005
   ```

3. **Nếu robot không xoay đủ nhanh:** Chuyển sang `TURN_SHARP`
   ```
   TURNTYPE:SHARP
   SHARP_P3.0
   SHARP_I0.06
   ```

4. **Tinh chỉnh từng tham số:**
   ```
   SHARP_P3.0   // Tăng Kp để xoay nhanh hơn
   SHARP_D10.0  // Tăng Kd để ổn định hơn
   ```

---

## ✅ Kiểm tra kết quả

Sau khi build và upload:

1. **Kiểm tra bánh quay thẳng:**
   - Gửi lệnh: `B150` (đặt tốc độ 150)
   - Nhận diện: Cả 2 bánh phải quay về cùng chiều, cùng tốc độ

2. **Kiểm tra quay phải/trái:**
   - Gửi lệnh: `TURNTYPE:NORMAL`
   - Gửi lệnh: `P1.0` (đặt Kp=1.0 để test)
   - Robot nên quay mều hơn so với cũ

3. **Xem log từ console:**
   ```
   [TurnType] Changed to: NORMAL
   [PID_NORMAL] Kp = 1.20
   ```

---

## 📝 Files được thay đổi

1. **[lib/core/config.h](lib/core/config.h)**
   - Thêm 9 define mới cho PID parameters của 3 loại cua

2. **[lib/brain/logic/robot_logic.h](lib/brain/logic/robot_logic.h)**
   - Thêm enum `TurnType`
   - Thêm 3 PID pointer: `_pidSharp`, `_pidNormal`, `_pidLight`
   - Thêm 9 hàm setter cho turn types

3. **[lib/brain/logic/robot_logic.cpp](lib/brain/logic/robot_logic.cpp)**
   - Sửa hàm `normalPID()` - **FIX LỖI BÁNH QUAY NGƯỢC**
   - Thêm khởi tạo 3 PID controllers trong `begin()`
   - Thêm 10 hàm mới cho turn types
   - Cập nhật `handleUARTTuning()` để hỗ trợ lệnh mới

---

## 🎯 Lợi ích

✅ **Sửa lỗi bánh quay ngược** - Cả 2 bánh lúc nào cũng quay cùng chiều
✅ **Linh hoạt hơn** - Có thể lựa chọn độ gắt của cua tùy vào đặc tính đường đua
✅ **Dễ điều chỉnh** - Riêng biệt PID cho từng loại cua
✅ **Tương thích ngược** - Vẫn hỗ trợ các lệnh cũ (P/I/D/B)

---

## 🐛 Debug tips

Nếu gặp vấn đề:

1. **Console sẽ in:**
   ```
   [TurnType] Changed to: SHARP
   [PID_SHARP] Kp = 2.50
   ```

2. **BLE sẽ gửi:**
   ```
   TURNTYPE:SHARP
   Sharp_Kp=2.50
   ```

3. **Nếu bánh vẫn quay ngược:**
   - Kiểm tra kết nối motor
   - Kiểm tra `motor.setSpeed()` trong [lib/actuators/motor/motor.cpp](lib/actuators/motor/motor.cpp)
   - Có thể motor wiring bị đảo (IN3/IN4 hoặc IN1/IN2)

---

**Build Status:** ✅ SUCCESS (Compiled without errors)
**Last Updated:** 2026-05-14
