#include "LightManager.h"
#include "LightController.h"
#include "../config.h"

// Static constants
const char *LightManager::PREF_NAMESPACE = "light_config";
const char *LightManager::PREF_SYSTEM_TYPE = "system_type";
const char *LightManager::PREF_HOST_ADDRESS = "host_addr";
const char *LightManager::PREF_PORT = "port";
const char *LightManager::PREF_AUTH_TOKEN = "auth_token";
const char *LightManager::PREF_CUSTOM_CONFIG = "custom_config";

LightManager::LightManager() : currentController(nullptr), isInitialized(false)
{
}

LightManager::~LightManager()
{
    cleanupController();
}

bool LightManager::begin()
{
    Serial.println("🌈 Initializing Light Manager...");

    // Load configuration from EEPROM
    if (loadConfiguration())
    {
        Serial.println("📋 Loaded lighting configuration: " + config.systemType);

        // Create and initialize controller (but don't authenticate yet)
        if (createController(config.systemType))
        {
            Serial.println("🔄 Attempting to initialize " + config.systemType + " controller...");
            if (currentController->initialize(config))
            {
                isInitialized = true;
                Serial.println("✅ Light Manager initialized successfully");

                // For systems with saved credentials, try to authenticate immediately
                if (currentController->requiresAuthentication())
                {
                    if (config.authToken.length() > 0)
                    {
                        Serial.println("🔐 Found saved credentials, attempting automatic authentication...");
                        if (currentController->isReady())
                        {
                            Serial.println("✅ Lighting system authenticated and ready");
                        }
                        else
                        {
                            Serial.println("⚠ Automatic authentication not fully successful");
                            Serial.println("   System will retry authentication when needed");
                        }
                    }
                    else
                    {
                        Serial.println("⚠ Lighting system requires authentication - will authenticate after device pairing");
                    }
                }
                else
                {
                    Serial.println("ℹ️ Lighting system does not require authentication");
                }

                return true;
            }
            else
            {
                Serial.println("⚠ Lighting controller failed to initialize (hardware may not be connected)");
                // Don't fail completely - allow system to continue running
                isInitialized = true; // Mark as initialized but with failed controller
                return true;
            }
        }
        else
        {
            Serial.println("❌ Failed to create lighting controller");
            return false;
        }
    }
    else
    {
        Serial.println("📋 No lighting configuration found - waiting for app configuration");
        // Don't auto-configure any lighting system
        isInitialized = true;
        return true;
    }

    return false;
}

bool LightManager::beginWithoutConfig()
{
    Serial.println("🌈 Initializing Light Manager (no auto-config)...");

    // Initialize preferences but don't load/create any lighting configuration
    isInitialized = true;

    Serial.println("✅ Light Manager ready - waiting for configuration from mobile app");
    return true;
}

bool LightManager::configure(const String &systemType, const String &hostAddress,
                             int port, const String &authToken,
                             const JsonObject &customConfig)
{

    Serial.println("🔧 Configuring lighting system: " + systemType);

    // Clean up existing controller
    cleanupController();

    // Create new configuration
    config.systemType = systemType;
    config.hostAddress = hostAddress;
    config.port = port;
    config.authToken = authToken;
    config.customConfig = customConfig;

    // Create and initialize new controller
    if (createController(systemType))
    {
        // Set up notification callback before initialization
        currentController->setNotificationCallback([this](const String &action, const String &instructions, int timeout)
                                                   { handleUserNotification(action, instructions, timeout); });

        if (currentController->initialize(config))
        {
            // Save configuration to EEPROM
            saveConfiguration();
            isInitialized = true;

            Serial.println("✅ Lighting system configured successfully");
            Serial.println("📊 System: " + systemType);
            if (hostAddress.length() > 0)
            {
                Serial.println("🌐 Host: " + hostAddress + ":" + String(port));
            }

            return true;
        }
        else
        {
            Serial.println("❌ Failed to initialize lighting controller");
            cleanupController();
            return false;
        }
    }
    else
    {
        Serial.println("❌ Unknown lighting system type: " + systemType);
        return false;
    }
}

