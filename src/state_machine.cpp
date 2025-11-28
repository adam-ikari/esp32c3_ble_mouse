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

// 鼠标移动状态变量外部声明
extern int8_t currentMomentumX;
extern int8_t currentMomentumY;
extern int8_t targetMomentumX;
extern int8_t targetMomentumY;
extern unsigned long momentumChangeTimer;
extern unsigned int momentumChangeInterval;

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
    
    // 初始化全局变量
    deviceConnected = false;
    buttonPressStartTime = 0;
    lastBlinkTime = 0;
    ledState = false;
    blinkCount = 0;
    
    Serial.println("LED引脚初始化完成");
    Serial.println("全局变量初始化完成");
    Serial.println("等待初始化完成事件...");
}

void Init::react(InitComplete const &) {
    Serial.println("接收到初始化完成事件，检查是否需要重新连接到已配对设备");
    // 根据设计，在初始化完成后先进入Reconnect状态尝试连接之前配对的设备
    transit<Reconnect>();
}

// Idle状态实现
void Idle::entry() {
    Serial.println("进入空闲状态 - 设备可被发现和连接");
    digitalWrite(LED_D4_PIN, LOW);
    digitalWrite(LED_D5_PIN, LOW);
    
    // 确保设备处于可被发现状态
    if (pServer) {
        NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
        if (!pAdvertising->isAdvertising()) {
            pAdvertising->start();
            Serial.println("启动广播，设备现在可被发现");
        } else {
            Serial.println("广播已在运行");
        }
    }
    
    // 检查是否已有连接的设备
    if (pServer && pServer->getConnectedCount() > 0) {
        Serial.println("检测到已有连接的设备，发送DeviceConnected事件");
        BleMouseState::dispatch(DeviceConnected());
    } else {
        Serial.println("空闲状态下设备可被连接...");
        Serial.println("等待已配对设备自动连接或新设备连接...");
    }
}

void Idle::react(BootButtonLongPress const &) {
    Serial.println("在空闲状态下长按按钮，切换到配对状态");
    transit<Pairing>();
}

void Idle::react(BootButtonShortPress const &) {
    Serial.println("在空闲状态下短按按钮，无操作");
    // 在空闲状态下短按无效
}

void Idle::react(DeviceConnected const &) {
    Serial.println("在空闲状态下设备已连接，切换到连接状态");
    transit<Connected>();
}

void Idle::react(DeviceDisconnected const &) {
    Serial.println("在空闲状态下设备断开连接，保持空闲状态");
    // 保持在当前状态
}

void Idle::react(ConnectionTimeout const &) {
    Serial.println("在空闲状态下连接超时，保持空闲状态");
    // 保持在当前状态
}

void Idle::react(PairingTimeout const &) {
    Serial.println("在空闲状态下配对超时，保持空闲状态");
    // 保持在当前状态
}

void Idle::react(ConnectionFailed const &) {
    Serial.println("在空闲状态下连接失败，保持空闲状态");
    // 保持在当前状态
}

void Idle::react(InitComplete const &) {
    Serial.println("在空闲状态下接收到初始化完成事件，保持空闲状态");
    // 保持在当前状态
}

// Reconnect状态实现
void Reconnect::entry() {
    Serial.println("进入重连状态 - 尝试连接之前配对的设备");
    digitalWrite(LED_D4_PIN, LOW);
    digitalWrite(LED_D5_PIN, LOW);
    reconnectStartTime = millis();
    
    // 确保广播是开启的，以便已配对设备可以连接
    if (pServer) {
        NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
        if (!pAdvertising->isAdvertising()) {
            pAdvertising->start();
            Serial.println("启动广播以等待已配对设备连接");
        }
    }
    
    // 开始重连逻辑
    startReconnection();
}

void Reconnect::react(DeviceConnected const &) {
    Serial.println("在重连状态下设备已连接，切换到连接状态");
    transit<Connected>();
}

void Reconnect::react(BootButtonLongPress const &) {
    Serial.println("在重连状态下长按按钮，切换到配对状态");
    transit<Pairing>();
}

