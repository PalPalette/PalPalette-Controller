#include "../lighting/LightManager.h"
#include "DeviceManager.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

DeviceManager::DeviceManager() : lastStatusUpdate(0)
{
}

String DeviceManager::generateUUIDFromMAC(const String &macAddress)
{
    // Create a deterministic pseudo-UUID v4 from MAC address
    // This is technically non-compliant with RFC 4122 for v4, but follows the v4 format.

    // 1. Clean the MAC address (should result in 12 hex characters)
    String cleanMac = macAddress;
    cleanMac.replace(":", "");
    cleanMac.toLowerCase();

    // 2. Setup characters and format string
    const char *hexChars = "0123456789abcdef";
    const char *yChars = "89ab"; // Valid Y values for UUID v4 variant
    String uuid = "";

    // 3. Loop through 32 hex character positions (i = 0 to 31)
    for (int i = 0; i < 32; i++)
    {
        // Add the hyphens *before* the character logic
        if (i == 8 || i == 12 || i == 16 || i == 20)
        {
            uuid += "-";
        }

        // --- Standard UUID v4 Indicators ---

        // Position 12 (1st nibble of 3rd group) must be '4' (Version)
        if (i == 12)
        {
            uuid += "4";
            continue; // Skip generation logic for this position
        }

        // Position 16 (1st nibble of 4th group) must be '8', '9', 'a', or 'b' (Variant)
        if (i == 16)
        {
            // Use deterministic Y value based on MAC, just like your original logic
            int macIndex = (i / 4) % cleanMac.length();
            char macChar = cleanMac[macIndex];
            int value = (macChar >= '0' && macChar <= '9') ? (macChar - '0') : (macChar - 'a' + 10);
            uuid += yChars[value % 4];
            continue; // Skip generation logic for this position
        }

        // --- Deterministic Generation Logic (Original Code) ---

        // Use MAC characters cyclically for deterministic generation
        int macIndex = i % cleanMac.length();

        // Ensure macIndex is valid and cleanMac is not empty
        if (cleanMac.isEmpty())
        {
            // Fallback if MAC address is empty or invalid
            uuid += hexChars[(i * 7) % 16];
            continue;
        }

        char macChar = cleanMac[macIndex];

        // Transform MAC character deterministically
        if (macChar >= '0' && macChar <= '9')
        {
            int value = (macChar - '0' + i) % 16;
            uuid += hexChars[value];
        }
        else if (macChar >= 'a' && macChar <= 'f')
        {
            int value = (macChar - 'a' + 10 + i) % 16;
            uuid += hexChars[value];
        }
        else
        {
            // Fallback for any unexpected characters
            int value = (i * 7) % 16;
            uuid += hexChars[value];
        }
    }

    return uuid;
}

bool DeviceManager::isValidLightingSystemType(const String &systemType)
{
    // Validate against backend allowed enum values
    return (systemType == "nanoleaf" ||
            systemType == "wled" ||
            systemType == "ws2812" ||
            systemType == "philips_hue");
}

void DeviceManager::begin()
{
    preferences.begin(DEVICE_PREF_NAMESPACE, false);

    // Load existing device info or generate minimal new info
    if (!loadDeviceInfo())
    {
        generateMinimalDeviceInfo();
        saveDeviceInfo();
    }

    Serial.println("📱 DeviceManager initialized");
    Serial.println("🆔 Device ID: " + deviceInfo.deviceId);
    Serial.println("📡 MAC Address: " + deviceInfo.macAddress);
    Serial.println("🔧 Firmware Version: " + deviceInfo.firmwareVersion);

    if (deviceInfo.isProvisioned)
    {
        Serial.println("✅ Device is provisioned");
    }
    else
    {
        Serial.println("⚠ Device needs provisioning");
        Serial.println("🔑 Pairing Code: " + deviceInfo.pairingCode);
    }
}

