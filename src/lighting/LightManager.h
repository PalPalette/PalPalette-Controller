#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include "LightController.h"
#include <ArduinoJson.h>
#include <Preferences.h>

/**
 * Light Manager
 * This class manages the lighting system configuration and provides
 * a unified interface for the main application to control lights
 * regardless of the underlying system (Nanoleaf, WLED, WS2812, etc.)
 */
class LightManager
{
private:
    LightController *currentController;
    LightConfig config;
    Preferences preferences;
    bool isInitialized;

    // Configuration keys for EEPROM storage
    static const char *PREF_NAMESPACE;
    static const char *PREF_SYSTEM_TYPE;
    static const char *PREF_HOST_ADDRESS;
    static const char *PREF_PORT;
    static const char *PREF_AUTH_TOKEN;
    static const char *PREF_CUSTOM_CONFIG;

public:
    LightManager();
    ~LightManager();

    /**
     * Initialize the light manager
     * Loads configuration from EEPROM and creates appropriate controller
     */
    bool begin();

    /**
     * Initialize the light manager without auto-configuring any lighting system
     * Use this when lighting configuration will come from the mobile app
     */
    bool beginWithoutConfig();

    /**
     * Configure the lighting system
     * @param systemType Type of lighting system (nanoleaf, wled, ws2812)
     * @param hostAddress IP address or hostname
     * @param port Port number
     * @param authToken Authentication token (if required)
     * @param customConfig Additional system-specific configuration
     */
    bool configure(const String &systemType, const String &hostAddress,
                   int port = 80, const String &authToken = "",
                   const JsonObject &customConfig = JsonObject());

    /**
     * Display a color palette on the configured lighting system
     */
    bool displayPalette(const ColorPalette &palette);

    /**
     * Turn off all lights
     */
    bool turnOff();

    /**
     * Set brightness (0-100%)
     */
    bool setBrightness(int brightness);

    /**
     * Test connection to the lighting system
     */
    bool testConnection();

    /**
     * Get current status
     */
    String getStatus();

    /**
     * Get system capabilities
     */
    JsonObject getCapabilities();

    /**
     * Check if system requires authentication
     */
    bool requiresAuthentication();

    /**
     * Perform authentication (for systems that need it)
     */
    bool authenticate();

    /**
     * Get current configuration
     */
    LightConfig getConfig() const { return config; }

    /**
     * Get current system type
     */
    String getCurrentSystemType();

    /**
     * Perform authentication for systems that require it
     * This should be called AFTER device pairing is complete
     * @return true if authentication successful or not required
     */
    bool authenticateLightingSystem();

    /**
     * Check if lighting system requires user authentication
     * @return true if user interaction is needed for authentication
     */
    bool requiresUserAuthentication();

    /**
     * Check if manager is properly initialized
     */
    bool isReady() const
    {
        return isInitialized && currentController != nullptr && currentController->isReady();
    }

    /**
     * Set callback for user notifications (e.g., WebSocket client)
     */
    void setUserNotificationCallback(std::function<void(const String &, const String &, int)> callback)
    {
        userNotificationCallback = callback;
    }

    /**
     * Save current configuration to EEPROM
     */
    bool saveConfiguration();

    /**
     * Load configuration from EEPROM
     */
    bool loadConfiguration();

    /**
     * Reset configuration to defaults
     */
    void resetConfiguration();

    /**
     * Get list of supported lighting systems
     */
    static String *getSupportedSystems();

    /**
     * Get count of supported systems
     */
    static int getSupportedSystemCount();

    /**
     * Create a default configuration for a system type
     */
    static LightConfig createDefaultConfig(const String &systemType);

    /**
     * Update loop - call this in main loop for animations
     */
    void loop();

    /**
     * Retry initialization if it failed during startup
     * Useful for network-dependent controllers like Nanoleaf
     */
    bool retryInitialization();

private:
    bool createController(const String &systemType);
    void cleanupController();
    JsonObject parseCustomConfig(const String &configStr);
    String serializeCustomConfig(const JsonObject &config);
    JsonObject createDefaultCustomConfig(const String &systemType);

    // User notification handling
    void handleUserNotification(const String &action, const String &instructions, int timeout);
    std::function<void(const String &, const String &, int)> userNotificationCallback;
};

#endif // LIGHT_MANAGER_H
