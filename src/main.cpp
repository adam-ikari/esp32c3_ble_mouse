#include <Arduino.h>
#include <math.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLEHIDDevice.h>
#include "state_machine.h"

// 按键引脚定义
#define BOOT_BUTTON_PIN 9  // BOOT 按键，低电平有效

// HID 报告描述符 - 鼠标
static const uint8_t hid_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Buttons)
    0x19, 0x01,        //     Usage Minimum (1)
    0x29, 0x03,        //     Usage Maximum (3)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x03,        //     Report Count (3)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5)
    0x81, 0x03,        //     Input (Constant) or Padding
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x06,        //     Input (Data, Variable, Relative)
    0xC0,              //   End Collection
    0xC0               // End Collection
};

// 全局变量
NimBLEServer *pServer = nullptr;
NimBLEHIDDevice *hid = nullptr;
NimBLECharacteristic *inputMouse = nullptr;
bool deviceConnected = false;
unsigned long buttonPressStartTime = 0;
unsigned long lastBlinkTime = 0;
bool ledState = false;
int blinkCount = 0;

// 鼠标移动状态变量
int8_t currentMomentumX = 0;
int8_t currentMomentumY = 0;
int8_t targetMomentumX = 0;
int8_t targetMomentumY = 0;
unsigned long momentumChangeTimer = 0;
unsigned int momentumChangeInterval = 2000; // 每2秒改变一次动量方向

// 回调类：连接状态改变
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("BLE设备已连接");
        Serial.println("客户端数量: " + String(pServer->getConnectedCount()));
        Serial.println("尝试发送DeviceConnected事件到状态机");
        BleMouseState::dispatch(DeviceConnected());
        Serial.println("DeviceConnected事件已发送");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE设备已断开连接");
        BleMouseState::dispatch(DeviceDisconnected());
        // 重新开始广播
        NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
        pAdvertising->start();
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32C3 BLE Mouse 启动中...");
    
    // 初始化随机数生成器
    randomSeed(analogRead(0) + millis());
    Serial.println("随机数生成器已初始化");
    
    // 初始化按键引脚
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    
    // 初始化 BLE
    NimBLEDevice::init("ESP32C3 BLE Mouse");
    // 设置BLE安全参数 用于HID设备
    NimBLEDevice::setSecurityAuth(true, true, true);
    
    // 创建 BLE 服务器
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    // 创建 HID 设备
    hid = new NimBLEHIDDevice(pServer);
    
    // 设置制造商信息
    hid->setManufacturer("Espressif");
    hid->setPnp(0x02, 0x05C3, 0x0001, 0x0001);
    hid->setHidInfo(0x00, 0x01);
    
    // 设置鼠标输入特征
    inputMouse = hid->getInputReport(1); // Report ID 1 for Mouse
    // 设置输入报告回调，以便接收来自客户端的报告
    
    // 设置 HID 报告描述符
    hid->setReportMap((uint8_t*)hid_report_descriptor, sizeof(hid_report_descriptor));
    
    // 根据标准BLE HID设备要求配置
    // 设置电池服务（可选，但有些设备会期望这个）
    hid->setBatteryLevel(100); // 设置初始电池电量为100%
    
    // 启动HID服务
    hid->startServices();
    
    // 设置广播
    NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(HID_MOUSE);
    pAdvertising->addServiceUUID(hid->getHidService()->getUUID());
    // 设置广播名称
    pAdvertising->setName("ESP32C3 BLE Mouse");
    pAdvertising->start();
    Serial.println("广播已启动，设备应可在手机上看到并连接");
    Serial.println("当前广播状态：" + String(pAdvertising->isAdvertising() ? "正在广播" : "未广播"));
    Serial.println("HID服务已启动，设备准备就绪");
    
    Serial.println("BLE 鼠标服务已启动");
    Serial.println("服务器回调已设置，等待连接...");
    
    // 初始化状态机
    Serial.println("启动状态机...");
    BleMouseState::start();
    Serial.println("状态机已启动");
    
    // 发送初始化完成事件
    Serial.println("发送初始化完成事件...");
    BleMouseState::dispatch(InitComplete());
    Serial.println("初始化完成事件已发送");
}

