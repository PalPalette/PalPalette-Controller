#ifndef LIGHT_CONTROLLER_H
#define LIGHT_CONTROLLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

// Constants
#define MAX_COLORS 10

/**
 * Color structure for RGB values
 */
struct RGBColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    RGBColor() : r(0), g(0), b(0) {}
    RGBColor(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

    // Convert from hex string
    static RGBColor fromHex(const String &hexColor)
    {
        String hex = hexColor;
        if (hex.startsWith("#"))
        {
            hex = hex.substring(1);
        }

        long rgb = strtol(hex.c_str(), NULL, 16);
        return RGBColor(
            (rgb >> 16) & 0xFF,
            (rgb >> 8) & 0xFF,
            rgb & 0xFF);
    }

    // Convert to hex string
    String toHex() const
    {
        char hex[8];
        sprintf(hex, "#%02X%02X%02X", r, g, b);
        return String(hex);
    }
};

/**
 * Color palette structure
 */
struct ColorPalette
{
    RGBColor colors[10]; // Maximum 10 colors
    int colorCount;
    String name;
    String messageId;
    String senderName;
    int duration;     // Display duration in milliseconds
    String animation; // Animation type (fade, pulse, static, etc.)

    ColorPalette() : colorCount(0), duration(5000), animation("fade") {}
};

/**
 * Light system configuration
 */
struct LightConfig
{
    String systemType;       // "nanoleaf", "wled", "generic_ws2812", etc.
    String hostAddress;      // IP address or hostname
    int port;                // Port number
    String authToken;        // Authentication token
    JsonObject customConfig; // System-specific configuration
};

/**
 * Abstract base class for all light controllers
 * This defines the interface that all lighting systems must implement
 */
class LightController
{
public:
    virtual ~LightController() {}

    /**
     * Initialize the light controller with configuration
     * @param config Configuration specific to this light system
     * @return true if initialization successful
     */
    virtual bool initialize(const LightConfig &config) = 0;

    /**
     * Test connection to the light system
     * @return true if connection is working
     */
    virtual bool testConnection() = 0;

    /**
     * Display a color palette on the lighting system
     * @param palette The color palette to display
     * @return true if display was successful
     */
    virtual bool displayPalette(const ColorPalette &palette) = 0;

    /**
     * Turn off all lights
     * @return true if successful
     */
    virtual bool turnOff() = 0;

    /**
     * Set brightness level (0-100)
     * @param brightness Brightness percentage
     * @return true if successful
     */
    virtual bool setBrightness(int brightness) = 0;

    /**
     * Get the current status of the light system
     * @return Status string for debugging
     */
    virtual String getStatus() = 0;

    /**
     * Get the system type identifier
     * @return System type string
     */
    virtual String getSystemType() = 0;

    /**
     * Perform any authentication required by the system
     * This might involve requesting tokens, pairing, etc.
     * @return true if authentication successful
     */
    virtual bool authenticate() = 0;

    /**
     * Check if the system requires authentication
     * @return true if authentication is needed
     */
    virtual bool requiresAuthentication() = 0;

    /**
     * Get updated configuration after authentication
     * This allows controllers to return updated auth tokens, etc.
     * @return updated configuration
     */
    virtual LightConfig getUpdatedConfig() { return config; }

    /**
     * Get system capabilities (number of lights, supported features, etc.)
     * @return JSON object describing capabilities
     */
    virtual JsonObject getCapabilities() = 0;

    /**
     * Check if the controller is ready for operations
     * @return true if controller is initialized and ready
     */
    virtual bool isReady() const
    {
        return isInitialized && isAuthenticated;
    }

    /**
     * Set callback for user interaction notifications
     * @param callback Function to call when user interaction is needed
     *                Format: callback(action_type, instructions, timeout_seconds)
     */
    virtual void setNotificationCallback(std::function<void(const String &, const String &, int)> callback)
    {
        // Default implementation - controllers can override if they support notifications
    }

protected:
    LightConfig config;
    bool isInitialized = false;
    bool isAuthenticated = false;

