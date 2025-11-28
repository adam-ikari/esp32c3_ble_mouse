#include <Arduino.h>
#include <BleMouse.h>

// LED 引脚定义
#define LED_D4_PIN 12  // 高电平有效
#define LED_D5_PIN 13  // 高电平有效

// 按键引脚定义
#define BOOT_BUTTON_PIN 9  // BOOT 按键，低电平有效

// 函数声明
void enterPairingMode();
void handlePairingModeBlink();
void toggleMouseMotion();
void moveMouse();

// 状态变量
BleMouse bleMouse("ESP32 BLE Mouse", "Espressif", 100);
bool isMouseMoving = false;  // 默认关闭鼠标移动
bool isPairingMode = false;
unsigned long buttonPressStartTime = 0;
unsigned long lastBlinkTime = 0;
bool ledState = false;
int blinkCount = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 BLE Mouse 启动中...");
  
  // 初始化 LED 引脚
  pinMode(LED_D4_PIN, OUTPUT);
  pinMode(LED_D5_PIN, OUTPUT);
  digitalWrite(LED_D4_PIN, LOW);
  digitalWrite(LED_D5_PIN, LOW);
  
  // 初始化按键引脚
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  
  // 启动 BLE 鼠标，使用最低版本协议保证最大兼容性
  bleMouse.begin();
  
  Serial.println("初始化完成");
}

void loop() {
  // 检查按键状态
  bool buttonPressed = (digitalRead(BOOT_BUTTON_PIN) == LOW);
  
  if (buttonPressed) {
    if (buttonPressStartTime == 0) {
      // 按键刚按下
      buttonPressStartTime = millis();
      Serial.println("按键按下");
    } else {
      // 按键持续按下
      unsigned long pressDuration = millis() - buttonPressStartTime;
      
      // 长按 5 秒进入配对模式
      if (pressDuration >= 5000 && !isPairingMode) {
        enterPairingMode();
      }
    }
  } else {
    if (buttonPressStartTime > 0) {
      // 按键刚释放
      unsigned long pressDuration = millis() - buttonPressStartTime;
      
      // 短按切换鼠标移动
      if (pressDuration < 1000) {
        toggleMouseMotion();
      }
      
      buttonPressStartTime = 0;
    }
  }
  
  // 处理配对模式的 LED 闪烁
  if (isPairingMode) {
    handlePairingModeBlink();
  }
  
  // 处理鼠标移动
  if (isMouseMoving && bleMouse.isConnected()) {
    moveMouse();
  }
  
  delay(10);
}

void enterPairingMode() {
  isPairingMode = true;
  blinkCount = 0;
  lastBlinkTime = millis();
  Serial.println("进入蓝牙配对模式");
  
  // 重新开始 BLE 广播，增加延迟确保资源释放
  bleMouse.end();
  delay(1000);  // 增加延迟确保 BLE 资源完全释放
  bleMouse.begin();
}

void handlePairingModeBlink() {
  unsigned long currentTime = millis();
  
  // 每秒闪烁 3 次，即每 333ms 闪烁一次
  if (currentTime - lastBlinkTime >= 333) {
    ledState = !ledState;
    digitalWrite(LED_D4_PIN, ledState ? HIGH : LOW);
    lastBlinkTime = currentTime;
  }
  
  // 检查是否已连接，如果连接成功则退出配对模式
  if (bleMouse.isConnected()) {
    isPairingMode = false;
    digitalWrite(LED_D4_PIN, LOW);
    Serial.println("设备已连接，退出配对模式");
  }
}

void toggleMouseMotion() {
  isMouseMoving = !isMouseMoving;
  
  if (isMouseMoving) {
    digitalWrite(LED_D5_PIN, HIGH);
    Serial.println("鼠标移动已开启");
  } else {
    digitalWrite(LED_D5_PIN, LOW);
    Serial.println("鼠标移动已关闭");
  }
}

void moveMouse() {
  // 简单的圆形移动模式
  static float angle = 0;
  
  int8_t deltaX = (int8_t)(5 * cos(angle));
  int8_t deltaY = (int8_t)(5 * sin(angle));
  
  bleMouse.move(deltaX, deltaY, 0);
  
  angle += 0.1;
  if (angle >= 2 * PI) {
    angle = 0;
  }
  
  delay(50);
}