void loop() {
    // 检查按键状态
    bool buttonPressed = (digitalRead(BOOT_BUTTON_PIN) == LOW);
    
    if (buttonPressed) {
        if (buttonPressStartTime == 0) {
            // 按键刚按下
            buttonPressStartTime = millis();
        } else {
            // 按键持续按下
            unsigned long pressDuration = millis() - buttonPressStartTime;
            
            // 长按 5 秒进入配对模式
            if (pressDuration >= 5000) {
                BleMouseState::dispatch(BootButtonLongPress());
                // 等待按键释放以避免重复触发
                while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
                    delay(10);
                }
                buttonPressStartTime = 0;
            }
        }
    } else {
        if (buttonPressStartTime > 0) {
            // 按键刚释放
            unsigned long pressDuration = millis() - buttonPressStartTime;
            
            // 短按切换鼠标移动
            if (pressDuration < 1000) {
                BleMouseState::dispatch(BootButtonShortPress());
            }
            
            buttonPressStartTime = 0;
        }
    }
    
    // 定期检查连接状态并手动触发状态转换（如果回调未被触发）
    static unsigned long lastConnectionCheck = 0;
    if (millis() - lastConnectionCheck > 1000) { // 每秒检查一次
        int connectedCount = pServer ? pServer->getConnectedCount() : 0;
        if (connectedCount > 0 && !deviceConnected) {
            // 检测到连接但状态未更新，手动触发连接事件
            Serial.println("检测到连接但回调未触发，手动触发DeviceConnected事件");
            Serial.println("当前连接数: " + String(connectedCount));
            deviceConnected = true;
            BleMouseState::dispatch(DeviceConnected());
        } else if (connectedCount == 0 && deviceConnected) {
            // 检测到断开连接但状态未更新
            Serial.println("检测到断开连接，手动触发DeviceDisconnected事件");
            deviceConnected = false;
            BleMouseState::dispatch(DeviceDisconnected());
        }
        lastConnectionCheck = millis();
    }
    
    // 处理配对模式的 LED 闪烁
    if (BleMouseState::is_in_state<Pairing>()) {
        unsigned long currentTime = millis();
        // 每秒闪烁 3 次，即每 333ms 闪烁一次
        if (currentTime - lastBlinkTime >= 333) {
            ledState = !ledState;
            digitalWrite(12, ledState ? HIGH : LOW);  // LED_D4_PIN
            digitalWrite(13, ledState ? HIGH : LOW);  // LED_D5_PIN
            lastBlinkTime = currentTime;
        }
    }
    
    // 处理重连状态的 LED 闪烁
    if (BleMouseState::is_in_state<Reconnect>()) {
        unsigned long currentTime = millis();
        // 每秒闪烁 1 次，即每 1000ms 闪烁一次
        if (currentTime - lastBlinkTime >= 1000) {
            ledState = !ledState;
            digitalWrite(12, ledState ? HIGH : LOW);  // LED_D4_PIN
            digitalWrite(13, ledState ? HIGH : LOW);  // LED_D5_PIN
            lastBlinkTime = currentTime;
        }
    }
    
    // 处理鼠标移动状态 - 基于随机动量的移动
    if (BleMouseState::is_in_state<MouseMotionEnable>()) {
        // 调用鼠标移动函数，实现随机动量移动
        unsigned long currentTime = millis();
        
        // 每隔一段时间随机改变目标动量
        if (currentTime - momentumChangeTimer > momentumChangeInterval) {
            // 生成随机的目标动量值 (-5 到 5)
            targetMomentumX = random(-5, 6);
            targetMomentumY = random(-5, 6);
            momentumChangeTimer = currentTime;
            Serial.println("动量目标已更新: X=" + String(targetMomentumX) + ", Y=" + String(targetMomentumY));
        }
        
        // 平滑地更新当前动量到目标动量
        if (currentMomentumX < targetMomentumX) currentMomentumX++;
        else if (currentMomentumX > targetMomentumX) currentMomentumX--;
        
        if (currentMomentumY < targetMomentumY) currentMomentumY++;
        else if (currentMomentumY > targetMomentumY) currentMomentumY--;
        
        // 限制最大动量值
        if (currentMomentumX > 5) currentMomentumX = 5;
        if (currentMomentumX < -5) currentMomentumX = -5;
        if (currentMomentumY > 5) currentMomentumY = 5;
        if (currentMomentumY < -5) currentMomentumY = -5;
        
        // 发送鼠标移动数据
        uint8_t buttons = 0;
        uint8_t mouseReport[4] = {buttons, 0, (uint8_t)currentMomentumX, (uint8_t)currentMomentumY};
        
        if (inputMouse && deviceConnected) {
            inputMouse->setValue(mouseReport, sizeof(mouseReport));
            inputMouse->notify();
        }
        
        // LED D4、D5 交替闪烁，每秒2次
        if (currentTime - lastBlinkTime >= 250) { // 每250ms切换一次
            ledState = !ledState;
            digitalWrite(12, ledState ? HIGH : LOW);  // LED_D4_PIN
            digitalWrite(13, ledState ? LOW : HIGH);  // LED_D5_PIN
            lastBlinkTime = currentTime;
        }
    }
    
    delay(10);
}