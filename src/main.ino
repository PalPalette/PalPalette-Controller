/*
 * PalPalette ESP32 Controller - Modular Self-Setup Version
 * Firmware Version: 2.0.0
 *
 * This is a complete refactor of the original monolithic ESP32 firmware
 * to support self-setup capability for open-source distribution.
 *
 * Key Features:
 * - WiFi captive portal for initial setup
 * - Automatic device registration with backend
 * - Self-generated device IDs and pairing codes
 * - Modular architecture for maintainability
 * - Full integration with new backend API
 */

#include <Arduino.h>
#include "config.h"
#include "core/WiFiManager.h"
#include "core/DeviceManager.h"
#include "core/WSClient.h"
#include "lighting/LightManager.h"

// Global objects
WiFiManager wifiManager;
DeviceManager deviceManager;
LightManager lightManager;
WSClient *wsClient = nullptr;

// State management
enum DeviceState
{
    STATE_INIT,
    STATE_WIFI_SETUP,
    STATE_WIFI_CONNECTING,
    STATE_DEVICE_REGISTRATION,
    STATE_WAITING_FOR_CLAIM,
    STATE_OPERATIONAL,
    STATE_ERROR
};

DeviceState currentState = STATE_INIT;
unsigned long stateChangeTime = 0;

// Timing variables
unsigned long lastStatusUpdate = 0;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000; // 10 seconds

// Helper function for repeating strings
String repeatString(const String &str, int count)
{
    String result = "";
    for (int i = 0; i < count; i++)
    {
        result += str;
    }
    return result;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n" + repeatString("=", 50));
    Serial.println("🎨 PalPalette ESP32 Controller Starting...");
    Serial.println("📦 Firmware Version: " + String(FIRMWARE_VERSION));
    Serial.println("🏗 Architecture: Modular Self-Setup");
    Serial.println(repeatString("=", 50));

    // Initialize managers
    Serial.println("\n🔧 Initializing system components...");

    wifiManager.begin();
    deviceManager.begin();

    // Initialize lighting system (WiFi-independent setup only)
    Serial.println("💡 Preparing lighting system...");

    // Only load configuration, don't attempt network connections yet
    if (lightManager.beginWithoutConfig())
    {
        Serial.println("✅ Lighting system ready - network initialization will occur after WiFi connection");
    }
    else
    {
        Serial.println("❌ Lighting system initialization failed");
    } // Print device information
    DeviceInfo deviceInfo = deviceManager.getDeviceInfo();
    Serial.println("\n📱 Device Information:");
    Serial.println("🆔 Device ID: " + deviceInfo.deviceId);
    Serial.println("📡 MAC Address: " + deviceInfo.macAddress);
    Serial.println("🔧 Firmware: " + deviceInfo.firmwareVersion);

    if (deviceInfo.isProvisioned)
    {
        Serial.println("✅ Status: Provisioned");
    }
    else
    {
        Serial.println("⚠ Status: Not provisioned");
        Serial.println("🔑 Pairing Code: " + deviceInfo.pairingCode);
        Serial.println("📱 Use this code in the mobile app to claim this device");
    }

    // Start state machine
    setState(STATE_WIFI_SETUP);

    Serial.println("\n🚀 System initialization complete!");
    Serial.println("🔄 Starting main operation loop...\n");
}

void loop()
{
    // Update all managers
    wifiManager.loop();
    lightManager.loop();
    if (wsClient)
    {
        wsClient->loop();
    }

    // Handle state machine
    handleStateMachine();

    // Periodic tasks
    handlePeriodicTasks();

    // Reset watchdog timer to prevent crashes
    yield();

    // Small delay to prevent watchdog issues and reduce memory pressure
    delay(100);
}

void setState(DeviceState newState)
{
    if (currentState != newState)
    {
        currentState = newState;
        stateChangeTime = millis();

        String stateName = getStateName(newState);
        Serial.println("🔄 State changed to: " + stateName);
    }
}

