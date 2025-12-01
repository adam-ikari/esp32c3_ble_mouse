#include "led_controller.h"

// 静态成员变量定义
bool LEDController::initialized = false;
unsigned long LEDController::lastBlinkTime = 0;
bool LEDController::ledState = false;
LEDController::Mode LEDController::currentMode = LEDController::Mode::OFF;
unsigned long LEDController::blinkInterval = 1000;
bool LEDController::d4State = false;
bool LEDController::d5State = false;

void LEDController::init() {
    if (!initialized) {
        pinMode(LED_D4_PIN, OUTPUT);
        pinMode(LED_D5_PIN, OUTPUT);
        digitalWrite(LED_D4_PIN, LOW);
        digitalWrite(LED_D5_PIN, LOW);
        initialized = true;
        currentMode = Mode::OFF;
        d4State = false;
        d5State = false;
    }
}

void LEDController::setState(bool d4, bool d5) {
    d4State = d4;
    d5State = d5;
    digitalWrite(LED_D4_PIN, d4 ? HIGH : LOW);
    digitalWrite(LED_D5_PIN, d5 ? HIGH : LOW);
    currentMode = Mode::ON; // 当手动设置状态时，暂时脱离自动模式
}

void LEDController::setMode(Mode mode) {
    currentMode = mode;
    
    // 根据模式设置初始状态
    switch (mode) {
        case Mode::OFF:
            setState(false, false);
            break;
        case Mode::ON:
            setState(true, true);
            break;
        case Mode::ALTERNATE:
            setState(true, false); // 初始D4亮，D5灭
            lastBlinkTime = millis();
            blinkInterval = 250; // 交替闪烁间隔
            break;
        case Mode::SLOW_BLINK:
            setState(true, true); // 初始都亮
            lastBlinkTime = millis();
            blinkInterval = 1000; // 慢速闪烁间隔
            break;
        case Mode::FAST_BLINK:
            setState(true, true); // 初始都亮
            lastBlinkTime = millis();
            blinkInterval = 333; // 快速闪烁间隔
            break;
        case Mode::HEARTBEAT:
            setState(true, true); // 初始都亮
            lastBlinkTime = millis();
            blinkInterval = 500; // 心跳间隔
            break;
    }
}

LEDController::Mode LEDController::getMode() {
    return currentMode;
}

void LEDController::update() {
    if (!initialized) {
        init();
        return;
    }
    
    unsigned long currentTime = millis();
    
    switch (currentMode) {
        case Mode::OFF:
        case Mode::ON:
            // 静态模式，无需更新
            break;
            
        case Mode::SLOW_BLINK:
            if (currentTime - lastBlinkTime >= blinkInterval) {
                ledState = !ledState;
                setState(ledState, ledState);
                lastBlinkTime = currentTime;
            }
            break;
            
        case Mode::FAST_BLINK:
            if (currentTime - lastBlinkTime >= blinkInterval) {
                ledState = !ledState;
                setState(ledState, ledState);
                lastBlinkTime = currentTime;
            }
            break;
            
        case Mode::ALTERNATE:
            if (currentTime - lastBlinkTime >= blinkInterval) {
                ledState = !ledState;
                setState(ledState, !ledState); // 交替状态
                lastBlinkTime = currentTime;
            }
            break;
            
        case Mode::HEARTBEAT:
            if (currentTime - lastBlinkTime >= blinkInterval) {
                // 简单的心跳模式：快闪两次
                static int heartbeatStep = 0;
                if (heartbeatStep == 0) {
                    setState(false, false);
                } else if (heartbeatStep == 1) {
                    setState(true, true);
                } else if (heartbeatStep == 2) {
                    setState(false, false);
                } else {
                    setState(true, true);
                    heartbeatStep = -1; // 重置
                }
                heartbeatStep++;
                lastBlinkTime = currentTime;
            }
            break;
    }
}

void LEDController::turnOff() {
    setMode(Mode::OFF);
}

void LEDController::turnOn() {
    setMode(Mode::ON);
}

void LEDController::blinkSync(int intervalMs) {
    blinkInterval = intervalMs;
    setMode(Mode::SLOW_BLINK);
}

void LEDController::blinkAlternate(int intervalMs) {
    blinkInterval = intervalMs;
    setMode(Mode::ALTERNATE);
}