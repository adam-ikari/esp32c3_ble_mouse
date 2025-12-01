#include <Arduino.h>
#include <math.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLEHIDDevice.h>
#include "state_machine.h"
#include "../include/led_controller.h"

// 按键引脚定义
#define BOOT_BUTTON_PIN 9 // BOOT 按键，低电平有效

// HID 报告描述符 - 鼠标（包含滚轮和中键声明但不使用）
static const uint8_t hid_report_descriptor[] = {
    0x05, 0x01, // Usage Page (Generic Desktop)
    0x09, 0x02, // Usage (Mouse)
    0xA1, 0x01, // Collection (Application)
    0x09, 0x01, //   Usage (Pointer)
    0xA1, 0x00, //   Collection (Physical)
    0x05, 0x09, //     Usage Page (Buttons)
    0x19, 0x01, //     Usage Minimum (1)
    0x29, 0x03, //     Usage Maximum (3) - 左键、右键、中键
    0x15, 0x00, //     Logical Minimum (0)
    0x25, 0x01, //     Logical Maximum (1)
    0x95, 0x03, //     Report Count (3)
    0x75, 0x01, //     Report Size (1)
    0x81, 0x02, //     Input (Data, Variable, Absolute)
    0x95, 0x01, //     Report Count (1)
    0x75, 0x05, //     Report Size (5)
    0x81, 0x03, //     Input (Constant) or Padding
    0x05, 0x01, //     Usage Page (Generic Desktop)
    0x09, 0x30, //     Usage (X)
    0x09, 0x31, //     Usage (Y)
    0x09, 0x38, //     Usage (Wheel) - 声明滚轮但不使用
    0x15, 0x81, //     Logical Minimum (-127)
    0x25, 0x7F, //     Logical Maximum (127)
    0x75, 0x08, //     Report Size (8)
    0x95, 0x03, //     Report Count (3) - X、Y、Wheel
    0x81, 0x06, //     Input (Data, Variable, Relative)
    0xC0,       //   End Collection
    0xC0        // End Collection
};

// 全局变量
NimBLEServer *pServer = nullptr;
NimBLEHIDDevice *hid = nullptr;
NimBLECharacteristic *inputMouse = nullptr;
bool deviceConnected = false;
unsigned long buttonPressStartTime = 0;

// 鼠标移动状态变量 - 模拟人类自然移动
float currentVelocityX = 0.0;
float currentVelocityY = 0.0;
float targetVelocityX = 0.0;
float targetVelocityY = 0.0;
float moveAngle = 0.0;
float moveRadius = 0.0;
unsigned long lastMoveUpdate = 0;
unsigned long patternChangeTimer = 0;
unsigned int patternChangeInterval = 3000; // 每3秒改变移动模式
int currentPattern = 0;                    // 0=随机漫步, 1=圆形轨迹, 2=8字形轨迹

// 移动和停顿控制
bool isMoving = false;
unsigned long movePhaseTimer = 0;
unsigned long pausePhaseTimer = 0;
unsigned int moveDuration = 0; // 移动持续时间，随机设置
unsigned int pauseDuration = 0; // 停顿持续时间，随机设置
bool inMovePhase = true; // true=移动阶段, false=停顿阶段

// 安卓拖动问题修复
bool lastWasMoving = false;
unsigned long lastReleaseReportTime = 0;
const unsigned long RELEASE_REPORT_INTERVAL = 100; // 每100ms发送一次释放报告

// LED 控制变量
unsigned long lastBlinkTime = 0;
bool ledState = false;
int blinkCount = 0;

// 鼠标运动状态记忆
bool rememberedMouseMotionState = false; // false=禁用, true=启用