String getStateName(DeviceState state)
{
    switch (state)
    {
    case STATE_INIT:
        return "INIT";
    case STATE_WIFI_SETUP:
        return "WIFI_SETUP";
    case STATE_WIFI_CONNECTING:
        return "WIFI_CONNECTING";
    case STATE_DEVICE_REGISTRATION:
        return "DEVICE_REGISTRATION";
    case STATE_WAITING_FOR_CLAIM:
        return "WAITING_FOR_CLAIM";
    case STATE_OPERATIONAL:
        return "OPERATIONAL";
    case STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

void handleStateMachine()
{
    switch (currentState)
    {
    case STATE_INIT:
        // Should not reach here as we start with WIFI_SETUP
        setState(STATE_WIFI_SETUP);
        break;

    case STATE_WIFI_SETUP:
        handleWiFiSetup();
        break;

    case STATE_WIFI_CONNECTING:
        handleWiFiConnecting();
        break;

    case STATE_DEVICE_REGISTRATION:
        handleDeviceRegistration();
        break;

    case STATE_WAITING_FOR_CLAIM:
        handleWaitingForClaim();
        break;

    case STATE_OPERATIONAL:
        handleOperational();
        break;

    case STATE_ERROR:
        handleError();
        break;
    }
}

void handleWiFiSetup()
{
    // Check if we have stored WiFi credentials
    if (wifiManager.hasStoredCredentials())
    {
        Serial.println("📶 Found stored WiFi credentials, attempting connection...");
        setState(STATE_WIFI_CONNECTING);
    }
    else
    {
        // Start captive portal if not already running
        if (!wifiManager.isInAPMode())
        {
            Serial.println("📶 No WiFi credentials found, starting setup mode...");
            Serial.println("🌐 Please connect to the WiFi network to configure this device:");

            String macAddr = WiFi.macAddress();
            macAddr.replace(":", "");
            String apSSID = String(DEFAULT_AP_SSID) + "-" + macAddr.substring(6);

            Serial.println("📶 Network: " + apSSID);
            Serial.println("🔐 Password: " + String(DEFAULT_AP_PASSWORD));
            Serial.println("🌐 Open a web browser to configure WiFi settings");

            wifiManager.startAPMode();
        }
    }
}

void handleWiFiConnecting()
{
    static unsigned long connectStartTime = 0;

    if (connectStartTime == 0)
    {
        connectStartTime = millis();
        Serial.println("📶 Attempting WiFi connection...");
    }

    if (wifiManager.connectToWiFi())
    {
        // WiFiManager already prints connection success message

        // Now that WiFi is connected, initialize lighting system with saved configuration
        Serial.println("🔄 WiFi connected - initializing lighting system with saved configuration...");
        if (lightManager.begin())
        {
            Serial.println("✅ Lighting system initialized with saved configuration");
        }
        else
        {
            Serial.println("📝 No saved lighting configuration found - will wait for mobile app setup");
        }

        connectStartTime = 0;
        setState(STATE_DEVICE_REGISTRATION);
    }
    else
    {
        // Check for timeout
        if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT)
        {
            Serial.println("⏰ WiFi connection timeout, returning to setup mode");
            connectStartTime = 0;
            setState(STATE_WIFI_SETUP);
        }
    }
}

void handleDeviceRegistration()
{
    static bool registrationAttempted = false;

    if (!registrationAttempted)
    {
        Serial.println("📡 Starting device registration process...");

        // First register with HTTP API
        String serverUrl = wifiManager.getServerURL();
        if (deviceManager.registerWithServer(serverUrl))
        {
            Serial.println("✅ Device registered with HTTP API");

            // Initialize WebSocket client
            if (wsClient)
            {
                delete wsClient;
            }
            wsClient = new WSClient(&deviceManager, &lightManager);
            wsClient->begin(serverUrl);

            // Attempt WebSocket connection
            if (wsClient->connect())
            {
                Serial.println("✅ WebSocket connection established");

                // Check provisioning status after registration response
                if (deviceManager.isProvisioned())
                {
                    Serial.println("🎉 Device is already claimed - transitioning to operational mode");
                    setState(STATE_OPERATIONAL);
                }
                else
                {
                    Serial.println("📝 Device is not yet claimed - waiting for user pairing");
                    setState(STATE_WAITING_FOR_CLAIM);
                }
            }
            else
            {
                Serial.println("⚠ WebSocket connection failed, will retry...");
            }
        }
        else
        {
            Serial.println("❌ Device registration failed, will retry...");
        }

        registrationAttempted = true;
    }

    // Reset registration attempt flag after delay
    if (millis() - stateChangeTime > REGISTRATION_RETRY_INTERVAL)
    {
        registrationAttempted = false;
    }
}

void handleWaitingForClaim()
{
    // Display pairing information periodically
    static unsigned long lastPairingInfo = 0;
    const unsigned long PAIRING_INFO_INTERVAL = 60000; // Reduced to 1 minute to reduce serial output

    if (millis() - lastPairingInfo > PAIRING_INFO_INTERVAL)
    {
        DeviceInfo deviceInfo = deviceManager.getDeviceInfo();
        Serial.println("\n📱 ===== DEVICE WAITING FOR CLAIM =====");
        Serial.println("🆔 Device ID: " + deviceInfo.deviceId);
        Serial.println("🔑 Pairing Code: " + deviceInfo.pairingCode);
        Serial.println("📱 Open the PalPalette mobile app and use this pairing code");
        Serial.println("⏰ Waiting for user to claim this device...");
        Serial.println("=====================================\n");

        lastPairingInfo = millis();
    }

    // Check if device was claimed (this will be handled by WebSocket message)
    if (deviceManager.isProvisioned())
    {
        Serial.println("🎉 Device has been claimed! Transitioning to operational mode.");
        setState(STATE_OPERATIONAL);
    }
}

void handleOperational()
{
    // Device is fully operational
    static unsigned long lastOperationalInfo = 0;
    const unsigned long OPERATIONAL_INFO_INTERVAL = 60000; // 1 minute

    if (millis() - lastOperationalInfo > OPERATIONAL_INFO_INTERVAL)
    {
        Serial.println("✅ Device operational - Ready to receive color palettes");
        lastOperationalInfo = millis();
    }

    // Check if device lost provisioning (shouldn't happen normally)
    if (!deviceManager.isProvisioned())
    {
        Serial.println("⚠ Device lost provisioning, returning to waiting state");
        setState(STATE_WAITING_FOR_CLAIM);
    }
}

void handleError()
{
    static unsigned long lastErrorReport = 0;
    const unsigned long ERROR_REPORT_INTERVAL = 10000; // 10 seconds

    if (millis() - lastErrorReport > ERROR_REPORT_INTERVAL)
    {
        Serial.println("❌ Device in error state - attempting recovery...");
        lastErrorReport = millis();

        // Try to recover by going back to WiFi setup
        setState(STATE_WIFI_SETUP);
    }
}

void handlePeriodicTasks()
{
    // Check WiFi connection periodically
    if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL)
    {
        if (currentState >= STATE_DEVICE_REGISTRATION && !wifiManager.isConnected())
        {
            Serial.println("⚠ WiFi connection lost, attempting recovery...");
            setState(STATE_WIFI_CONNECTING);
        }
        lastWiFiCheck = millis();
    }

    // Update device status periodically (if registered and connected)
    if (currentState >= STATE_DEVICE_REGISTRATION && deviceManager.shouldUpdateStatus())
    {
        if (wifiManager.isConnected())
        {
            String serverUrl = wifiManager.getServerURL();
            if (deviceManager.updateStatus(serverUrl))
            {
                Serial.println("📊 Device status updated successfully");
            }
        }
    }
}

