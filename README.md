# ESP32 BLE 鼠标

本项目是基于 ESP32 开发的 BLE 鼠标。

## LED 控制

CORE ESP32 核心板带有 2 个 LED。开发者可以参考表 4-1 进行相应引脚的控制。

| LED 编号 | 对应 GPIO | 引脚功能    | 描述       |
| -------- | --------- | ----------- | ---------- |
| D4       | GPIO12    | GPIO12 配置 | 高电平有效 |
| D5       | GPIO13    | GPIO13 配置 | 高电平有效 |

## 按键介绍

CORE ESP32 核心板带有两个按键，其中 BOOT 按键可实现 BOOT 下载功能，RST 按键可实现复位功能，引脚控制参考表 4-2。

| 按键编号 | 对应 GPIO | 引脚功能                       | 描述       |
| -------- | --------- | ------------------------------ | ---------- |
| BOOT     | GPIO9     | 当按下按键时，芯片进入下载模式 | 低电平有效 |
| RST      |           | 当按下按键时，芯片复位         | 低电平有效 |

## 状态切换

```mermaid
stateDiagram-v2

[*] --> Init
Init --> Idle

state Init {
    [*] --> InitEntry
}

state Idle {
    [*] --> IdleEntry
}

state Reconnect {
    [*] --> ReconnectEntry
}

state Pairing {
    [*] --> PairingEntry
}

state Connected {
    [*] --> ConnectedEntry
    ConnectedEntry --> MouseMotionDisable
    MouseMotionDisable --> MouseMotionEnable: short press boot key
    MouseMotionEnable --> MouseMotionDisable: short press boot key
    
    state MouseMotionDisable {
        [*] --> MouseMotionDisableEntry
    }
    
    state MouseMotionEnable {
        [*] --> MouseMotionEnableEntry
    }
}

Idle --> Reconnect : has device connected before / init complete event
Idle --> Pairing: long press boot key (5s+)
Reconnect --> Connected : device connect success
Reconnect --> Reconnect : connection timeout / connection failed
Reconnect --> Pairing: long press boot key (5s+)
Pairing --> Connected : device connect success
Pairing --> Reconnect : timeout (60s) or connect failed
Connected --> Reconnect : device disconnect
Connected --> Pairing : long press boot key (5s+)
Connected --> MouseMotionDisable : init or disable mouse motion
MouseMotionDisable --> MouseMotionEnable : short press boot key
MouseMotionEnable --> MouseMotionDisable : short press boot key

note right of Init
    初始化LED和全局变量
    启动状态机
end note

note right of Idle
    设备可被发现和连接
    LED D4、D5 熄灭
    启动广播
end note

note right of Reconnect
    尝试连接已配对设备
    LED D4、D5 同步闪烁(1Hz)
    超时30秒后继续重连
end note

note right of Pairing
    设备处于配对模式
    LED D4、D5 同步闪烁(3Hz)
    超时60秒后返回重连状态
end note

note right of Connected
    设备已连接
    LED D4、D5 常亮
end note

note right of MouseMotionDisable
    鼠标移动功能禁用
    LED D4、D5 常亮
end note

note right of MouseMotionEnable
    鼠标移动功能启用
    基于随机动量的移动
    LED D4、D5 交替闪烁(2Hz)
end note
```

## 使用方法

### 启动

连接电源即启动，设备将先进入Init状态进行初始化，然后自动进入Reconnect状态尝试连接之前配对的设备，如果没有成功则进入Idle状态等待连接。

### 配对

长按 BOOT 按键超过 5 秒进入蓝牙配对模式，此模式下 LED D4 和 LED D5 同步每秒闪烁 3 次。

### 重新连接

启动和断开连接之后会自动尝试连接上次连接的蓝牙设备，处于重连状态时 LED D4 和 LED D5 同步每秒闪烁 1 次。

### 开关鼠标动作

短按 BOOT 开关鼠标动作。
开启鼠标动作时，设备模拟鼠标基于随机动量移动（随机方向和平滑过渡），此时 LED D4、D5 交替闪烁，每秒 2 次。
关闭鼠标动作时，LED D4、D5 常亮。

### 状态指示

- **Init状态**: LED熄灭，初始化系统
- **Idle状态**: LED熄灭，设备可被连接
- **Reconnect状态**: LED同步闪烁(1Hz)，尝试重连
- **Pairing状态**: LED同步闪烁(3Hz)，配对模式
- **Connected状态**: LED常亮，已连接但鼠标移动禁用
- **MouseMotionEnable状态**: LED交替闪烁(2Hz)，鼠标移动启用