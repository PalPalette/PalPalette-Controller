#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "DeviceManager.h"
#include "../lighting/LightManager.h"
#include "../config.h"

using namespace websockets;

// Legacy struct for backward compatibility
struct LegacyColorPalette
{
    String messageId;
    String senderId;
    String senderName;
    unsigned long timestamp;
    String colors[10];
    int colorCount;
};

class WSClient
{
private:
    WebsocketsClient client;
    DeviceManager *deviceManager;
    LightManager *lightManager;
    String serverUrl;
    bool isConnected;
    unsigned long lastHeartbeat;
    unsigned long lastConnectionAttempt;
    ColorPalette currentPalette;

    // Message handlers
    void handleColorPalette(JsonDocument &doc);
    void handleDeviceRegistered(JsonDocument &doc);
    void handleDeviceClaimed(JsonDocument &doc);
    void handleSetupComplete(JsonDocument &doc);
    void handleLightingSystemConfig(JsonDocument &doc);
    void handleTestLightingSystem(JsonDocument &doc);
    void handleFactoryReset(JsonDocument &doc);

    // Connection management
    void onMessageCallback(WebsocketsMessage message);
    void onEventsCallback(WebsocketsEvent event, String data);

    // User notification handling
    void handleUserNotification(const String &action, const String &instructions, int timeout);

    // Status reporting
    void sendLightingSystemStatus();
    void sendDeviceStatus();

    // Utility functions
    void displayColorPaletteSerial();
    void displayColorPaletteOnLights();
    ColorPalette convertToLightPalette(const LegacyColorPalette &legacyPalette);

public:
    WSClient(DeviceManager *devManager, LightManager *lightMgr = nullptr);

    void begin(const String &url);
    bool connect();
    void disconnect();
    bool isClientConnected();
    void loop();
    void sendHeartbeat();
    bool registerDevice();
    void sendMessage(const String &message);

    // Light management
    void setLightManager(LightManager *lightMgr);

    // Manual lighting authentication retry (for when initial authentication fails)
    bool retryLightingAuthentication();

    // Status helpers
    bool shouldSendHeartbeat();
    bool shouldRetryConnection();
};

#endif