void Reconnect::react(ConnectionTimeout const &) {
    Serial.println("重连超时，继续尝试重连");
    // 重连超时，继续尝试
    startReconnection();
}

void Reconnect::react(ConnectionFailed const &) {
    Serial.println("连接失败，继续尝试连接");
    // 连接失败，继续尝试
    startReconnection();
}

void Reconnect::startReconnection() {
    Serial.println("尝试重新连接到之前配对的设备...");
    // 在BLE HID设备中，通常我们只需要保持广播开启
    // 已配对的设备会自动尝试连接
    // 也可以考虑特定的重新连接逻辑
}

void Reconnect::checkTimeout() {
    if (millis() - reconnectStartTime > Reconnect::RECONNECT_TIMEOUT) {
        BleMouseState::dispatch(ConnectionTimeout());
    }
}

// Pairing状态实现
void Pairing::entry() {
    Serial.println("进入配对状态");
    digitalWrite(LED_D4_PIN, LOW);
    digitalWrite(LED_D5_PIN, LOW);
    pairingStartTime = millis();
    blinkCount = 0;
    lastBlinkTime = millis();
    ledState = false;
    
    // 确保设备处于可被发现状态，允许新设备配对连接
    if (pServer) {
        NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
        if (pAdvertising->isAdvertising()) {
            pAdvertising->stop();
            delay(100);
        }
        // 重新启动广播以允许新的配对请求
        pAdvertising->start();
        Serial.println("广播已启动，设备进入配对模式");
    } else {
        Serial.println("错误：pServer为nullptr，无法启动广播");
    }
    
    // 重新开始BLE广播
    startPairing();
}

void Pairing::react(DeviceConnected const &) {
    Serial.println("在配对状态下设备已连接，切换到连接状态");
    transit<Connected>();
}

void Pairing::react(BootButtonLongPress const &) {
    Serial.println("在配对状态下长按按钮，无操作");
    // 在配对状态下长按无效
}

void Pairing::react(PairingTimeout const &) {
    Serial.println("配对超时，切换到重连状态");
    transit<Reconnect>();
}

void Pairing::react(ConnectionFailed const &) {
    Serial.println("配对连接失败，切换到重连状态");
    transit<Reconnect>();
}

void Pairing::startPairing() {
    Serial.println("开始蓝牙配对...");
    Serial.println("确保BLE广播正在运行...");
    // 重新开始BLE广播
    if (pServer) {
        NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
        Serial.println("停止当前广播...");
        pAdvertising->stop();
        delay(1000);
        Serial.println("启动新的广播...");
        pAdvertising->start();
        Serial.println("广播已启动，等待连接...");
    } else {
        Serial.println("错误：pServer为nullptr");
    }
}

void Pairing::checkTimeout() {
    if (millis() - pairingStartTime > Pairing::PAIRING_TIMEOUT) {
        BleMouseState::dispatch(PairingTimeout());
    }
}

// Connected状态实现
void Connected::entry() {
    Serial.println("进入连接状态 - LED常亮（鼠标移动功能处于禁用状态）");
    // LED常亮表示已连接，鼠标移动功能初始处于禁用状态
    digitalWrite(LED_D4_PIN, HIGH);
    digitalWrite(LED_D5_PIN, HIGH);
    // 连接后默认LED常亮，表示在Connected状态但鼠标移动功能禁用
    // 用户可以通过短按按钮切换到鼠标移动启用状态
    
    // 确保广播已停止，因为我们已经连接了
    if (pServer) {
        NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
        if (pAdvertising->isAdvertising()) {
            pAdvertising->stop();
            Serial.println("设备已连接，停止广播");
        }
    }
    
    Serial.println("连接状态设置完成，鼠标移动功能当前禁用");
}

void Connected::react(BootButtonShortPress const &) {
    Serial.println("在连接状态下短按按钮，切换到鼠标移动启用状态");
    // 短按切换到鼠标移动启用状态
    transit<MouseMotionEnable>();
}

void Connected::react(BootButtonLongPress const &) {
    Serial.println("在连接状态下长按按钮，返回配对模式");
    // 长按返回配对模式
    transit<Pairing>();
}

