#include "state_machine.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLEHIDDevice.h>

// LED 引脚定义
#define LED_D4_PIN 12  // 高电平有效
#define LED_D5_PIN 13  // 高电平有效

// 全局变量声明
extern NimBLEServer *pServer;
extern NimBLEHIDDevice *hid;
extern NimBLECharacteristic *inputMouse;
extern bool deviceConnected;
extern unsigned long buttonPressStartTime;
extern unsigned long lastBlinkTime;
extern bool ledState;
extern int blinkCount;

// 定义静态成员
float MouseMotionEnable::angle = 0;

// Init状态实现
void Init::entry() {
    Serial.println("进入初始化状态");
    // 初始化LED
    pinMode(LED_D4_PIN, OUTPUT);
    pinMode(LED_D5_PIN, OUTPUT);
    digitalWrite(LED_D4_PIN, LOW);
    digitalWrite(LED_D5_PIN, LOW);
    
    // 初始化BLE将在main.cpp中完成
    // 发送初始化完成事件
    BleMouseState::dispatch(InitComplete());
}

void Init::react(InitComplete const &) {
    transit<Idle>();
}

// Idle状态实现
void Idle::entry() {
    Serial.println("进入空闲状态");
    digitalWrite(LED_D4_PIN, LOW);
    digitalWrite(LED_D5_PIN, LOW);
    
    // 检查是否有之前连接的设备
    // 这里简化处理，假设没有已保存的设备
    // 在实际应用中可以从EEPROM或SPIFFS读取
}

void Idle::react(BootButtonLongPress const &) {
    transit<Pairing>();
}

void Idle::react(BootButtonShortPress const &) {
    // 在空闲状态下短按无效
}

// Reconnect状态实现
void Reconnect::entry() {
    Serial.println("进入重连状态");
    reconnectStartTime = millis();
    // 开始重连逻辑
    startReconnection();
}

void Reconnect::react(DeviceConnected const &) {
    transit<Connected>();
}

void Reconnect::react(BootButtonLongPress const &) {
    transit<Pairing>();
}

void Reconnect::react(ConnectionTimeout const &) {
    // 重连超时，继续尝试
    startReconnection();
}

void Reconnect::react(ConnectionFailed const &) {
    // 连接失败，继续尝试
    startReconnection();
}

void Reconnect::startReconnection() {
    Serial.println("尝试重新连接...");
    // 实现重连逻辑
    // 这里简化处理，实际需要查找并连接已知设备
}

void Reconnect::checkTimeout() {
    if (millis() - reconnectStartTime > Reconnect::RECONNECT_TIMEOUT) {
        BleMouseState::dispatch(ConnectionTimeout());
    }
}

// Pairing状态实现
void Pairing::entry() {
    Serial.println("进入配对状态");
    pairingStartTime = millis();
    blinkCount = 0;
    lastBlinkTime = millis();
    
    // 重新开始BLE广播
    startPairing();
}

void Pairing::react(DeviceConnected const &) {
    transit<Connected>();
}

void Pairing::react(BootButtonLongPress const &) {
    // 在配对状态下长按无效
}

void Pairing::react(PairingTimeout const &) {
    transit<Reconnect>();
}

void Pairing::react(ConnectionFailed const &) {
    transit<Reconnect>();
}

void Pairing::startPairing() {
    Serial.println("开始蓝牙配对...");
    // 重新开始BLE广播
    if (pServer) {
        NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
        pAdvertising->stop();
        delay(1000);
        pAdvertising->start();
    }
}

void Pairing::checkTimeout() {
    if (millis() - pairingStartTime > Pairing::PAIRING_TIMEOUT) {
        BleMouseState::dispatch(PairingTimeout());
    }
}

// Connected状态实现
void Connected::entry() {
    Serial.println("进入连接状态");
    digitalWrite(LED_D4_PIN, LOW);
    digitalWrite(LED_D5_PIN, LOW);
    // 连接成功后立即进入鼠标移动禁用状态
    // 从这里用户可以通过按键启用鼠标移动
    transit<MouseMotionDisable>();
}

void Connected::react(BootButtonLongPress const &) {
    transit<Pairing>();
}

void Connected::react(DeviceDisconnected const &) {
    transit<Reconnect>();
}

// MouseMotionDisable状态实现
void MouseMotionDisable::entry() {
    Serial.println("鼠标移动已禁用");
    digitalWrite(LED_D4_PIN, HIGH);
    digitalWrite(LED_D5_PIN, HIGH);
}

void MouseMotionDisable::react(BootButtonShortPress const &) {
    transit<MouseMotionEnable>();
}

// MouseMotionEnable状态实现
void MouseMotionEnable::entry() {
    Serial.println("鼠标移动已启用");
    angle = 0;
}

void MouseMotionEnable::react(BootButtonShortPress const &) {
    transit<MouseMotionDisable>();
}

void MouseMotionEnable::moveMouse() {
    // 圆形移动模式
    int8_t deltaX = (int8_t)(5 * cos(angle));
    int8_t deltaY = (int8_t)(5 * sin(angle));
    uint8_t buttons = 0;
    
    // 构建鼠标报告
    uint8_t mouseReport[4] = {buttons, 0, (uint8_t)deltaX, (uint8_t)deltaY};
    
    if (inputMouse && deviceConnected) {
        inputMouse->setValue(mouseReport, sizeof(mouseReport));
        inputMouse->notify();
    }
    
    angle += 0.1;
    if (angle >= 2 * PI) {
        angle = 0;
    }
}

void MouseMotionEnable::updateLED() {
    // LED D4、D5 交替闪烁，每秒2次
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= 250) { // 每250ms切换一次
        ledState = !ledState;
        digitalWrite(LED_D4_PIN, ledState ? HIGH : LOW);
        digitalWrite(LED_D5_PIN, ledState ? LOW : HIGH);
        lastBlinkTime = currentTime;
    }
}

namespace tinyfsm {
  template<> 
  void Fsm<BleMouseState>::set_initial_state(void) {
    current_state_ptr = &_state_instance<Init>::value;
  }
}