void DeviceManager::generateMinimalDeviceInfo()
{
    // Only generate essential info - UUID will come from server registration
    deviceInfo.macAddress = WiFi.macAddress();
    deviceInfo.firmwareVersion = FIRMWARE_VERSION;
    deviceInfo.isProvisioned = false;
    deviceInfo.isOnline = false;
    deviceInfo.deviceId = ""; // Will be assigned by server during registration

    Serial.println("🔄 Generated minimal device info (UUID will be assigned by server)");
}

void DeviceManager::generateDeviceInfo()
{
    // Generate proper UUID from MAC address for backend compatibility
    String macAddr = WiFi.macAddress();
    macAddr.replace(":", "");
    macAddr.toLowerCase();

    // Generate UUID v4 format from MAC address
    deviceInfo.deviceId = generateUUIDFromMAC(macAddr);

    deviceInfo.macAddress = WiFi.macAddress();
    deviceInfo.firmwareVersion = FIRMWARE_VERSION;
    deviceInfo.isProvisioned = false;
    deviceInfo.isOnline = false;

    // Generate a simple 6-character pairing code based on MAC
    String pairingBase = macAddr.substring(6); // Last 6 chars of MAC
    deviceInfo.pairingCode = "";
    for (int i = 0; i < 6; i++)
    {
        char c = pairingBase[i];
        if (c >= '0' && c <= '9')
        {
            deviceInfo.pairingCode += c;
        }
        else
        {
            // Convert hex letters to numbers (A=1, B=2, etc.)
            deviceInfo.pairingCode += String((c >= 'A' ? c - 'A' + 1 : c - 'a' + 1) % 10);
        }
    }

    Serial.println("🔄 Generated new device info");
}

bool DeviceManager::saveDeviceInfo()
{
    preferences.putString(PREF_DEVICE_ID, deviceInfo.deviceId);
    preferences.putString(PREF_MAC_ADDRESS, deviceInfo.macAddress);
    preferences.putBool(PREF_IS_PROVISIONED, deviceInfo.isProvisioned);

    // Save server-provided pairing code
    if (deviceInfo.pairingCode.length() > 0)
    {
        preferences.putString(PREF_PAIRING_CODE, deviceInfo.pairingCode);
    }

    Serial.println("💾 Device info saved");
    return true;
}

bool DeviceManager::loadDeviceInfo()
{
    String savedDeviceId = preferences.getString(PREF_DEVICE_ID, "");

    if (savedDeviceId.length() == 0)
    {
        return false;
    }

    deviceInfo.deviceId = savedDeviceId;
    deviceInfo.macAddress = preferences.getString(PREF_MAC_ADDRESS, WiFi.macAddress());
    deviceInfo.isProvisioned = preferences.getBool(PREF_IS_PROVISIONED, false);
    deviceInfo.firmwareVersion = FIRMWARE_VERSION;
    deviceInfo.isOnline = false;

    // Load pairing code from server (stored during registration)
    // Don't generate local pairing codes - server provides authoritative codes
    deviceInfo.pairingCode = preferences.getString(PREF_PAIRING_CODE, "");
    if (!deviceInfo.isProvisioned && deviceInfo.pairingCode.length() == 0)
    {
        // No stored pairing code yet - will be assigned during server registration
        deviceInfo.pairingCode = "";
    }

    Serial.println("📂 Device info loaded from preferences");
    return true;
}

