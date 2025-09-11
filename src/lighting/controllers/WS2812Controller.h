#ifndef WS2812_CONTROLLER_H
#define WS2812_CONTROLLER_H

#include "../LightController.h"

// Only include FastLED if available (for boards that support it)
#ifdef FASTLED_VERSION
#include <FastLED.h>
#else
// Fallback LED control for basic ESP32 implementations
#ifdef ESP32
#include <Adafruit_NeoPixel.h>
#endif
#endif

/**
 * Generic WS2812B LED strip controller
 *
 * This controller provides direct control of WS2812B LED strips
 * connected to the ESP32. This is the "basic" lighting option
 * that works with simple LED strips.
 *
 * Features:
 * - Direct GPIO control of WS2812B strips
 * - Color animations and transitions
 * - Brightness control
 * - Multiple animation patterns
 */
class WS2812Controller : public LightController
{
private:
    int ledPin;
    int ledCount;
    int brightness;

#ifdef FASTLED_VERSION
    CRGB *leds;
    int ledType;
#else
#ifdef ESP32
    Adafruit_NeoPixel *strip;
#endif
#endif

    // Animation state
    struct
    {
        bool isAnimating;
        unsigned long lastUpdate;
        int currentStep;
        int totalSteps;
        ColorPalette currentPalette;
        String currentAnimation;
    } animationState;

public:
    WS2812Controller();
    virtual ~WS2812Controller();

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

    // WS2812-specific methods
    void setPixelColor(int pixel, const RGBColor &color);
    void showLEDs();
    void clearLEDs();
    void animateLoop(); // Call this in main loop for animations

    // Animation methods
    bool startFadeAnimation(const ColorPalette &palette, int duration);
    bool startStaticDisplay(const ColorPalette &palette);
    bool startRainbowAnimation(int duration);
    bool startWipeAnimation(const ColorPalette &palette, int duration);

private:
    void initializeLEDs();
    void distributePaletteColors(const ColorPalette &palette);
    RGBColor interpolateColor(const RGBColor &color1, const RGBColor &color2, float factor);
    RGBColor rainbowColor(int position, int total);
};

#endif // WS2812_CONTROLLER_H
