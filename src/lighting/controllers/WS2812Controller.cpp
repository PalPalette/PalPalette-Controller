#include "WS2812Controller.h"

WS2812Controller::WS2812Controller()
    : ledPin(2), ledCount(30), brightness(255)
{

#ifdef FASTLED_VERSION
    leds = nullptr;
    ledType = WS2812B;
#else
#ifdef ESP32
    strip = nullptr;
#endif
#endif

    animationState.isAnimating = false;
    animationState.lastUpdate = 0;
    animationState.currentStep = 0;
    animationState.totalSteps = 0;

    debugLog("WS2812Controller created with default values - Pin: " + String(ledPin) + ", Count: " + String(ledCount));
}

WS2812Controller::~WS2812Controller()
{
#ifdef FASTLED_VERSION
    if (leds)
    {
        delete[] leds;
    }
#else
#ifdef ESP32
    if (strip)
    {
        delete strip;
    }
#endif
#endif
}

bool WS2812Controller::initialize(const LightConfig &config)
{
    this->config = config;

    debugLog("Initializing WS2812 controller");

    // Parse configuration
    if (config.customConfig["ledPin"].is<int>())
    {
        ledPin = config.customConfig["ledPin"];
    }
    if (config.customConfig["ledCount"].is<int>())
    {
        ledCount = config.customConfig["ledCount"];
    }
    if (config.customConfig["brightness"].is<int>())
    {
        brightness = config.customConfig["brightness"];
    }

    debugLog("LED Pin: " + String(ledPin) + ", Count: " + String(ledCount));

    initializeLEDs();

    // Don't test connection during setup - hardware may not be connected yet
    // The test will be done later when the user explicitly tests the system
    debugLog("WS2812 controller initialized (hardware test skipped during setup)");
    isInitialized = true;
    isAuthenticated = true; // No authentication needed
    return true;
}

bool WS2812Controller::testConnection()
{
    debugLog("Testing WS2812 LED strip connection");

    // Check if initialization was successful
#ifdef FASTLED_VERSION
    if (!leds)
    {
        debugLog("WARNING: FastLED not initialized - leds pointer is null (hardware may not be connected)");
        return false;
    }
#else
#ifdef ESP32
    if (!strip)
    {
        debugLog("WARNING: NeoPixel not initialized - strip pointer is null (hardware may not be connected)");
        return false;
    }
#endif
#endif

    try
    {
        // Test with a simple color sequence
        clearLEDs();

        // Red
        setPixelColor(0, RGBColor(255, 0, 0));
        showLEDs();
        delay(200);

        // Green
        setPixelColor(0, RGBColor(0, 255, 0));
        showLEDs();
        delay(200);

        // Blue
        setPixelColor(0, RGBColor(0, 0, 255));
        showLEDs();
        delay(200);

        // Clear
        clearLEDs();
        showLEDs();

        debugLog("WS2812 test sequence completed successfully");
        return true;
    }
    catch (...)
    {
        debugLog("ERROR: Exception during WS2812 test sequence");
        return false;
    }
}

bool WS2812Controller::displayPalette(const ColorPalette &palette)
{
    debugLog("Displaying palette: " + palette.name + " with " + String(palette.colorCount) + " colors");

    animationState.currentPalette = palette;

    if (palette.animation == "static")
    {
        return startStaticDisplay(palette);
    }
    else if (palette.animation == "fade")
    {
        return startFadeAnimation(palette, palette.duration);
    }
    else if (palette.animation == "wipe")
    {
        return startWipeAnimation(palette, palette.duration);
    }
    else if (palette.animation == "rainbow")
    {
        return startRainbowAnimation(palette.duration);
    }
    else
    {
        // Default to static
        return startStaticDisplay(palette);
    }
}

bool WS2812Controller::turnOff()
{
    debugLog("Turning off WS2812 LEDs");
    animationState.isAnimating = false;
    clearLEDs();
    showLEDs();
    return true;
}

bool WS2812Controller::setBrightness(int brightnessPercent)
{
    brightnessPercent = max(0, min(100, brightnessPercent));
    brightness = map(brightnessPercent, 0, 100, 0, 255);

#ifdef FASTLED_VERSION
    FastLED.setBrightness(brightness);
    FastLED.show();
#else
#ifdef ESP32
    if (strip)
    {
        strip->setBrightness(brightness);
        strip->show();
    }
#endif
#endif

    debugLog("Set brightness to " + String(brightnessPercent) + "%");
    return true;
}

String WS2812Controller::getStatus()
{
    String status = "WS2812 Strip | Pin: " + String(ledPin);
    status += " | LEDs: " + String(ledCount);
    status += " | Brightness: " + String(map(brightness, 0, 255, 0, 100)) + "%";
    status += " | Animating: " + String(animationState.isAnimating ? "Yes" : "No");
    return status;
}

String WS2812Controller::getSystemType()
{
    return "ws2812";
}