bool DeviceManager::registerMinimalWithServer(const String &serverUrl)
{
    if (serverUrl.length() == 0)
    {
        Serial.println("❌ No server URL provided for minimal registration");
        return false;
    }

    // Convert WebSocket URL to HTTP URL for registration
    String httpUrl = serverUrl;
    httpUrl.replace("ws://", "http://");
    httpUrl.replace("wss://", "https://");

    // Remove port if it's the WebSocket port, add API endpoint
    int portIndex = httpUrl.lastIndexOf(':');
    if (portIndex > 7)
    { // After http://
        String baseUrl = httpUrl.substring(0, portIndex);
        httpUrl = baseUrl + ":3000/devices/register"; // API is on port 3000
    }
    else
    {
        httpUrl += "/devices/register";
    }

    HTTPClient http;
    http.begin(httpUrl);
    http.addHeader("Content-Type", "application/json");

    // Minimal registration - only required field per backend documentation
    // This significantly reduces memory usage and startup time
    // Optional fields (lighting config) will be sent later via WebSocket
    JsonDocument doc;
    doc["macAddress"] = deviceInfo.macAddress;

    String payload;
    serializeJson(doc, payload);

    Serial.println("📡 Performing minimal device registration...");
    Serial.println("🌐 URL: " + httpUrl);
    Serial.println("📦 Minimal Payload: " + payload);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode == 200 || httpResponseCode == 201)
    {
        String response = http.getString();
        Serial.println("✅ Minimal device registration successful!");

        // Only print first 200 chars of response to avoid memory issues
        if (response.length() > 200)
        {
            Serial.println("📨 Response: " + response.substring(0, 200) + "...");
        }
        else
        {
            Serial.println("📨 Response: " + response);
        }

        // Parse response to get device ID and pairing code
        JsonDocument responseDoc;
        if (deserializeJson(responseDoc, response) == DeserializationError::Ok)
        {
            // Handle both flat response and nested device object
            JsonVariant deviceData;
            if (responseDoc["device"].isNull())
            {
                deviceData = responseDoc.as<JsonVariant>();
            }
            else
            {
                deviceData = responseDoc["device"].as<JsonVariant>();
            }

            // Store the server-provided device UUID (critical!)
            if (deviceData["id"].is<String>())
            {
                deviceInfo.deviceId = deviceData["id"].as<String>();
                Serial.println("🆔 Server assigned Device UUID: " + deviceInfo.deviceId);
            }
            else if (deviceData["deviceId"].is<String>())
            {
                deviceInfo.deviceId = deviceData["deviceId"].as<String>();
                Serial.println("🆔 Server assigned Device UUID: " + deviceInfo.deviceId);
            }

            if (deviceData["pairingCode"].is<String>())
            {
                deviceInfo.pairingCode = deviceData["pairingCode"].as<String>();
                Serial.println("🔑 Server assigned Pairing Code: " + deviceInfo.pairingCode);
            }

            // Check if device is already claimed/provisioned
            if (deviceData["status"].is<String>())
            {
                String deviceStatus = deviceData["status"].as<String>();
                Serial.println("📊 Device Status: " + deviceStatus);

                if (deviceStatus == "claimed")
                {
                    deviceInfo.isProvisioned = true;
                    Serial.println("✅ Device is already claimed - marking as provisioned");
                }
                else
                {
                    deviceInfo.isProvisioned = false;
                    Serial.println("📝 Device is not yet claimed - waiting for user pairing");
                }
            }
        }

        saveDeviceInfo();
        http.end();
        return true;
    }
    else
    {
        Serial.println("❌ Minimal device registration failed");
        Serial.println("📊 HTTP Response Code: " + String(httpResponseCode));
        if (httpResponseCode > 0)
        {
            Serial.println("📨 Response: " + http.getString());
        }
        http.end();
        return false;
    }
}

