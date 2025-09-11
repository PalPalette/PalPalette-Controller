#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WiFiAP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "../config.h"

class WiFiManager
{
private:
    Preferences preferences;
    AsyncWebServer *server;
    DNSServer *dnsServer;
    String savedSSID;
    String savedPassword;
    bool isAPMode;
    unsigned long apStartTime;

    void setupCaptivePortal();
    void handleRoot(AsyncWebServerRequest *request);
    void handleSave(AsyncWebServerRequest *request);
    void handleStatus(AsyncWebServerRequest *request);
    void handleReset(AsyncWebServerRequest *request);
    String getSetupPageHTML();

public:
    WiFiManager();
    ~WiFiManager();

    void begin();
    bool connectToWiFi();
    void startAPMode();
    void stopAPMode();
    bool isConnected();
    bool isInAPMode();
    void saveWiFiCredentials(const String &ssid, const String &password);
    void saveLightingConfig(const String &systemType, const String &hostAddress = "", int port = 0);
    void clearWiFiCredentials();
    String getSSID();
    String getLocalIP();
    String getMacAddress();
    void loop();
    bool hasStoredCredentials();
    void setServerURL(const String &url);
    String getServerURL();
};

#endif