// 回调类：连接状态改变
class ServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer)
    {
        deviceConnected = true;
        Serial.println("BLE设备已连接");
        Serial.println("客户端数量: " + String(pServer->getConnectedCount()));
        Serial.println("尝试发送DeviceConnected事件到状态机");
        BleMouseState::dispatch(DeviceConnected());
        Serial.println("DeviceConnected事件已发送");
    }

    void onDisconnect(NimBLEServer *pServer)
    {
        deviceConnected = false;
        Serial.println("BLE设备已断开连接");
        BleMouseState::dispatch(DeviceDisconnected());
        // 重新开始广播
        NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
        pAdvertising->start();
    }
};

void setup()
{
    Serial.begin(115200);
    Serial.println("启动中...");

    // 初始化LED控制器
    LEDController::init();
    LEDController::setMode(LEDController::Mode::OFF);
    Serial.println("LED控制器已初始化");

    // 初始化随机数生成器
    randomSeed(analogRead(0) + millis());
    Serial.println("随机数生成器已初始化");

    // 初始化按键引脚
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    // 初始化 BLE
    NimBLEDevice::init("Magic Mouse");
    // 设置BLE安全参数 用于HID设备
    NimBLEDevice::setSecurityAuth(true, true, true);

    // 创建 BLE 服务器
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // 创建 HID 设备
    hid = new NimBLEHIDDevice(pServer);

    // 设置制造商信息
    hid->setManufacturer("Huawei");
    hid->setPnp(0x02, 0x05C3, 0x0001, 0x0001);
    hid->setHidInfo(0x00, 0x01);

    // 设置鼠标输入特征
    inputMouse = hid->getInputReport(1); // Report ID 1 for Mouse
    // 设置输入报告回调，以便接收来自客户端的报告

    // 设置 HID 报告描述符
    hid->setReportMap((uint8_t *)hid_report_descriptor, sizeof(hid_report_descriptor));

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
    pAdvertising->setName("Magic Mouse");
    pAdvertising->start();
    Serial.println("广播已启动");
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

void loop()
{
    // 检查按键状态
    bool buttonPressed = (digitalRead(BOOT_BUTTON_PIN) == LOW);

    if (buttonPressed)
    {
        if (buttonPressStartTime == 0)
        {
            // 按键刚按下
            buttonPressStartTime = millis();
        }
        else
        {
            // 按键持续按下
            unsigned long pressDuration = millis() - buttonPressStartTime;

            // 长按 5 秒进入配对模式
            if (pressDuration >= 3000)
            {
                BleMouseState::dispatch(BootButtonLongPress());
                // 等待按键释放以避免重复触发
                while (digitalRead(BOOT_BUTTON_PIN) == LOW)
                {
                    delay(10);
                }
                buttonPressStartTime = 0;
            }
        }
    }
    else
    {
        if (buttonPressStartTime > 0)
        {
            // 按键刚释放
            unsigned long pressDuration = millis() - buttonPressStartTime;

            // 短按切换鼠标移动
            if (pressDuration < 1000)
            {
                BleMouseState::dispatch(BootButtonShortPress());
            }

            buttonPressStartTime = 0;
        }
    }

    // 定期检查连接状态并手动触发状态转换（如果回调未被触发）
    static unsigned long lastConnectionCheck = 0;
    if (millis() - lastConnectionCheck > 1000)
    { // 每秒检查一次
        int connectedCount = pServer ? pServer->getConnectedCount() : 0;
        if (connectedCount > 0 && !deviceConnected)
        {
            // 检测到连接但状态未更新，手动触发连接事件
            Serial.println("检测到连接但回调未触发，手动触发DeviceConnected事件");
            Serial.println("当前连接数: " + String(connectedCount));
            deviceConnected = true;
            BleMouseState::dispatch(DeviceConnected());
        }
        else if (connectedCount == 0 && deviceConnected)
        {
            // 检测到断开连接但状态未更新
            Serial.println("检测到断开连接，手动触发DeviceDisconnected事件");
            deviceConnected = false;
            BleMouseState::dispatch(DeviceDisconnected());
        }
        lastConnectionCheck = millis();
    }

    // 处理配对模式的 LED 闪烁
    if (BleMouseState::is_in_state<Pairing>())
    {
        unsigned long currentTime = millis();
        // 每秒闪烁 3 次，即每 333ms 闪烁一次
        if (currentTime - lastBlinkTime >= 333)
        {
            ledState = !ledState;
            digitalWrite(12, ledState ? HIGH : LOW); // LED_D4_PIN
            digitalWrite(13, ledState ? HIGH : LOW); // LED_D5_PIN
            lastBlinkTime = currentTime;
        }
    }

    // 处理重连状态的 LED 闪烁
    if (BleMouseState::is_in_state<Reconnect>())
    {
        unsigned long currentTime = millis();
        // 每秒闪烁 1 次，即每 1000ms 闪烁一次
        if (currentTime - lastBlinkTime >= 1000)
        {
            ledState = !ledState;
            digitalWrite(12, ledState ? HIGH : LOW); // LED_D4_PIN
            digitalWrite(13, ledState ? HIGH : LOW); // LED_D5_PIN
            lastBlinkTime = currentTime;
        }
    }

    // 处理鼠标移动状态 - 模拟人类自然移动
    if (BleMouseState::is_in_state<MouseMotionEnable>())
    {
        unsigned long currentTime = millis();
        float deltaTime = (currentTime - lastMoveUpdate) / 1000.0; // 转换为秒
        lastMoveUpdate = currentTime;

        // 管理移动和停顿周期
        if (inMovePhase)
        {
            // 移动阶段
            if (currentTime - movePhaseTimer > moveDuration)
            {
                // 切换到停顿阶段
                inMovePhase = false;
                pausePhaseTimer = currentTime;
                // 随机设置停顿时间
                pauseDuration = random(MIN_PAUSE_DURATION, MAX_PAUSE_DURATION);
                Serial.println("切换到停顿阶段，停顿时长: " + String(pauseDuration) + "ms");

                // 停止移动
                currentVelocityX = 0.0;
                currentVelocityY = 0.0;
                targetVelocityX = 0.0;
                targetVelocityY = 0.0;
            }
        }
        else
        {
            // 停顿阶段
            if (currentTime - pausePhaseTimer > pauseDuration)
            {
                // 切换到移动阶段
                inMovePhase = true;
                movePhaseTimer = currentTime;
                // 随机设置移动时间
                moveDuration = random(MIN_MOVE_DURATION, MAX_MOVE_DURATION);
                Serial.println("切换到移动阶段，移动时长: " + String(moveDuration) + "ms");

                // 可能改变移动模式
                if (random(0, 100) < 30)
                { // 30%概率改变模式
                    currentPattern = random(0, 3);
                    moveRadius = random(5.0, 15.0); // 增大随机移动幅度
                    Serial.println("改变移动模式: " + String(currentPattern) + ", 幅度: " + String(moveRadius));
                }
            }
        }

        // 只在移动阶段计算和执行移动
        if (inMovePhase)
        {
            // 每隔一段时间改变移动模式
            if (currentTime - patternChangeTimer > patternChangeInterval)
            {
                currentPattern = random(0, 3); // 随机选择移动模式
                patternChangeTimer = currentTime;
                moveRadius = random(5.0, 15.0); // 增大随机移动幅度
                Serial.println("切换到移动模式: " + String(currentPattern) + ", 幅度: " + String(moveRadius));
            }

            // 根据当前模式计算目标速度
            float randomSpeed;
            switch (currentPattern)
            {
            case 0:                             // 随机漫步模式
                moveAngle += random(-0.3, 0.3); // 随机转向
                randomSpeed = moveRadius * (0.5 + 0.5 * sin(currentTime * 0.001));
                targetVelocityX = randomSpeed * cos(moveAngle);
                targetVelocityY = randomSpeed * sin(moveAngle);
                break;

            case 1:                // 圆形轨迹模式
                moveAngle += 0.05; // 缓慢旋转
                targetVelocityX = moveRadius * cos(moveAngle);
                targetVelocityY = moveRadius * sin(moveAngle);
                break;

            case 2: // 8字形轨迹模式
                moveAngle += 0.03;
                targetVelocityX = moveRadius * sin(moveAngle);
                targetVelocityY = moveRadius * sin(moveAngle * 2) * 0.5;
                break;
            }

            // 添加微小的随机扰动，模拟手部微小抖动
            targetVelocityX += random(-100, 100) / 1000.0;
            targetVelocityY += random(-100, 100) / 1000.0;

            // 平滑过渡到目标速度（模拟人体动作的惯性）
            float smoothFactor = 0.1;
            currentVelocityX += (targetVelocityX - currentVelocityX) * smoothFactor;
            currentVelocityY += (targetVelocityY - currentVelocityY) * smoothFactor;

            // 限制最大速度
            float maxSpeed = 20.0; // 增大最大速度限制
            float currentSpeed = sqrt(currentVelocityX * currentVelocityX + currentVelocityY * currentVelocityY);
            if (currentSpeed > maxSpeed)
            {
                currentVelocityX = (currentVelocityX / currentSpeed) * maxSpeed;
                currentVelocityY = (currentVelocityY / currentSpeed) * maxSpeed;
            }
        }
        else
        {
            // 停顿阶段，逐渐减速到0
            currentVelocityX *= 0.9;
            currentVelocityY *= 0.9;
            targetVelocityX = 0.0;
            targetVelocityY = 0.0;
        }

        // 转换为整数移动值
        int8_t moveX = (int8_t)constrain(currentVelocityX, -127, 127);
        int8_t moveY = (int8_t)constrain(currentVelocityY, -127, 127);

        // 始终发送鼠标报告，确保状态正确（避免安卓拖动问题）
        uint8_t buttons = 0;                                                   // 明确设置无点击状态（包括中键）
        uint8_t mouseReport[4] = {buttons, (uint8_t)moveX, (uint8_t)moveY, 0}; // 滚轮始终为0

        // 检测是否从移动状态变为静止状态
        bool currentlyMoving = (abs(moveX) > 0 || abs(moveY) > 0);
        if (lastWasMoving && !currentlyMoving) {
            // 刚停止移动，立即发送释放报告
            uint8_t releaseReport[4] = {0, 0, 0, 0}; // 完全释放状态
            if (inputMouse && deviceConnected) {
                inputMouse->setValue(releaseReport, sizeof(releaseReport));
                inputMouse->notify();
                lastReleaseReportTime = currentTime;
            }
        }
        
        // 在停顿阶段定期发送释放报告（安卓兼容性）
        if (!currentlyMoving && (currentTime - lastReleaseReportTime > RELEASE_REPORT_INTERVAL)) {
            uint8_t releaseReport[4] = {0, 0, 0, 0}; // 完全释放状态
            if (inputMouse && deviceConnected) {
                inputMouse->setValue(releaseReport, sizeof(releaseReport));
                inputMouse->notify();
                lastReleaseReportTime = currentTime;
            }
        }
        
        // 正常发送移动报告
        if (inputMouse && deviceConnected)
        {
            inputMouse->setValue(mouseReport, sizeof(mouseReport));
            inputMouse->notify();
        }
        
        lastWasMoving = currentlyMoving;

        // LED D4、D5 交替闪烁，每秒2次
        if (currentTime - lastBlinkTime >= 250)
        { // 每250ms切换一次
            ledState = !ledState;
            digitalWrite(12, ledState ? HIGH : LOW); // LED_D4_PIN
            digitalWrite(13, ledState ? LOW : HIGH); // LED_D5_PIN
            lastBlinkTime = currentTime;
        }
    }

    delay(10);
}