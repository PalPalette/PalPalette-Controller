#include "WiFiManager.h"
#include <ArduinoJson.h>

WiFiManager::WiFiManager() : server(nullptr), dnsServer(nullptr), isAPMode(false), apStartTime(0)
{
}

WiFiManager::~WiFiManager()
{
    if (server)
    {
        delete server;
    }
    if (dnsServer)
    {
        delete dnsServer;
    }
}

void WiFiManager::begin()
{
    preferences.begin(DEVICE_PREF_NAMESPACE, false);

    // Load saved credentials
    savedSSID = preferences.getString(PREF_WIFI_SSID, "");
    savedPassword = preferences.getString(PREF_WIFI_PASSWORD, "");

    Serial.println("📶 WiFiManager initialized");
    if (savedSSID.length() > 0)
    {
        Serial.println("📝 Found saved WiFi credentials for: " + savedSSID);
    }
    else
    {
        Serial.println("📝 No saved WiFi credentials found");
    }
}

bool WiFiManager::connectToWiFi()
{
    if (savedSSID.length() == 0)
    {
        Serial.println("❌ No WiFi credentials available");
        return false;
    }

    Serial.println("📶 Attempting to connect to WiFi: " + savedSSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("✅ WiFi connected successfully!");
        Serial.println("📍 IP Address: " + WiFi.localIP().toString());
        Serial.println("📡 Signal Strength: " + String(WiFi.RSSI()) + " dBm");
        return true;
    }
    else
    {
        Serial.println("❌ WiFi connection failed");
        return false;
    }
}

void WiFiManager::startAPMode()
{
    if (isAPMode)
    {
        Serial.println("⚠ Already in AP mode");
        return;
    }

    Serial.println("🔄 Starting Access Point mode...");

    // Create AP SSID with MAC address suffix for uniqueness
    String macAddr = WiFi.macAddress();
    macAddr.replace(":", "");
    String apSSID = String(DEFAULT_AP_SSID) + "-" + macAddr.substring(6);

    WiFi.mode(WIFI_AP);
    bool apStarted = WiFi.softAP(apSSID.c_str(), DEFAULT_AP_PASSWORD);

    if (apStarted)
    {
        Serial.println("✅ Access Point started successfully!");
        Serial.println("📶 AP SSID: " + apSSID);
        Serial.println("🔐 AP Password: " + String(DEFAULT_AP_PASSWORD));
        Serial.println("📍 AP IP: " + WiFi.softAPIP().toString());

        setupCaptivePortal();
        isAPMode = true;
        apStartTime = millis();
    }
    else
    {
        Serial.println("❌ Failed to start Access Point");
    }
}

void WiFiManager::stopAPMode()
{
    if (!isAPMode)
    {
        return;
    }

    Serial.println("🔄 Stopping Access Point mode...");

    // Safely cleanup web server
    if (server != nullptr)
    {
        server->end();
        delete server;
        server = nullptr;
    }

    // Safely cleanup DNS server
    if (dnsServer != nullptr)
    {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }

    // Disconnect AP and cleanup WiFi state
    WiFi.softAPdisconnect(true);
    isAPMode = false;
    apStartTime = 0;

    Serial.println("✅ Access Point stopped and resources cleaned up");
}

void WiFiManager::setupCaptivePortal()
{
    // Ensure cleanup of existing servers before creating new ones
    if (server != nullptr)
    {
        server->end();
        delete server;
        server = nullptr;
    }

    if (dnsServer != nullptr)
    {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }

    // Create new server instances
    server = new AsyncWebServer(80);
    dnsServer = new DNSServer();

    // Verify allocation succeeded
    if (server == nullptr || dnsServer == nullptr)
    {
        Serial.println("❌ Failed to allocate memory for web server components");
        // Cleanup any successful allocation
        if (server != nullptr)
        {
            delete server;
            server = nullptr;
        }
        if (dnsServer != nullptr)
        {
            delete dnsServer;
            dnsServer = nullptr;
        }
        return;
    }

    // Start DNS server for captive portal with error checking
    if (!dnsServer->start(53, "*", WiFi.softAPIP()))
    {
        Serial.println("❌ Failed to start DNS server for captive portal");
        // Cleanup on failure
        delete server;
        server = nullptr;
        delete dnsServer;
        dnsServer = nullptr;
        return;
    }

    // Setup web server routes
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleRoot(request); });

    server->on("/save", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleSave(request); });

    server->on("/status", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleStatus(request); });

    server->on("/reset", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleReset(request); });

    // Captive portal - redirect all requests to setup page
    server->onNotFound([this](AsyncWebServerRequest *request)
                       { handleRoot(request); });

    server->begin();
    Serial.println("✅ Captive portal web server started successfully");
}