bool WS2812Controller::authenticate()
{
    return true; // No authentication needed for direct LED control
}

bool WS2812Controller::requiresAuthentication()
{
    return false;
}

bool WS2812Controller::isReady() const
{
    // Check if base initialization is complete
    if (!isInitialized || !isAuthenticated)
    {
        return false;
    }

    // Check if hardware is properly initialized
#ifdef FASTLED_VERSION
    return leds != nullptr;
#else
#ifdef ESP32
    return strip != nullptr;
#else
    return true; // No hardware check possible on other platforms
#endif
#endif
}

JsonObject WS2812Controller::getCapabilities()
{
    JsonDocument doc;
    JsonObject caps = doc.to<JsonObject>();

    caps["systemType"] = "ws2812";
    caps["supportsAnimation"] = true;
    caps["supportsBrightness"] = true;
    caps["supportsColorTemperature"] = false;
    caps["maxColors"] = 10;
    caps["ledCount"] = ledCount;
    caps["requiresAuthentication"] = false;
    caps["isDirect"] = true; // Direct GPIO control

    JsonArray supportedAnimations = caps["supportedAnimations"].to<JsonArray>();
    supportedAnimations.add("static");
    supportedAnimations.add("fade");
    supportedAnimations.add("wipe");
    supportedAnimations.add("rainbow");

    return caps;
}

void WS2812Controller::setPixelColor(int pixel, const RGBColor &color)
{
    if (pixel < 0 || pixel >= ledCount)
    {
        return;
    }

#ifdef FASTLED_VERSION
    if (leds)
    {
        leds[pixel] = CRGB(color.r, color.g, color.b);
    }
    else
    {
        debugLog("WARNING: setPixelColor called but leds is null");
    }
#else
#ifdef ESP32
    if (strip)
    {
        strip->setPixelColor(pixel, strip->Color(color.r, color.g, color.b));
    }
    else
    {
        debugLog("WARNING: setPixelColor called but strip is null");
    }
#endif
#endif
}

void WS2812Controller::showLEDs()
{
#ifdef FASTLED_VERSION
    if (leds)
    {
        FastLED.show();
    }
    else
    {
        debugLog("WARNING: showLEDs called but leds is null");
    }
#else
#ifdef ESP32
    if (strip)
    {
        strip->show();
    }
    else
    {
        debugLog("WARNING: showLEDs called but strip is null");
    }
#endif
#endif
}

void WS2812Controller::clearLEDs()
{
#ifdef FASTLED_VERSION
    if (leds)
    {
        fill_solid(leds, ledCount, CRGB::Black);
    }
#else
#ifdef ESP32
    if (strip)
    {
        strip->clear();
    }
#endif
#endif
}

void WS2812Controller::animateLoop()
{
    if (!animationState.isAnimating)
    {
        return;
    }

    unsigned long currentTime = millis();

    // Check if it's time for the next animation step
    unsigned long stepDuration = animationState.totalSteps > 0 ? (animationState.currentPalette.duration / animationState.totalSteps) : 50;

    if (currentTime - animationState.lastUpdate >= stepDuration)
    {
        animationState.lastUpdate = currentTime;

        if (animationState.currentAnimation == "fade")
        {
            // Fade animation logic
            float progress = (float)animationState.currentStep / animationState.totalSteps;

            for (int i = 0; i < ledCount; i++)
            {
                RGBColor color1 = animationState.currentPalette.colors[i % animationState.currentPalette.colorCount];
                RGBColor color2 = animationState.currentPalette.colors[(i + 1) % animationState.currentPalette.colorCount];

                RGBColor interpolated = interpolateColor(color1, color2, progress);
                setPixelColor(i, interpolated);
            }
        }
        else if (animationState.currentAnimation == "wipe")
        {
            // Wipe animation logic
            int pixelsToLight = map(animationState.currentStep, 0, animationState.totalSteps, 0, ledCount);

            clearLEDs();
            for (int i = 0; i < pixelsToLight; i++)
            {
                RGBColor color = animationState.currentPalette.colors[i % animationState.currentPalette.colorCount];
                setPixelColor(i, color);
            }
        }
        else if (animationState.currentAnimation == "rainbow")
        {
            // Rainbow animation logic
            for (int i = 0; i < ledCount; i++)
            {
                RGBColor color = rainbowColor((i + animationState.currentStep) % 360, 360);
                setPixelColor(i, color);
            }
        }

        showLEDs();
        animationState.currentStep++;

        // Check if animation is complete
        if (animationState.currentStep >= animationState.totalSteps)
        {
            animationState.isAnimating = false;
            debugLog("Animation completed");
        }
    }
}

bool WS2812Controller::startFadeAnimation(const ColorPalette &palette, int duration)
{
    animationState.isAnimating = true;
    animationState.currentAnimation = "fade";
    animationState.currentStep = 0;
    animationState.totalSteps = duration / 50; // 50ms per step
    animationState.lastUpdate = millis();

    debugLog("Starting fade animation for " + String(duration) + "ms");
    return true;
}

