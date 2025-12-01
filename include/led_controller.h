#pragma once

#include <Arduino.h>

// LED 引脚定义
#define LED_D4_PIN 12  // 高电平有效
#define LED_D5_PIN 13  // 高电平有效

class LEDController {
public:
    // LED模式枚举
    enum class Mode {
        OFF,            // 全部关闭
        ON,             // 全部常亮
        SLOW_BLINK,     // 慢速同步闪烁 (1Hz)
        FAST_BLINK,     // 快速同步闪烁 (3Hz)
        ALTERNATE,      // 交替闪烁 (2Hz)
        HEARTBEAT       // 心跳模式
    };

private:
    static bool initialized;
    static unsigned long lastBlinkTime;
    static bool ledState;
    static Mode currentMode;
    static unsigned long blinkInterval;
    static bool d4State;
    static bool d5State;

public:
    // 初始化LED引脚
    static void init();
    
    // 设置LED状态
    static void setState(bool d4, bool d5);
    
    // 设置LED模式
    static void setMode(Mode mode);
    
    // 获取当前LED模式
    static Mode getMode();
    
    // 更新LED状态（需要在loop中调用）
    static void update();
    
    // 简单的LED控制函数
    static void turnOff();
    static void turnOn();
    static void blinkSync(int intervalMs = 1000);  // 同步闪烁
    static void blinkAlternate(int intervalMs = 500);  // 交替闪烁
};