bool LightManager::displayPalette(const ColorPalette &palette)
{
    if (!isInitialized)
    {
        Serial.println("❌ Light Manager not initialized");
        return false;
    }

    if (!currentController)
    {
        Serial.println("❌ No lighting controller available");
        return false;
    }

    if (!currentController->isReady())
    {
        Serial.println("⚠ Lighting controller not ready (hardware may not be connected)");
        return false;
    }

    Serial.println("🎨 Displaying palette: " + palette.name);
    return currentController->displayPalette(palette);
}

bool LightManager::turnOff()
{
    if (!isReady())
    {
        return false;
    }

    return currentController->turnOff();
}

bool LightManager::setBrightness(int brightness)
{
    if (!isReady())
    {
        return false;
    }

    return currentController->setBrightness(brightness);
}

bool LightManager::testConnection()
{
    if (!isReady())
    {
        return false;
    }

    return currentController->testConnection();
}

String LightManager::getStatus()
{
    if (!isReady())
    {
        return "Not Initialized";
    }

    return currentController->getStatus();
}

JsonObject LightManager::getCapabilities()
{
    if (!isReady())
    {
        JsonDocument doc;
        JsonObject caps = doc.to<JsonObject>();
        caps["error"] = "Not initialized";
        return caps;
    }

    return currentController->getCapabilities();
}

bool LightManager::requiresAuthentication()
{
    if (!isReady())
    {
        return false;
    }

    return currentController->requiresAuthentication();
}

bool LightManager::authenticate()
{
    if (!isReady())
    {
        return false;
    }

    Serial.println("🔐 Starting authentication for " + config.systemType);
    bool success = currentController->authenticate();

    if (success)
    {
        // Update auth token in config if available
        LightConfig updatedConfig = currentController->getUpdatedConfig();
        Serial.println("🔍 Updated config received from authenticate():");
        Serial.println("  - Host Address: " + updatedConfig.hostAddress);
        Serial.println("  - Port: " + String(updatedConfig.port));
        Serial.println("  - Auth Token Length: " + String(updatedConfig.authToken.length()));

        // Update all configuration fields that may have changed during authentication
        if (updatedConfig.hostAddress.length() > 0)
        {
            config.hostAddress = updatedConfig.hostAddress;
            Serial.println("💾 Updated host address: " + config.hostAddress);
        }

        if (updatedConfig.port > 0)
        {
            config.port = updatedConfig.port;
            Serial.println("💾 Updated port: " + String(config.port));
        }

        if (updatedConfig.authToken.length() > 0)
        {
            config.authToken = updatedConfig.authToken;
            Serial.println("💾 Updated auth token (length: " + String(config.authToken.length()) + ")");
        }

        saveConfiguration();
        Serial.println("✅ Authentication successful");
    }
    else
    {
        Serial.println("❌ Authentication failed");
    }

    return success;
}

bool LightManager::authenticateLightingSystem()
{
    if (!currentController)
    {
        Serial.println("❌ No lighting controller available for authentication");
        return false;
    }

    if (!currentController->requiresAuthentication())
    {
        Serial.println("✅ Lighting system does not require authentication");
        return true;
    }

    Serial.println("🔐 Starting lighting system authentication...");

    if (currentController->authenticate())
    {
        Serial.println("✅ Lighting system authentication successful");
        // Get updated configuration with new auth tokens
        LightConfig updatedConfig = currentController->getUpdatedConfig();
        Serial.println("🔍 Updated config received:");
        Serial.println("  - System Type: " + updatedConfig.systemType);
        Serial.println("  - Host Address: " + updatedConfig.hostAddress);
        Serial.println("  - Port: " + String(updatedConfig.port));
        Serial.println("  - Auth Token Length: " + String(updatedConfig.authToken.length()));

        // Update all configuration fields that may have changed during authentication
        if (updatedConfig.hostAddress.length() > 0)
        {
            config.hostAddress = updatedConfig.hostAddress;
            Serial.println("💾 Updated host address in local config: " + config.hostAddress);
        }

        if (updatedConfig.port > 0)
        {
            config.port = updatedConfig.port;
            Serial.println("💾 Updated port in local config: " + String(config.port));
        }

        if (updatedConfig.authToken.length() > 0)
        {
            config.authToken = updatedConfig.authToken;
            Serial.println("💾 Updated auth token in local config (length: " + String(config.authToken.length()) + ")");
        }

        // Save updated configuration (may include new auth tokens)
        bool saveResult = saveConfiguration();
        Serial.println("💾 Save configuration result: " + String(saveResult ? "SUCCESS" : "FAILED"));
        return true;
    }
    else
    {
        Serial.println("❌ Lighting system authentication failed");
        return false;
    }
}

