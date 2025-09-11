#include "WLEDController.h"

WLEDController::WLEDController()
    : ledCount(0), isConnected(false)
{
}

WLEDController::~WLEDController()
{
    // WLED doesn't require cleanup
}

bool WLEDController::initialize(const LightConfig &config)
{
    this->config = config;

    debugLog("Initializing WLED controller");
    debugLog("Host: " + config.hostAddress + ":" + String(config.port));

    // Build base URL
    baseUrl = "http://" + config.hostAddress;
    if (config.port != 80)
    {
        baseUrl += ":" + String(config.port);
    }

    // Test connection and get info
    if (testConnection())
    {
        isInitialized = true;
        isAuthenticated = true; // WLED doesn't require authentication by default
        getInfo();
        return true;
    }

    debugLog("Failed to connect to WLED");
    return false;
}

bool WLEDController::testConnection()
{
    JsonDocument response;
    bool success = sendHttpRequest("/json/info", "GET", "", &response);

    if (success && response["ver"].is<const char *>())
    {
        String version = response["ver"];
        debugLog("Successfully connected to WLED version: " + version);
        isConnected = true;
        return true;
    }

    isConnected = false;
    return false;
}

bool WLEDController::displayPalette(const ColorPalette &palette)
{
    if (!isConnected)
    {
        debugLog("Not connected to WLED");
        return false;
    }

    debugLog("Displaying palette: " + palette.name + " with " + String(palette.colorCount) + " colors");

    JsonDocument command = createColorCommand(palette);
    bool success = sendWLEDCommand(command);

    if (success)
    {
        debugLog("Successfully displayed color palette on WLED");
    }
    else
    {
        debugLog("Failed to display color palette on WLED");
    }

    return success;
}

bool WLEDController::turnOff()
{
    JsonDocument command;
    command["on"] = false;

    return sendWLEDCommand(command);
}

bool WLEDController::setBrightness(int brightness)
{
    // Clamp brightness to valid range (WLED uses 0-255)
    brightness = max(0, min(100, brightness));
    int wledBrightness = map(brightness, 0, 100, 0, 255);

    JsonDocument command;
    command["bri"] = wledBrightness;

    bool success = sendWLEDCommand(command);
    if (success)
    {
        debugLog("Set brightness to " + String(brightness) + "%");
    }

    return success;
}

String WLEDController::getStatus()
{
    if (!isConnected)
    {
        return "Disconnected";
    }

    JsonDocument response;
    if (sendHttpRequest("/json/state", "GET", "", &response))
    {
        bool isOn = response["on"];
        int brightness = response["bri"];
        String status = String(isOn ? "On" : "Off");
        status += " | Brightness: " + String(map(brightness, 0, 255, 0, 100)) + "%";
        status += " | LEDs: " + String(ledCount);
        return status;
    }

    return "Connection Error";
}

String WLEDController::getSystemType()
{
    return "wled";
}

bool WLEDController::authenticate()
{
    // WLED typically doesn't require authentication
    // If authentication is needed, it would be implemented here
    isAuthenticated = true;
    return true;
}

bool WLEDController::requiresAuthentication()
{
    return false; // WLED typically doesn't require authentication
}

bool WLEDController::isReady() const
{
    // WLED is ready if initialized, authenticated, and has valid host
    return isInitialized && isAuthenticated && config.hostAddress.length() > 0;
}

JsonObject WLEDController::getCapabilities()
{
    JsonDocument doc;
    JsonObject caps = doc.to<JsonObject>();

    caps["systemType"] = "wled";
    caps["supportsAnimation"] = true;
    caps["supportsBrightness"] = true;
    caps["supportsColorTemperature"] = false;
    caps["maxColors"] = 10;
    caps["ledCount"] = ledCount;
    caps["requiresAuthentication"] = false;

    JsonArray supportedAnimations = caps["supportedAnimations"].to<JsonArray>();
    supportedAnimations.add("static");
    supportedAnimations.add("fade");
    supportedAnimations.add("wipe");
    supportedAnimations.add("rainbow");

    return caps;
}

bool WLEDController::setSegmentColors(const ColorPalette &palette)
{
    JsonDocument command;

    // Create segment command
    JsonArray segments = command["seg"].to<JsonArray>();
    JsonObject segment = segments.add<JsonObject>();

    segment["id"] = wledConfig.segmentId;
    segment["on"] = true;

    // Set colors array
    JsonArray colors = segment["col"].to<JsonArray>();

    for (int i = 0; i < palette.colorCount && i < 3; i++)
    { // WLED supports up to 3 colors per segment
        JsonArray color = colors.add<JsonArray>();
        color.add(palette.colors[i].r);
        color.add(palette.colors[i].g);
        color.add(palette.colors[i].b);
    }

    // Set transition time
    command["transition"] = wledConfig.transitionTime;

    return sendWLEDCommand(command);
}

