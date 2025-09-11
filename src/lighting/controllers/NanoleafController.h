#ifndef NANOLEAF_CONTROLLER_H
#define NANOLEAF_CONTROLLER_H

#include "../LightController.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <algorithm>

/**
 * Nanoleaf Aurora/Canvas/Shapes controller implementation
 *
 * This controller interfaces with Nanoleaf panels using their REST API.
 * It supports:
 * - Automatic panel discovery via mDNS
 * - Authentication token management
 * - Color palette display with smooth transitions
 * - Panel-specific animations
 * - Brightness control
 * - Status monitoring
 */
class NanoleafController : public LightController
{
private:
    HTTPClient http;
    String baseUrl;
    String authToken;
    int panelCount;
    bool isConnected;
    unsigned long lastHeartbeat;

    // Nanoleaf-specific configuration
    struct
    {
        int transitionTime = 10; // Transition time in tenths of seconds
        bool enableExternalControl = true;
        String defaultAnimation = "fade";
        int defaultBrightness = 100;
    } nanoleafConfig;

    // Panel information
    struct PanelInfo
    {
        int panelId;
        int x, y, o;   // Position and orientation
        int shapeType; // Shape type (12 = controller, should be excluded)
    };

    PanelInfo panels[50]; // Support up to 50 panels

    // Discovery results storage
    struct DiscoveredDevice
    {
        String hostname;
        String ipAddress;
        uint16_t port;
        bool isResponding;
    };

    DiscoveredDevice discoveredDevices[10]; // Support up to 10 discovered devices
    int discoveredDeviceCount;

public:
    // HSB color structure for Nanoleaf API
    struct HSBColor
    {
        int h; // Hue: 0-360
        int s; // Saturation: 0-100
        int b; // Brightness: 0-100
    };

    // Helper function to convert RGB to HSB
    HSBColor rgbToHsb(const RGBColor &rgb);

    NanoleafController();
    virtual ~NanoleafController();

    // Implement LightController interface
    bool initialize(const LightConfig &config) override;
    bool testConnection() override;
    bool displayPalette(const ColorPalette &palette) override;
    bool turnOff() override;
    bool setBrightness(int brightness) override;
    String getStatus() override;
    String getSystemType() override;
    bool authenticate() override;
    bool requiresAuthentication() override;
    LightConfig getUpdatedConfig() override;
    JsonObject getCapabilities() override;
    bool isReady() const override;

    // Nanoleaf-specific methods
    bool discoverNanoleaf();
    bool discoverNanoleaf(int deviceIndex); // Select specific device from discovery
    bool requestAuthToken();
    bool getPanelLayout();
    bool setStaticColors(const ColorPalette &palette);
    bool setAnimatedColors(const ColorPalette &palette, const String &animationType);
    bool enableExternalControl();
    bool disableExternalControl();
    void showConnectionSuccess(); // Visual feedback for successful connection

    // Discovery helpers
    int getDiscoveredDeviceCount();
    String getDiscoveredDeviceInfo(int index);

    // User interaction notification methods
    void setNotificationCallback(std::function<void(const String &, const String &, int)> callback);

    // Nanoleaf animation types
    enum AnimationType
    {
        STATIC,
        FADE,
        WHEEL,
        FLOW,
        CUSTOM
    };

private:
    bool sendHttpRequest(const String &endpoint, const String &method, const String &payload = "", JsonDocument *response = nullptr);
    String createColorAnimationData(const ColorPalette &palette, AnimationType animation);
    String createStaticColorData(const ColorPalette &palette);
    bool validateAuthToken();
    String rgbToHsl(const RGBColor &color);
    RGBColor hslToRgb(float h, float s, float l);
    void distributeColorsAcrossPanels(const ColorPalette &palette, JsonArray &panelColors);

    // User notification methods
    void notifyUserActionRequired();
    void notifyUserActionProgress(int remainingSeconds);
    void notifyUserActionCompleted(bool success);

    // Callback for user notifications (set by LightManager or main app)
    std::function<void(const String &, const String &, int)> notificationCallback;
};

#endif // NANOLEAF_CONTROLLER_H