// Full registration with capabilities - should be called after minimal registration and system verification
bool DeviceManager::registerWithServer(const String &serverUrl)
{
    if (serverUrl.length() == 0)
    {
        Serial.println("❌ No server URL provided for full registration");
        return false;
    }

    // Convert WebSocket URL to HTTP URL for registration
    String httpUrl = serverUrl;
    httpUrl.replace("ws://", "http://");
    httpUrl.replace("wss://", "https://");

    // Remove port if it's the WebSocket port, add API endpoint
    int portIndex = httpUrl.lastIndexOf(':');
    if (portIndex > 7)
    { // After http://
        String baseUrl = httpUrl.substring(0, portIndex);
        httpUrl = baseUrl + ":3000/devices/register"; // Assume API is on port 3000
    }
    else
    {
        httpUrl += "/devices/register";
    }

    HTTPClient http;
    http.begin(httpUrl);
    http.addHeader("Content-Type", "application/json");

    // Prepare registration data according to RegisterDeviceDto schema
    JsonDocument doc;
    doc["macAddress"] = deviceInfo.macAddress;
    doc["deviceType"] = DEVICE_TYPE;
    doc["firmwareVersion"] = deviceInfo.firmwareVersion;

    // Update IP address
    deviceInfo.ipAddress = WiFi.localIP().toString();
    doc["ipAddress"] = deviceInfo.ipAddress;

    // Include lighting configuration if available
    Preferences lightingPrefs;
    lightingPrefs.begin("light_config", true); // Use same namespace as LightManager
    String lightingSystem = lightingPrefs.getString("system_type", "");

    if (lightingSystem.length() > 0)
    {
        if (isValidLightingSystemType(lightingSystem))
        {
            doc["lightingSystemType"] = lightingSystem;

            String lightingHost = lightingPrefs.getString("host_addr", "");
            int lightingPort = lightingPrefs.getInt("port", 0);
            String authToken = lightingPrefs.getString("auth_token", "");

            if (lightingHost.length() > 0)
            {
                doc["lightingHostAddress"] = lightingHost;
            }

            if (lightingPort > 0)
            {
                doc["lightingPort"] = lightingPort;
            }

            if (authToken.length() > 0)
            {
                doc["lightingAuthToken"] = authToken;
            }

            Serial.println("📡 Including lighting configuration in registration:");
            Serial.println("💡 System: " + lightingSystem);
            if (lightingHost.length() > 0)
            {
                Serial.println("🌐 Host: " + lightingHost + (lightingPort > 0 ? ":" + String(lightingPort) : ""));
            }
        }
        else
        {
            Serial.println("⚠ Invalid lighting system type '" + lightingSystem + "' - skipping in registration");
            Serial.println("📋 Valid types: nanoleaf, wled, ws2812, philips_hue");
        }
    }
    lightingPrefs.end();

    String payload;
    serializeJson(doc, payload);

    Serial.println("📡 Registering device with server...");
    Serial.println("🌐 URL: " + httpUrl);
    Serial.println("📦 Payload: " + payload);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode == 200 || httpResponseCode == 201)
    {
        String response = http.getString();
        Serial.println("✅ Device registered successfully!");
        // Only print first 200 chars of response to avoid memory issues
        if (response.length() > 200)
        {
            Serial.println("📨 Response: " + response.substring(0, 200) + "...");
        }
        else
        {
            Serial.println("📨 Response: " + response);
        }

        // Parse response to get device ID and pairing code
        JsonDocument responseDoc;
        if (deserializeJson(responseDoc, response) == DeserializationError::Ok)
        {
            // Handle both flat response and nested device object
            JsonVariant deviceData;
            if (responseDoc["device"].isNull())
            {
                deviceData = responseDoc.as<JsonVariant>();
            }
            else
            {
                deviceData = responseDoc["device"].as<JsonVariant>();
            }

            if (deviceData["id"].is<String>())
            {
                deviceInfo.deviceId = deviceData["id"].as<String>();
                Serial.println("🆔 Server assigned Device ID: " + deviceInfo.deviceId);
            }
            else if (deviceData["deviceId"].is<String>())
            {
                deviceInfo.deviceId = deviceData["deviceId"].as<String>();
                Serial.println("🆔 Server assigned Device ID: " + deviceInfo.deviceId);
            }

            if (deviceData["pairingCode"].is<String>())
            {
                deviceInfo.pairingCode = deviceData["pairingCode"].as<String>();
                Serial.println("🔑 Server assigned Pairing Code: " + deviceInfo.pairingCode);
            }

            // Check if device is already claimed/provisioned
            if (deviceData["status"].is<String>())
            {
                String deviceStatus = deviceData["status"].as<String>();
                Serial.println("📊 Device Status: " + deviceStatus);

                if (deviceStatus == "claimed")
                {
                    deviceInfo.isProvisioned = true;
                    Serial.println("✅ Device is already claimed - marking as provisioned");
                }
                else
                {
                    deviceInfo.isProvisioned = false;
                    Serial.println("📝 Device is not yet claimed - waiting for user pairing");
                }
            }
        }

        saveDeviceInfo();
        http.end();
        return true;
    }
    else
    {
        Serial.println("❌ Device registration failed");
        Serial.println("📊 HTTP Response Code: " + String(httpResponseCode));
        if (httpResponseCode > 0)
        {
            Serial.println("📨 Response: " + http.getString());
        }
        http.end();
        return false;
    }
}