/*
 * Utility Functions
 */

void printSystemStatus()
{
    Serial.println("\n" + repeatString("=", 40));
    Serial.println("📊 SYSTEM STATUS REPORT");
    Serial.println(repeatString("=", 40));

    // Device info
    DeviceInfo deviceInfo = deviceManager.getDeviceInfo();
    Serial.println("🆔 Device ID: " + deviceInfo.deviceId);
    Serial.println("📡 MAC Address: " + deviceInfo.macAddress);
    Serial.println("🔧 Firmware: " + deviceInfo.firmwareVersion);
    Serial.println("✅ Provisioned: " + String(deviceInfo.isProvisioned ? "Yes" : "No"));

    if (!deviceInfo.isProvisioned)
    {
        Serial.println("🔑 Pairing Code: " + deviceInfo.pairingCode);
    }

    // Network info
    Serial.println("📶 WiFi SSID: " + wifiManager.getSSID());
    Serial.println("📍 IP Address: " + wifiManager.getLocalIP());
    Serial.println("🔗 WiFi Connected: " + String(wifiManager.isConnected() ? "Yes" : "No"));

    // WebSocket info
    if (wsClient)
    {
        Serial.println("🔌 WebSocket: " + String(wsClient->isClientConnected() ? "Connected" : "Disconnected"));
    }
    else
    {
        Serial.println("🔌 WebSocket: Not initialized");
    }

    // System info
    Serial.println("🧠 Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("⏰ Uptime: " + String(millis() / 1000) + " seconds");
    Serial.println("🔄 Current State: " + getStateName(currentState));

    Serial.println(repeatString("=", 40) + "\n");
}

/*
 * Serial Command Interface (for debugging)
 */
void serialEvent()
{
    if (Serial.available())
    {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "status")
        {
            printSystemStatus();
        }
        else if (command == "reset")
        {
            Serial.println("🔄 Resetting device...");
            deviceManager.resetDevice();
            wifiManager.clearWiFiCredentials();
            ESP.restart();
        }
        else if (command == "restart")
        {
            Serial.println("🔄 Restarting device...");
            ESP.restart();
        }
        else if (command == "wifi")
        {
            Serial.println("📶 WiFi Status:");
            Serial.println("  SSID: " + wifiManager.getSSID());
            Serial.println("  IP: " + wifiManager.getLocalIP());
            Serial.println("  Connected: " + String(wifiManager.isConnected() ? "Yes" : "No"));
        }
        else if (command == "prefs")
        {
            Serial.println("🗂 Preferences Debug:");
            Preferences debugPrefs;

            // Check palpalette namespace (main device preferences)
            debugPrefs.begin("palpalette", true);
            Serial.println("📋 Namespace: 'palpalette'");
            Serial.println("  lighting_system: '" + debugPrefs.getString("lighting_system", "") + "'");
            Serial.println("  lighting_host: '" + debugPrefs.getString("lighting_host", "") + "'");
            Serial.println("  lighting_port: " + String(debugPrefs.getInt("lighting_port", 0)));
            Serial.println("  wifi_ssid: '" + debugPrefs.getString("wifi_ssid", "") + "'");
            debugPrefs.end();

            // Check light_config namespace (legacy lighting preferences)
            debugPrefs.begin("light_config", true);
            Serial.println("📋 Namespace: 'light_config'");
            Serial.println("  system_type: '" + debugPrefs.getString("system_type", "") + "'");
            Serial.println("  host_addr: '" + debugPrefs.getString("host_addr", "") + "'");
            Serial.println("  port: " + String(debugPrefs.getInt("port", 0)));
            debugPrefs.end();
        }
        else if (command == "lights")
        {
            Serial.println("💡 Reinitializing lighting system...");
            if (lightManager.begin())
            {
                Serial.println("✅ Lighting system reinitialized: " + lightManager.getCurrentSystemType());
            }
            else
            {
                Serial.println("❌ Failed to reinitialize lighting system");
            }
        }
        else if (command == "nanoleaf")
        {
            Serial.println("🔍 Testing Nanoleaf discovery and connection...");
            if (lightManager.getCurrentSystemType() == "nanoleaf")
            {
                Serial.println("💡 Current system is Nanoleaf, testing connection...");
                if (lightManager.testConnection())
                {
                    Serial.println("✅ Nanoleaf connection test successful");
                }
                else
                {
                    Serial.println("❌ Nanoleaf connection test failed");
                }
            }
            else
            {
                Serial.println("⚠ Current system is not Nanoleaf (current: " + lightManager.getCurrentSystemType() + ")");
                Serial.println("💡 Try 'lights' command to reinitialize lighting system");
            }
        }
        else if (command == "help")
        {
            Serial.println("🆘 Available Commands:");
            Serial.println("  status   - Show full system status");
            Serial.println("  wifi     - Show WiFi information");
            Serial.println("  prefs    - Show preferences debug info");
            Serial.println("  lights   - Reinitialize lighting system");
            Serial.println("  nanoleaf - Test Nanoleaf discovery and connection");
            Serial.println("  reset    - Reset device settings");
            Serial.println("  restart  - Restart the device");
            Serial.println("  help     - Show this help message");
        }
        else if (command.length() > 0)
        {
            Serial.println("❓ Unknown command: " + command);
            Serial.println("💡 Type 'help' for available commands");
        }
    }
}