void Connected::react(DeviceConnected const &) {
    // 设备已经连接，保持当前状态
    Serial.println("接收到设备已连接事件，保持连接状态");
}

void Connected::react(DeviceDisconnected const &) {
    Serial.println("设备断开连接，返回重连状态");
    transit<Reconnect>();
}

void Connected::react(ConnectionTimeout const &) {
    Serial.println("连接超时事件，保持连接状态");
    // 默认处理，不执行状态转换
}

void Connected::react(PairingTimeout const &) {
    Serial.println("配对超时事件，保持连接状态");
    // 默认处理，不执行状态转换
}

void Connected::react(ConnectionFailed const &) {
    Serial.println("连接失败事件，保持连接状态");
    // 默认处理，不执行状态转换
}

void Connected::react(InitComplete const &) {
    Serial.println("初始化完成事件，保持连接状态");
    // 默认处理，不执行状态转换
}

// MouseMotionDisable状态实现
void MouseMotionDisable::entry() {
    Serial.println("进入鼠标移动禁用状态");
    // LED常亮表示已连接，但鼠标移动功能禁用
    digitalWrite(LED_D4_PIN, HIGH);
    digitalWrite(LED_D5_PIN, HIGH);
    
    // 确保鼠标报告不发送移动数据
    if (inputMouse && deviceConnected) {
        uint8_t mouseReport[4] = {0, 0, 0, 0}; // 无移动的空报告
        inputMouse->setValue(mouseReport, sizeof(mouseReport));
    }
}

void MouseMotionDisable::react(BootButtonShortPress const &) {
    Serial.println("在鼠标移动禁用状态下短按按钮，切换到鼠标移动启用状态");
    transit<MouseMotionEnable>();
}

void MouseMotionDisable::react(BootButtonLongPress const &) {
    Serial.println("在鼠标移动禁用状态下长按按钮，切换到配对模式");
    transit<Pairing>();
}

void MouseMotionDisable::react(DeviceConnected const &) {
    Serial.println("在鼠标移动禁用状态下接收到设备已连接事件，保持当前状态");
    // 默认处理，不执行状态转换
}

void MouseMotionDisable::react(DeviceDisconnected const &) {
    Serial.println("在鼠标移动禁用状态下设备断开连接，切换到重连状态");
    transit<Reconnect>();
}

void MouseMotionDisable::react(ConnectionTimeout const &) {
    Serial.println("在鼠标移动禁用状态下连接超时，保持当前状态");
    // 默认处理，不执行状态转换
}

void MouseMotionDisable::react(PairingTimeout const &) {
    Serial.println("在鼠标移动禁用状态下配对超时，保持当前状态");
    // 默认处理，不执行状态转换
}

void MouseMotionDisable::react(ConnectionFailed const &) {
    Serial.println("在鼠标移动禁用状态下连接失败，保持当前状态");
    // 默认处理，不执行状态转换
}

void MouseMotionDisable::react(InitComplete const &) {
    Serial.println("在鼠标移动禁用状态下初始化完成，保持当前状态");
    // 默认处理，不执行状态转换
}

// MouseMotionEnable状态实现
void MouseMotionEnable::entry() {
    Serial.println("进入鼠标移动启用状态");
    // 初始化鼠标移动参数
    angle = 0;
    // 在main.cpp中初始化了全局变量，这里可以重置为初始值
    currentMomentumX = 0;
    currentMomentumY = 0;
    targetMomentumX = 0;
    targetMomentumY = 0;
    momentumChangeTimer = millis();
    momentumChangeInterval = 2000; // 每2秒改变一次动量方向
    
    Serial.println("鼠标随机动量移动模式已启动");
}

void MouseMotionEnable::react(BootButtonShortPress const &) {
    Serial.println("在鼠标移动启用状态下短按按钮，切换到鼠标移动禁用状态");
    transit<MouseMotionDisable>();
}

namespace tinyfsm {
  template<> 
  void Fsm<BleMouseState>::set_initial_state(void) {
    current_state_ptr = &_state_instance<Init>::value;
  }
}