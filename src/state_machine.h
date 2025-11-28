#pragma once

#include <tinyfsm.hpp>

// 事件定义
struct BootButtonShortPress : tinyfsm::Event {};
struct BootButtonLongPress : tinyfsm::Event {};
struct DeviceConnected : tinyfsm::Event {};
struct DeviceDisconnected : tinyfsm::Event {};
struct ConnectionTimeout : tinyfsm::Event {};
struct PairingTimeout : tinyfsm::Event {};
struct ConnectionFailed : tinyfsm::Event {};
struct InitComplete : tinyfsm::Event {};

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
    void react(BootButtonLongPress const &) override;
    void react(DeviceDisconnected const &) override;
};

class MouseMotionDisable : public BleMouseState {
public:
    void entry() override;
    void react(BootButtonShortPress const &) override;
};

class MouseMotionEnable : public BleMouseState {
private:
    static float angle;
public:
    void entry() override;
    void react(BootButtonShortPress const &) override;
    void moveMouse();
    void updateLED();
};