void WiFiManager::handleRoot(AsyncWebServerRequest *request)
{
    request->send(200, "text/html", getSetupPageHTML());
}

void WiFiManager::handleSave(AsyncWebServerRequest *request)
{
    String ssid = "";
    String password = "";
    String serverUrl = "";

    Serial.println("🔍 DEBUG: Processing captive portal form submission...");

    if (request->hasParam("ssid", true))
    {
        ssid = request->getParam("ssid", true)->value();
        Serial.println("  - SSID: '" + ssid + "'");
    }
    if (request->hasParam("password", true))
    {
        password = request->getParam("password", true)->value();
        Serial.println("  - Password: [hidden]");
    }
    if (request->hasParam("server", true))
    {
        serverUrl = request->getParam("server", true)->value();
        Serial.println("  - Server URL: '" + serverUrl + "'");
    }

    if (ssid.length() > 0)
    {
        saveWiFiCredentials(ssid, password);
        if (serverUrl.length() > 0)
        {
            setServerURL(serverUrl);
        }

        request->send(200, "text/html",
                      "<html><body><h1>Settings Saved!</h1>"
                      "<p>Device will restart and connect to WiFi.</p>"
                      "<p>Configure your lighting system through the PalPalette mobile app after pairing.</p>"
                      "<p>You can close this window.</p></body></html>");

        delay(2000);
        ESP.restart();
    }
    else
    {
        request->send(400, "text/html",
                      "<html><body><h1>Error</h1>"
                      "<p>SSID is required!</p>"
                      "<a href='/'>Go Back</a></body></html>");
    }
}

void WiFiManager::handleStatus(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    doc["deviceId"] = preferences.getString(PREF_DEVICE_ID, "Not set");
    doc["macAddress"] = WiFi.macAddress();
    doc["firmwareVersion"] = FIRMWARE_VERSION;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis();
    doc["isProvisioned"] = preferences.getBool(PREF_IS_PROVISIONED, false);

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WiFiManager::handleReset(AsyncWebServerRequest *request)
{
    clearWiFiCredentials();
    request->send(200, "text/html",
                  "<html><body><h1>Device Reset</h1>"
                  "<p>All settings cleared. Device will restart.</p></body></html>");

    delay(2000);
    ESP.restart();
}

String WiFiManager::getSetupPageHTML()
{
    String serverUrl = getServerURL();

    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>PalPalette Setup</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
    html += "h1 { color: #333; text-align: center; margin-bottom: 30px; }";
    html += ".form-group { margin-bottom: 20px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; color: #555; }";
    html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }";
    html += "button { background: #007bff; color: white; padding: 12px 30px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; width: 100%; }";
    html += "button:hover { background: #0056b3; }";
    html += ".info { background: #e9ecef; padding: 15px; border-radius: 5px; margin-bottom: 20px; }";
    html += ".scan-btn { margin-top: 5px; padding: 5px 10px; font-size: 12px; width: auto; }";
    html += "</style>";
    html += "<script>";
    html += "function selectNetwork(ssid) { document.getElementById('ssid').value = ssid; }";
    html += "function scanNetworks() { alert('Network scan feature would be implemented here'); }";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<h1>PalPalette Device Setup</h1>";
    html += "<div class='info'>";
    html += "<strong>Device Information:</strong><br>";
    html += "MAC Address: " + WiFi.macAddress() + "<br>";
    html += "Firmware: " + String(FIRMWARE_VERSION);
    html += "</div>";
    html += "<form action='/save' method='post'>";
    html += "<div class='form-group'>";
    html += "<label for='ssid'>WiFi Network Name (SSID):</label>";
    html += "<input type='text' id='ssid' name='ssid' required placeholder='Enter your WiFi network name'>";
    html += "<button type='button' onclick='scanNetworks()' class='scan-btn'>Scan Networks</button>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='password'>WiFi Password:</label>";
    html += "<input type='password' id='password' name='password' placeholder='Enter your WiFi password (leave blank if none)'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='server'>Server URL (optional):</label>";
    html += "<input type='text' id='server' name='server' value='" + serverUrl + "' placeholder='ws://your-server.com:3001'>";
    html += "<small style='color: #666;'>Default server will be used if left blank</small>";
    html += "</div>";
    html += "<div style='background: #e9f4ff; padding: 15px; border-radius: 5px; margin-bottom: 20px;'>";
    html += "<strong>💡 Lighting System Configuration</strong><br>";
    html += "Your lighting system will be configured through the PalPalette mobile app after this device is paired. ";
    html += "Supported systems: WS2812 LED strips, WLED controllers, and Nanoleaf panels.";
    html += "</div>";
    html += "<button type='submit'>Save Settings & Connect</button>";
    html += "</form>";
    html += "<div style='margin-top: 30px; text-align: center;'>";
    html += "<a href='/status' style='color: #007bff; text-decoration: none;'>Device Status</a> | ";
    html += "<a href='/reset' onclick='return confirm(\"This will reset all settings. Continue?\")' style='color: #dc3545; text-decoration: none;'>Reset Device</a>";
    html += "</div>";
    html += "</div>";
    html += "</body></html>";

    return html;
}