bool WLEDController::setEffect(const String &effectName)
{
    JsonDocument command;
    JsonArray segments = command["seg"].to<JsonArray>();
    JsonObject segment = segments.add<JsonObject>();

    segment["id"] = wledConfig.segmentId;

    // Map effect names to WLED effect IDs
    if (effectName == "static")
    {
        segment["fx"] = 0; // Solid color
    }
    else if (effectName == "fade")
    {
        segment["fx"] = 1; // Blink
    }
    else if (effectName == "wipe")
    {
        segment["fx"] = 3; // Color wipe
    }
    else if (effectName == "rainbow")
    {
        segment["fx"] = 9; // Rainbow
    }
    else
    {
        segment["fx"] = 0; // Default to solid
    }

    return sendWLEDCommand(command);
}

bool WLEDController::getInfo()
{
    JsonDocument response;
    if (!sendHttpRequest("/json/info", "GET", "", &response))
    {
        return false;
    }

    if (response["leds"].is<JsonObject>())
    {
        JsonObject leds = response["leds"];
        ledCount = leds["count"];
        debugLog("WLED has " + String(ledCount) + " LEDs configured");
    }

    return true;
}

bool WLEDController::sendWLEDCommand(const JsonDocument &command)
{
    String payload;
    serializeJson(command, payload);

    return sendHttpRequest("/json/state", "POST", payload);
}

JsonDocument WLEDController::createColorCommand(const ColorPalette &palette)
{
    JsonDocument command;

    command["on"] = true;
    command["transition"] = wledConfig.transitionTime;

    // Create segment with colors
    JsonArray segments = command["seg"].to<JsonArray>();
    JsonObject segment = segments.add<JsonObject>();

    segment["id"] = wledConfig.segmentId;
    segment["on"] = true;

    // Choose effect based on palette animation
    if (palette.animation == "static")
    {
        segment["fx"] = 0; // Solid color
        // For static, use the first color
        JsonArray colors = segment["col"].to<JsonArray>();
        JsonArray primaryColor = colors.add<JsonArray>();
        primaryColor.add(palette.colors[0].r);
        primaryColor.add(palette.colors[0].g);
        primaryColor.add(palette.colors[0].b);
    }
    else if (palette.animation == "fade")
    {
        segment["fx"] = 1; // Blink/fade effect
        // Set up to 3 colors for the effect
        JsonArray colors = segment["col"].to<JsonArray>();
        for (int i = 0; i < min(palette.colorCount, 3); i++)
        {
            JsonArray color = colors.add<JsonArray>();
            color.add(palette.colors[i].r);
            color.add(palette.colors[i].g);
            color.add(palette.colors[i].b);
        }
    }
    else
    {
        // Default to color wipe for other animations
        segment["fx"] = 3;
        JsonArray colors = segment["col"].to<JsonArray>();
        JsonArray primaryColor = colors.add<JsonArray>();
        primaryColor.add(palette.colors[0].r);
        primaryColor.add(palette.colors[0].g);
        primaryColor.add(palette.colors[0].b);

        if (palette.colorCount > 1)
        {
            JsonArray secondaryColor = colors.add<JsonArray>();
            secondaryColor.add(palette.colors[1].r);
            secondaryColor.add(palette.colors[1].g);
            secondaryColor.add(palette.colors[1].b);
        }
    }

    // Set speed based on duration
    int speed = 128; // Default speed
    if (palette.duration > 0)
    {
        // Map duration to WLED speed (0-255, higher = faster)
        speed = map(palette.duration, 1000, 10000, 255, 50);
        speed = constrain(speed, 0, 255);
    }
    segment["sx"] = speed;

    return command;
}

bool WLEDController::sendHttpRequest(const String &endpoint, const String &method, const String &payload, JsonDocument *response)
{
    String url = baseUrl + endpoint;

    debugLog(method + " " + url);
    if (payload.length() > 0)
    {
        debugLog("Payload: " + payload);
    }

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode;
    if (method == "GET")
    {
        httpResponseCode = http.GET();
    }
    else if (method == "POST")
    {
        httpResponseCode = http.POST(payload);
    }
    else if (method == "PUT")
    {
        httpResponseCode = http.PUT(payload);
    }
    else
    {
        debugLog("Unsupported HTTP method: " + method);
        http.end();
        return false;
    }

    debugLog("HTTP Response Code: " + String(httpResponseCode));

    if (httpResponseCode > 0)
    {
        String responseStr = http.getString();

        if (response != nullptr && responseStr.length() > 0)
        {
            DeserializationError error = deserializeJson(*response, responseStr);
            if (error)
            {
                debugLog("JSON parsing error: " + String(error.c_str()));
                http.end();
                return false;
            }
        }

        http.end();
        return (httpResponseCode >= 200 && httpResponseCode < 300);
    }

    debugLog("HTTP request failed");
    http.end();
    return false;
}