bool DeviceManager::updateStatus(const String &serverUrl, LightManager *lightManager)
{
    if (serverUrl.length() == 0 || deviceInfo.deviceId.length() == 0)
    {
        return false;
    }

    // Convert WebSocket URL to HTTP URL for status update
    String httpUrl = serverUrl;
    httpUrl.replace("ws://", "http://");
    httpUrl.replace("wss://", "https://");

    int portIndex = httpUrl.lastIndexOf(':');
    if (portIndex > 7)
    {
        String baseUrl = httpUrl.substring(0, portIndex);
        httpUrl = baseUrl + ":3000/devices/" + deviceInfo.deviceId + "/status";
    }
    else
    {
        httpUrl += "/devices/" + deviceInfo.deviceId + "/status";
    }

    HTTPClient http;
    http.begin(httpUrl);
    http.addHeader("Content-Type", "application/json");

    // Create status update payload according to UpdateStatusDto schema
    JsonDocument doc;
    doc["isOnline"] = true;
    doc["isProvisioned"] = deviceInfo.isProvisioned;
    doc["ipAddress"] = WiFi.localIP().toString();
    doc["firmwareVersion"] = deviceInfo.firmwareVersion;
    doc["macAddress"] = deviceInfo.macAddress;

    // Add WiFi signal strength if available
    doc["wifiRSSI"] = WiFi.RSSI();

    // Structure system stats according to schema
    JsonObject systemStats = doc["systemStats"].to<JsonObject>();
    systemStats["freeHeap"] = ESP.getFreeHeap();
    systemStats["uptime"] = millis();
    systemStats["lastUpdate"] = ""; // Will be set by server

    // Do NOT include lighting system config fields in status update payload (per backend schema)

    String payload;
    serializeJson(doc, payload);

    int httpResponseCode = http.PUT(payload);
    http.end();

    if (httpResponseCode == 200)
    {
        markStatusUpdated();
        return true;
    }

    return false;
}

void DeviceManager::setProvisioned(bool provisioned)
{
    deviceInfo.isProvisioned = provisioned;
    preferences.putBool(PREF_IS_PROVISIONED, provisioned);

    if (provisioned)
    {
        Serial.println("✅ Device marked as provisioned");
    }
    else
    {
        Serial.println("⚠ Device marked as not provisioned");
    }
}

bool DeviceManager::isProvisioned()
{
    return deviceInfo.isProvisioned;
}

String DeviceManager::getDeviceId()
{
    return deviceInfo.deviceId;
}

String DeviceManager::getMacAddress()
{
    return deviceInfo.macAddress;
}

String DeviceManager::getPairingCode()
{
    return deviceInfo.pairingCode;
}

DeviceInfo DeviceManager::getDeviceInfo()
{
    return deviceInfo;
}

void DeviceManager::resetDevice()
{
    Serial.println("🔄 Resetting device...");

    // Clear all stored data
    preferences.clear();

    // Regenerate device info
    generateDeviceInfo();
    saveDeviceInfo();

    Serial.println("✅ Device reset complete");
    Serial.println("🆔 New Device ID: " + deviceInfo.deviceId);
    Serial.println("🔑 New Pairing Code: " + deviceInfo.pairingCode);
}

bool DeviceManager::shouldUpdateStatus()
{
    return (millis() - lastStatusUpdate) > STATUS_UPDATE_INTERVAL;
}

void DeviceManager::markStatusUpdated()
{
    lastStatusUpdate = millis();
}

void DeviceManager::setOnlineStatus(bool online)
{
    deviceInfo.isOnline = online;
}

bool DeviceManager::isOnline()
{
    return deviceInfo.isOnline;
}
