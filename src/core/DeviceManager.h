// Forward declaration to allow pointer usage
class LightManager;
struct LightConfig;
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include "../config.h"

// Forward declaration to allow pointer usage
class LightManager;
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

    void generateMinimalDeviceInfo();
    void generateDeviceInfo();
    bool saveDeviceInfo();
    bool loadDeviceInfo();
    String generateUUIDFromMAC(const String &macAddress);
    bool isValidLightingSystemType(const String &systemType);

public:
    DeviceManager();

    void begin();
    bool registerMinimalWithServer(const String &serverUrl);
    bool registerWithServer(const String &serverUrl);
    bool updateStatus(const String &serverUrl, LightManager *lightManager = nullptr);
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