bool WiFiManager::isConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::isInAPMode()
{
    return isAPMode;
}

void WiFiManager::saveWiFiCredentials(const String &ssid, const String &password)
{
    preferences.putString(PREF_WIFI_SSID, ssid);
    preferences.putString(PREF_WIFI_PASSWORD, password);
    savedSSID = ssid;
    savedPassword = password;

    Serial.println("💾 WiFi credentials saved for: " + ssid);
}

void WiFiManager::saveLightingConfig(const String &systemType, const String &hostAddress, int port)
{
    preferences.putString("lighting_system", systemType);

    if (hostAddress.length() > 0)
    {
        preferences.putString("lighting_host", hostAddress);
    }
    else
    {
        preferences.remove("lighting_host");
    }

    if (port > 0)
    {
        preferences.putInt("lighting_port", port);
    }
    else
    {
        preferences.remove("lighting_port");
    }

    Serial.println("💡 Lighting configuration saved: " + systemType);
    if (hostAddress.length() > 0)
    {
        Serial.println("🌐 Host: " + hostAddress + (port > 0 ? ":" + String(port) : ""));
    }
}

void WiFiManager::clearWiFiCredentials()
{
    preferences.remove(PREF_WIFI_SSID);
    preferences.remove(PREF_WIFI_PASSWORD);
    preferences.remove(PREF_SERVER_URL);
    preferences.remove(PREF_DEVICE_ID);
    preferences.remove(PREF_IS_PROVISIONED);

    savedSSID = "";
    savedPassword = "";

    Serial.println("🗑 WiFi credentials and device settings cleared");
}

String WiFiManager::getSSID()
{
    return savedSSID;
}

String WiFiManager::getLocalIP()
{
    if (isConnected())
    {
        return WiFi.localIP().toString();
    }
    else if (isAPMode)
    {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

String WiFiManager::getMacAddress()
{
    return WiFi.macAddress();
}

void WiFiManager::loop()
{
    if (isAPMode && dnsServer != nullptr)
    {
        dnsServer->processNextRequest();

        // Check for AP timeout with proper error handling
        if (millis() - apStartTime > CAPTIVE_PORTAL_TIMEOUT)
        {
            Serial.println("⏰ Captive portal timeout reached, cleaning up and restarting...");

            // Proper cleanup before restart
            stopAPMode();
            delay(1000); // Give time for cleanup
            ESP.restart();
        }
    }
}

bool WiFiManager::hasStoredCredentials()
{
    return savedSSID.length() > 0;
}

void WiFiManager::setServerURL(const String &url)
{
    preferences.putString(PREF_SERVER_URL, url);
    Serial.println("💾 Server URL saved: " + url);
}

String WiFiManager::getServerURL()
{
    return preferences.getString(PREF_SERVER_URL, DEFAULT_SERVER_URL);
}

bool WiFiManager::isCaptivePortalHealthy()
{
    return isAPMode && (server != nullptr) && (dnsServer != nullptr);
}
