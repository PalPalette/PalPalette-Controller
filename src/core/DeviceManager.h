#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include "../config.h"

struct DeviceInfo
{
    String deviceId;
    String macAddress;
    String pairingCode;
    bool isProvisioned;
    bool isOnline;
    String ipAddress;
    String firmwareVersion;
};

class DeviceManager
{
private:
    Preferences preferences;
    DeviceInfo deviceInfo;
    unsigned long lastStatusUpdate;

    void generateDeviceInfo();
    bool saveDeviceInfo();
    bool loadDeviceInfo();

public:
    DeviceManager();

    void begin();
    bool registerWithServer(const String &serverUrl);
    bool updateStatus(const String &serverUrl);
    void setProvisioned(bool provisioned);
    bool isProvisioned();
    String getDeviceId();
    String getMacAddress();
    String getPairingCode();
    DeviceInfo getDeviceInfo();
    void resetDevice();
    bool shouldUpdateStatus();
    void markStatusUpdated();

    // Status update helpers
    void setOnlineStatus(bool online);
    bool isOnline();
};

#endif
