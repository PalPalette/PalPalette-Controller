#ifndef WLED_CONTROLLER_H
#define WLED_CONTROLLER_H

#include "../LightController.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/**
 * WLED controller implementation
 *
 * This controller interfaces with WLED-powered LED strips using their JSON API.
 * WLED is a popular open-source firmware for ESP8266/ESP32 LED controllers.
 *
 * Features supported:
 * - JSON API control
 * - Segment-based color control
 * - Effects and transitions
 * - Brightness control
 * - Status monitoring
 */
class WLEDController : public LightController
{
private:
    HTTPClient http;
    String baseUrl;
    int ledCount;
    bool isConnected;

    // WLED-specific configuration
    struct
    {
        int segmentId = 0;      // Default segment to control
        int transitionTime = 7; // Transition time in tenths of seconds
        bool useMainSegment = true;
    } wledConfig;

public:
    WLEDController();
    virtual ~WLEDController();

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
    JsonObject getCapabilities() override;
    bool isReady() const override;

    // WLED-specific methods
    bool setSegmentColors(const ColorPalette &palette);
    bool setEffect(const String &effectName);
    bool getInfo();

private:
    bool sendWLEDCommand(const JsonDocument &command);
    JsonDocument createColorCommand(const ColorPalette &palette);
    bool sendHttpRequest(const String &endpoint, const String &method, const String &payload = "", JsonDocument *response = nullptr);
};

#endif // WLED_CONTROLLER_H