bool WS2812Controller::startStaticDisplay(const ColorPalette &palette)
{
    animationState.isAnimating = false;
    distributePaletteColors(palette);
    showLEDs();

    debugLog("Displaying static color palette");
    return true;
}

bool WS2812Controller::startRainbowAnimation(int duration)
{
    animationState.isAnimating = true;
    animationState.currentAnimation = "rainbow";
    animationState.currentStep = 0;
    animationState.totalSteps = 360; // One full rainbow cycle
    animationState.lastUpdate = millis();

    debugLog("Starting rainbow animation");
    return true;
}

bool WS2812Controller::startWipeAnimation(const ColorPalette &palette, int duration)
{
    animationState.isAnimating = true;
    animationState.currentAnimation = "wipe";
    animationState.currentStep = 0;
    animationState.totalSteps = ledCount * 2; // Wipe on and off
    animationState.lastUpdate = millis();

    debugLog("Starting wipe animation");
    return true;
}

void WS2812Controller::initializeLEDs()
{
    debugLog("Initializing LEDs with count: " + String(ledCount) + " on pin: " + String(ledPin));

    // Validate parameters
    if (ledCount <= 0 || ledCount > 300)
    { // Limit to 300 LEDs for ESP32C3
        debugLog("ERROR: Invalid LED count: " + String(ledCount) + ", limiting to safe range");
        ledCount = min(300, max(1, ledCount)); // Clamp between 1 and 300
        if (ledCount <= 0)
            ledCount = 30; // Fallback to default
    }

    if (ledPin < 0 || ledPin > 48)
    {
        debugLog("ERROR: Invalid LED pin: " + String(ledPin));
        ledPin = 2; // Reset to safe default
    }

#ifdef FASTLED_VERSION
    try
    {
        leds = new CRGB[ledCount];
        if (leds)
        {
            FastLED.addLeds<WS2812B, ledPin, GRB>(leds, ledCount);
            FastLED.setBrightness(brightness);
            FastLED.clear();
            FastLED.show();
            debugLog("Initialized FastLED library successfully");
        }
        else
        {
            debugLog("ERROR: Failed to allocate memory for FastLED");
        }
    }
    catch (...)
    {
        debugLog("ERROR: Exception during FastLED initialization");
        leds = nullptr;
    }
#else
#ifdef ESP32
    try
    {
        debugLog("Creating NeoPixel object...");
        strip = new Adafruit_NeoPixel(ledCount, ledPin, NEO_GRB + NEO_KHZ800);
        if (strip)
        {
            debugLog("NeoPixel object created, calling begin()...");
            strip->begin();
            debugLog("Begin() completed, setting brightness...");
            strip->setBrightness(brightness);
            debugLog("Brightness set, clearing strip...");
            strip->clear();
            debugLog("Strip cleared, calling show()...");
            strip->show();
            debugLog("Initialized Adafruit NeoPixel library successfully");
        }
        else
        {
            debugLog("ERROR: Failed to create NeoPixel object");
        }
    }
    catch (...)
    {
        debugLog("ERROR: Exception during NeoPixel initialization");
        strip = nullptr;
    }
#else
    debugLog("No LED library available for this platform");
#endif
#endif
}

void WS2812Controller::distributePaletteColors(const ColorPalette &palette)
{
    for (int i = 0; i < ledCount; i++)
    {
        RGBColor color = palette.colors[i % palette.colorCount];
        setPixelColor(i, color);
    }
}

RGBColor WS2812Controller::interpolateColor(const RGBColor &color1, const RGBColor &color2, float factor)
{
    factor = max(0.0f, min(1.0f, factor));

    uint8_t r = color1.r + (color2.r - color1.r) * factor;
    uint8_t g = color1.g + (color2.g - color1.g) * factor;
    uint8_t b = color1.b + (color2.b - color1.b) * factor;

    return RGBColor(r, g, b);
}

RGBColor WS2812Controller::rainbowColor(int position, int total)
{
    position = position % total;
    float hue = (float)position / total * 360.0;

    // Convert HSV to RGB (simplified)
    float s = 1.0;
    float v = 1.0;

    float c = v * s;
    float x = c * (1 - abs(fmod(hue / 60.0, 2) - 1));
    float m = v - c;

    float r, g, b;

    if (hue >= 0 && hue < 60)
    {
        r = c;
        g = x;
        b = 0;
    }
    else if (hue >= 60 && hue < 120)
    {
        r = x;
        g = c;
        b = 0;
    }
    else if (hue >= 120 && hue < 180)
    {
        r = 0;
        g = c;
        b = x;
    }
    else if (hue >= 180 && hue < 240)
    {
        r = 0;
        g = x;
        b = c;
    }
    else if (hue >= 240 && hue < 300)
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

    return RGBColor(
        (uint8_t)((r + m) * 255),
        (uint8_t)((g + m) * 255),
        (uint8_t)((b + m) * 255));
}