bool LightManager::requiresUserAuthentication()
{
    if (!currentController)
    {
        return false;
    }

    return currentController->requiresAuthentication() && !currentController->isReady();
}

String LightManager::getCurrentSystemType()
{
    return config.systemType;
}

bool LightManager::saveConfiguration()
{
    Serial.println("💾 Starting to save lighting configuration...");
    Serial.println("   System Type: " + config.systemType);
    Serial.println("   Host Address: " + config.hostAddress);
    Serial.println("   Port: " + String(config.port));
    Serial.println("   Auth Token Length: " + String(config.authToken.length()));
    Serial.println("   Auth Token Preview: " + (config.authToken.length() > 0 ? config.authToken.substring(0, min(8, (int)config.authToken.length())) + "..." : "None"));

    // Check if we have minimum required data
    if (config.systemType.length() == 0)
    {
        Serial.println("❌ Cannot save config: System type is empty");
        return false;
    }

    // Check available flash memory
    Serial.println("🔍 Flash Memory Info:");
    Serial.println("  - Free heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("  - Flash size: " + String(ESP.getFlashChipSize()) + " bytes");

    preferences.begin(PREF_NAMESPACE, false);
    Serial.println("🔧 Opened preferences namespace: " + String(PREF_NAMESPACE));

    bool success = true;

    // Save system type
    if (preferences.putString(PREF_SYSTEM_TYPE, config.systemType))
    {
        Serial.println("✅ Saved system type: " + config.systemType);
    }
    else
    {
        Serial.println("❌ Failed to save system type: " + config.systemType);
        success = false;
    }

    // Save host address
    Serial.println("🔍 Host address to save: '" + config.hostAddress + "' (length: " + String(config.hostAddress.length()) + ")");
    if (preferences.putString(PREF_HOST_ADDRESS, config.hostAddress))
    {
        Serial.println("✅ Saved host address: " + config.hostAddress);
    }
    else
    {
        Serial.println("❌ Failed to save host address: '" + config.hostAddress + "'");

        // Try to save as empty string if null
        if (config.hostAddress.length() == 0)
        {
            Serial.println("🔧 Trying to save empty host address...");
            if (preferences.putString(PREF_HOST_ADDRESS, ""))
            {
                Serial.println("✅ Saved empty host address");
            }
            else
            {
                Serial.println("❌ Failed to save even empty host address");
                success = false;
            }
        }
        else
        {
            success = false;
        }
    }

    // Save port
    if (preferences.putInt(PREF_PORT, config.port))
    {
        Serial.println("✅ Saved port: " + String(config.port));
    }
    else
    {
        Serial.println("❌ Failed to save port: " + String(config.port));
        success = false;
    }

    // Save auth token
    if (preferences.putString(PREF_AUTH_TOKEN, config.authToken))
    {
        Serial.println("✅ Saved auth token (length: " + String(config.authToken.length()) + ")");
    }
    else
    {
        Serial.println("❌ Failed to save auth token (length: " + String(config.authToken.length()) + ")");
        success = false;
    }

    // Serialize and save custom config
    String customConfigStr = serializeCustomConfig(config.customConfig);
    Serial.println("🔍 Custom config string: '" + customConfigStr + "'");

    // Handle empty custom config
    if (customConfigStr.length() == 0)
    {
        customConfigStr = "{}"; // Use empty JSON object instead of empty string
        Serial.println("🔧 Using empty JSON object for custom config");
    }

    if (preferences.putString(PREF_CUSTOM_CONFIG, customConfigStr))
    {
        Serial.println("✅ Saved custom config (length: " + String(customConfigStr.length()) + ")");
    }
    else
    {
        Serial.println("❌ Failed to save custom config (length: " + String(customConfigStr.length()) + ")");
        Serial.println("❌ Custom config content: '" + customConfigStr + "'");
        success = false;
    }

    preferences.end();
    Serial.println("🔧 Closed preferences namespace");

    if (success)
    {
        Serial.println("✅ Lighting configuration saved successfully");

        // Verify by reading back immediately
        Serial.println("🔍 Verifying saved configuration...");
        preferences.begin(PREF_NAMESPACE, true);
        String verifySystemType = preferences.getString(PREF_SYSTEM_TYPE, "");
        String verifyAuthToken = preferences.getString(PREF_AUTH_TOKEN, "");
        preferences.end();

        Serial.println("📋 Verification results:");
        Serial.println("  - System Type: '" + verifySystemType + "'");
        Serial.println("  - Auth Token Length: " + String(verifyAuthToken.length()));

        if (verifySystemType != config.systemType || verifyAuthToken != config.authToken)
        {
            Serial.println("❌ Verification failed - data not correctly saved!");
            return false;
        }
        else
        {
            Serial.println("✅ Verification successful - data correctly saved");
        }
    }
    else
    {
        Serial.println("❌ Failed to save lighting configuration - some preferences failed");
    }

    return success;
}

bool LightManager::loadConfiguration()
{
    // First check if there's configuration from WiFi setup (captive portal)
    Preferences wifiPrefs;
    wifiPrefs.begin(DEVICE_PREF_NAMESPACE, true);

    String systemTypeFromWifi = wifiPrefs.getString("lighting_system", "");
    if (systemTypeFromWifi.length() > 0)
    {
        Serial.println("📱 Loading lighting configuration from WiFi setup");
        config.systemType = systemTypeFromWifi;
        config.hostAddress = wifiPrefs.getString("lighting_host", "");
        config.port = wifiPrefs.getInt("lighting_port", 80);
        config.authToken = ""; // Will be auto-generated for Nanoleaf

        // Set default ports based on system type
        if (config.port == 0)
        {
            if (config.systemType == "nanoleaf")
            {
                config.port = 16021;
            }
            else if (config.systemType == "wled")
            {
                config.port = 80;
            }
        }

        // Create default custom config based on system type
        config.customConfig = createDefaultCustomConfig(config.systemType);

        wifiPrefs.end();
        return true;
    }
    wifiPrefs.end();

    // Fall back to old lighting-specific preferences
    preferences.begin(PREF_NAMESPACE, true);

    config.systemType = preferences.getString(PREF_SYSTEM_TYPE, "");

    if (config.systemType.length() == 0)
    {
        preferences.end();
        return false; // No configuration found
    }

    config.hostAddress = preferences.getString(PREF_HOST_ADDRESS, "");
    config.port = preferences.getInt(PREF_PORT, 80);
    config.authToken = preferences.getString(PREF_AUTH_TOKEN, "");

    String customConfigStr = preferences.getString(PREF_CUSTOM_CONFIG, "");
    config.customConfig = parseCustomConfig(customConfigStr);

    preferences.end();
    return true;
}

void LightManager::resetConfiguration()
{
    Serial.println("🔄 Resetting lighting configuration");

    preferences.begin(PREF_NAMESPACE, false);
    preferences.clear();
    preferences.end();

    cleanupController();
    isInitialized = false;

    // Reset to empty config
    config = LightConfig();
}

String *LightManager::getSupportedSystems()
{
    return LightControllerFactory::getSupportedSystems();
}

int LightManager::getSupportedSystemCount()
{
    return LightControllerFactory::getSupportedSystemCount();
}

LightConfig LightManager::createDefaultConfig(const String &systemType)
{
    LightConfig config;
    config.systemType = systemType;

    if (systemType == "nanoleaf")
    {
        config.port = 16021; // Default Nanoleaf port
    }
    else if (systemType == "wled")
    {
        config.port = 80; // Default HTTP port
    }
    else if (systemType == "ws2812")
    {
        config.port = 0; // Not applicable for direct control

        // Default WS2812 configuration
        JsonDocument doc;
        doc["ledPin"] = 2;
        doc["ledCount"] = 30;
        doc["brightness"] = 255;
        config.customConfig = doc.as<JsonObject>();
    }

    return config;
}

void LightManager::loop()
{
    if (!isReady())
    {
        return;
    }

    // Handle WS2812 animations if using WS2812 controller
    if (config.systemType == "ws2812")
    {
        // This would call the animation loop for WS2812Controller
        // We need to add this method to the base class or cast
        // For now, this is a placeholder
    }
}

bool LightManager::createController(const String &systemType)
{
    currentController = LightControllerFactory::createController(systemType);
    return (currentController != nullptr);
}

void LightManager::cleanupController()
{
    if (currentController)
    {
        delete currentController;
        currentController = nullptr;
    }
    isInitialized = false;
}

JsonObject LightManager::parseCustomConfig(const String &configStr)
{
    JsonDocument doc;

    if (configStr.length() > 0)
    {
        DeserializationError error = deserializeJson(doc, configStr);
        if (!error)
        {
            return doc.as<JsonObject>();
        }
    }

    // Return empty object if parsing fails
    return doc.to<JsonObject>();
}

String LightManager::serializeCustomConfig(const JsonObject &config)
{
    if (config.isNull())
    {
        return "";
    }

    String result;
    serializeJson(config, result);
    return result;
}

JsonObject LightManager::createDefaultCustomConfig(const String &systemType)
{
    JsonDocument doc;

    if (systemType == "ws2812")
    {
        doc["ledPin"] = 2;
        doc["ledCount"] = 30;
        doc["brightness"] = 255;
    }
    else if (systemType == "wled")
    {
        // WLED doesn't need special custom config
    }
    else if (systemType == "nanoleaf")
    {
        // Nanoleaf config will be auto-discovered
    }

    return doc.as<JsonObject>();
}

void LightManager::handleUserNotification(const String &action, const String &instructions, int timeout)
{
    Serial.println("🔔 User Action Required:");
    Serial.println("   Action: " + action);
    Serial.println("   Instructions: " + instructions);
    if (timeout > 0)
    {
        Serial.println("   Timeout: " + String(timeout) + " seconds");
    }

    // Forward to external callback (e.g., WebSocket client for mobile app notification)
    if (userNotificationCallback)
    {
        userNotificationCallback(action, instructions, timeout);
    }
}

bool LightManager::retryInitialization()
{
    if (isInitialized && currentController != nullptr && currentController->isReady())
    {
        Serial.println("💡 Light controller is already working correctly");
        return true;
    }

    Serial.println("🔄 Retrying lighting system initialization...");

    if (currentController != nullptr && config.systemType.length() > 0)
    {
        Serial.println("🔄 Attempting to re-initialize " + config.systemType + " controller...");
        Serial.println("🔍 Config details:");
        Serial.println("  - System Type: " + config.systemType);
        Serial.println("  - Host Address: '" + config.hostAddress + "'");
        Serial.println("  - Port: " + String(config.port));
        Serial.println(String("  - Auth Token: ") + (config.authToken.length() > 0 ? "Present" : "None"));

        if (currentController->initialize(config))
        {
            Serial.println("✅ Lighting controller initialized successfully on retry");
            return true;
        }
        else
        {
            Serial.println("❌ Lighting controller initialization failed on retry");
            return false;
        }
    }
    else
    {
        Serial.println("⚠ No controller or configuration available for retry");
        return false;
    }
}
