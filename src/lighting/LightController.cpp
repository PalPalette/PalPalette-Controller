#include "LightController.h"
#include "controllers/NanoleafController.h"

// Static array of supported systems
static String supportedSystems[] = {
    "nanoleaf"};

static const int SUPPORTED_SYSTEM_COUNT = sizeof(supportedSystems) / sizeof(supportedSystems[0]);

// Factory implementation
LightController *LightControllerFactory::createController(const String &systemType)
{
    String type = systemType;
    type.toLowerCase();

    Serial.println("🏭 Creating controller for system: " + type);

    if (type == "nanoleaf")
    {
        Serial.println("🍃 Creating Nanoleaf controller");
        return new NanoleafController();
    }
    else
    {
        Serial.println("❌ Unknown system type: " + type + " (only 'nanoleaf' is supported)");
        return nullptr;
    }
}

String *LightControllerFactory::getSupportedSystems()
{
    return supportedSystems;
}

int LightControllerFactory::getSupportedSystemCount()
{
    return SUPPORTED_SYSTEM_COUNT;
}

bool LightControllerFactory::isSystemSupported(const String &systemType)
{
    String type = systemType;
    type.toLowerCase();

    return (type == "nanoleaf" || type == "wled" || type == "ws2812");
}

// Utility functions for color conversion
uint32_t LightControllerUtils::rgbToUint32(const RGBColor &color)
{
    return (color.r << 16) | (color.g << 8) | color.b;
}

RGBColor LightControllerUtils::uint32ToRgb(uint32_t color)
{
    RGBColor rgb;
    rgb.r = (color >> 16) & 0xFF;
    rgb.g = (color >> 8) & 0xFF;
    rgb.b = color & 0xFF;
    return rgb;
}

RGBColor LightControllerUtils::interpolateColor(const RGBColor &color1, const RGBColor &color2, float factor)
{
    if (factor <= 0.0)
        return color1;
    if (factor >= 1.0)
        return color2;

    RGBColor result;
    result.r = color1.r + (color2.r - color1.r) * factor;
    result.g = color1.g + (color2.g - color1.g) * factor;
    result.b = color1.b + (color2.b - color1.b) * factor;

    return result;
}

RGBColor LightControllerUtils::hsv2rgb(float h, float s, float v)
{
    RGBColor rgb;
    float c = v * s;
    float x = c * (1 - abs(fmod(h / 60.0, 2) - 1));
    float m = v - c;

    float r, g, b;

    if (h >= 0 && h < 60)
    {
        r = c;
        g = x;
        b = 0;
    }
    else if (h >= 60 && h < 120)
    {
        r = x;
        g = c;
        b = 0;
    }
    else if (h >= 120 && h < 180)
    {
        r = 0;
        g = c;
        b = x;
    }
    else if (h >= 180 && h < 240)
    {
        r = 0;
        g = x;
        b = c;
    }
    else if (h >= 240 && h < 300)
    {
        r = x;
        g = 0;
        b = c;
    }
    else
    {
        r = c;
        g = 0;
        b = x;
    }

    rgb.r = (r + m) * 255;
    rgb.g = (g + m) * 255;
    rgb.b = (b + m) * 255;

    return rgb;
}

RGBColor LightControllerUtils::adjustBrightness(const RGBColor &color, float brightness)
{
    if (brightness < 0.0)
        brightness = 0.0;
    if (brightness > 1.0)
        brightness = 1.0;

    RGBColor result;
    result.r = color.r * brightness;
    result.g = color.g * brightness;
    result.b = color.b * brightness;

    return result;
}

String LightControllerUtils::formatJsonError(const String &error)
{
    JsonDocument doc;
    doc["success"] = false;
    doc["error"] = error;

    String result;
    serializeJson(doc, result);
    return result;
}

String LightControllerUtils::formatJsonSuccess(const String &message)
{
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = message;

    String result;
    serializeJson(doc, result);
    return result;
}
