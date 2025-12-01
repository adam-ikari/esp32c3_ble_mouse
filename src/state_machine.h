#pragma once

#include <tinyfsm.hpp>

// 随机时间范围常量
const unsigned int MIN_MOVE_DURATION = 1000;   // 最小移动时间 1秒
const unsigned int MAX_MOVE_DURATION = 4000;   // 最大移动时间 4秒
const unsigned int MIN_PAUSE_DURATION = 500;   // 最小停顿时间 0.5秒
const unsigned int MAX_PAUSE_DURATION = 3000;  // 最大停顿时间 3秒

// 事件定义
struct BootButtonShortPress : tinyfsm::Event {};
struct BootButtonLongPress : tinyfsm::Event {};
struct DeviceConnected : tinyfsm::Event {};
struct DeviceDisconnected : tinyfsm::Event {};
struct ConnectionTimeout : tinyfsm::Event {};
struct PairingTimeout : tinyfsm::Event {};
struct ConnectionFailed : tinyfsm::Event {};
struct InitComplete : tinyfsm::Event {};
struct RestoreMouseMotionState : tinyfsm::Event {}; // 内部事件：恢复鼠标运动状态

// 基状态类
class BleMouseState : public tinyfsm::Fsm<BleMouseState> {
public:
    virtual void entry() {}
    virtual void exit() {}
    virtual void react(BootButtonShortPress const &) {}
    virtual void react(BootButtonLongPress const &) {}
    virtual void react(DeviceConnected const &) {}
    virtual void react(DeviceDisconnected const &) {}
    virtual void react(ConnectionTimeout const &) {}
    virtual void react(PairingTimeout const &) {}
    virtual void react(ConnectionFailed const &) {}
    virtual void react(InitComplete const &) {}
    virtual void react(RestoreMouseMotionState const &) {}
};

// 状态类定义
class Init : public BleMouseState {
public:
    void entry() override;
    void react(InitComplete const &) override;
};

class Idle : public BleMouseState {
public:
    void entry() override;
    void react(BootButtonLongPress const &) override;
    void react(BootButtonShortPress const &) override;
    void react(DeviceConnected const &) override;
    void react(DeviceDisconnected const &) override;
    void react(ConnectionTimeout const &) override;
    void react(PairingTimeout const &) override;
    void react(ConnectionFailed const &) override;
    void react(InitComplete const &) override;
};

class Reconnect : public BleMouseState {
private:
    unsigned long reconnectStartTime;
public:
    void entry() override;
    void react(DeviceConnected const &) override;
    void react(BootButtonLongPress const &) override;
    void react(ConnectionTimeout const &) override;
    void react(ConnectionFailed const &) override;
private:
    void startReconnection();
    void checkTimeout();
    static constexpr unsigned long RECONNECT_TIMEOUT = 30000; // 30秒超时
};

class Pairing : public BleMouseState {
private:
    unsigned long pairingStartTime;
public:
    void entry() override;
    void react(DeviceConnected const &) override;
    void react(DeviceDisconnected const &) override;
    void react(BootButtonLongPress const &) override;
    void react(PairingTimeout const &) override;
    void react(ConnectionFailed const &) override;
private:
    void startPairing();
    void checkTimeout();
    static constexpr unsigned long PAIRING_TIMEOUT = 60000; // 60秒超时
};

class Connected : public BleMouseState {
public:
    void entry() override;
    void react(BootButtonShortPress const &) override;
    void react(BootButtonLongPress const &) override;
    void react(DeviceConnected const &) override;
    void react(DeviceDisconnected const &) override;
    void react(ConnectionTimeout const &) override;
    void react(PairingTimeout const &) override;
    void react(ConnectionFailed const &) override;
    void react(InitComplete const &) override;
    void react(RestoreMouseMotionState const &) override;
};

class MouseMotionDisable : public BleMouseState {
public:
    void entry() override;
    void react(BootButtonShortPress const &) override;
    void react(BootButtonLongPress const &) override;
    void react(DeviceConnected const &) override;
    void react(DeviceDisconnected const &) override;
    void react(ConnectionTimeout const &) override;
    void react(PairingTimeout const &) override;
    void react(ConnectionFailed const &) override;
    void react(InitComplete const &) override;
};

class MouseMotionEnable : public BleMouseState {
private:
    static float angle;
public:
    void entry() override;
    void react(BootButtonShortPress const &) override;
    void react(BootButtonLongPress const &) override;
    void react(DeviceDisconnected const &) override;
};