    /**
     * Utility function to convert color palette to system-specific format
     * Subclasses can override this for custom color handling
     */
    virtual JsonArray colorsToJson(const ColorPalette &palette)
    {
        JsonDocument doc;
        JsonArray colorArray = doc.to<JsonArray>();

        for (int i = 0; i < palette.colorCount; i++)
        {
            JsonObject color = colorArray.add<JsonObject>();
            color["r"] = palette.colors[i].r;
            color["g"] = palette.colors[i].g;
            color["b"] = palette.colors[i].b;
            color["hex"] = palette.colors[i].toHex();
        }

        return colorArray;
    }

    /**
     * Log debug information if debugging is enabled
     */
    void debugLog(const String &message)
    {
#ifdef DEBUG_LIGHT_CONTROLLER
        Serial.println("[" + getSystemType() + "] " + message);
#endif
    }
};

/**
 * Factory class for creating light controllers
 */
class LightControllerFactory
{
public:
    /**
     * Create a light controller instance based on system type
     * @param systemType The type of lighting system
     * @return Pointer to light controller instance, or nullptr if unknown type
     */
    static LightController *createController(const String &systemType);

    /**
     * Get list of supported lighting systems
     * @return Array of supported system type strings
     */
    static String *getSupportedSystems();

    /**
     * Get number of supported systems
     * @return Count of supported systems
     */
    static int getSupportedSystemCount();

    /**
     * Check if a system type is supported
     * @param systemType The system type to check
     * @return True if supported, false otherwise
     */
    static bool isSystemSupported(const String &systemType);
};

/**
 * Utility class for color and light controller operations
 */
class LightControllerUtils
{
public:
    /**
     * Convert hex color string to RGBColor
     * @param hexColor Hex color string (e.g., "#FF0000" or "FF0000")
     * @return RGBColor struct
     */
    static RGBColor hexToColor(const String &hexColor)
    {
        RGBColor color;
        String hex = hexColor;

        // Remove # if present
        if (hex.startsWith("#"))
        {
            hex = hex.substring(1);
        }

        // Ensure we have 6 characters
        if (hex.length() == 6)
        {
            color.r = strtol(hex.substring(0, 2).c_str(), NULL, 16);
            color.g = strtol(hex.substring(2, 4).c_str(), NULL, 16);
            color.b = strtol(hex.substring(4, 6).c_str(), NULL, 16);
        }
        else
        {
            // Default to black if invalid
            color.r = color.g = color.b = 0;
        }

        return color;
    }

    /**
     * Convert RGBColor to hex string
     * @param color RGBColor struct
     * @return Hex color string (e.g., "#FF0000")
     */
    static String colorToHex(const RGBColor &color)
    {
        char hex[8];
        sprintf(hex, "#%02X%02X%02X", color.r, color.g, color.b);
        return String(hex);
    }

    /**
     * Convert RGB values to uint32_t
     * @param color RGBColor struct
     * @return 32-bit color value
     */
    static uint32_t rgbToUint32(const RGBColor &color);

    /**
     * Convert uint32_t to RGB values
     * @param color 32-bit color value
     * @return RGBColor struct
     */
    static RGBColor uint32ToRgb(uint32_t color);

    /**
     * Interpolate between two colors
     * @param color1 First color
     * @param color2 Second color
     * @param factor Interpolation factor (0.0 to 1.0)
     * @return Interpolated color
     */
    static RGBColor interpolateColor(const RGBColor &color1, const RGBColor &color2, float factor);

    /**
     * Convert HSV to RGB
     * @param h Hue (0-360)
     * @param s Saturation (0-1)
     * @param v Value/Brightness (0-1)
     * @return RGBColor struct
     */
    static RGBColor hsv2rgb(float h, float s, float v);

    /**
     * Adjust color brightness
     * @param color Original color
     * @param brightness Brightness factor (0.0 to 1.0)
     * @return Adjusted color
     */
    static RGBColor adjustBrightness(const RGBColor &color, float brightness);

    /**
     * Format JSON error response
     * @param error Error message
     * @return JSON formatted error string
     */
    static String formatJsonError(const String &error);

    /**
     * Format JSON success response
     * @param message Success message
     * @return JSON formatted success string
     */
    static String formatJsonSuccess(const String &message);
};

#endif // LIGHT_CONTROLLER_H
