#include "WSClient.h"

WSClient::WSClient(DeviceManager *devManager, LightManager *lightMgr)
    : deviceManager(devManager), lightManager(lightMgr), isConnected(false),
      lastHeartbeat(0), lastPongReceived(0), lastConnectionAttempt(0),
      retryAttempts(0), lastRetryReset(millis())
{
}

WSClient::~WSClient()
{
    // Proper cleanup of WebSocket connection
    if (isConnected)
    {
        Serial.println("🔌 WSClient destructor: Cleaning up WebSocket connection");
        disconnect();
    }
}

void WSClient::begin(const String &url)
{
    serverUrl = url;

    // Setup WebSocket event callbacks
    client.onMessage([this](WebsocketsMessage message)
                     { onMessageCallback(message); });

    client.onEvent([this](WebsocketsEvent event, String data)
                   { onEventsCallback(event, data); });

    Serial.println("🔌 WebSocket client initialized");
    Serial.println("🌐 Server URL: " + serverUrl);
}

bool WSClient::connect()
{
    if (isConnected)
    {
        return true;
    }

    if (serverUrl.length() == 0)
    {
        Serial.println("❌ No server URL configured for WebSocket connection");
        return false;
    }

    Serial.println("🔌 Attempting WebSocket connection to: " + serverUrl);
    Serial.printf("🔧 Free heap before connection: %d bytes\n", ESP.getFreeHeap());

    // Update attempt time before trying to connect
    lastConnectionAttempt = millis();

    bool connected = client.connect(serverUrl);

    if (connected)
    {
        Serial.println("✅ WebSocket connected successfully!");
        isConnected = true;

        // Initialize heartbeat timer
        lastHeartbeat = millis();

        // Register device immediately after connection
        if (registerDevice())
        {
            Serial.println("📋 Device registration message sent successfully");
        }
        else
        {
            Serial.println("⚠ Device registration message failed to send");
        }

        return true;
    }
    else
    {
        Serial.println("❌ WebSocket connection failed");
        Serial.printf("🔧 Free heap after failed connection: %d bytes\n", ESP.getFreeHeap());
        isConnected = false;
        return false;
    }
}

void WSClient::disconnect()
{
    if (isConnected)
    {
        Serial.println("🔌 Disconnecting WebSocket...");
        Serial.printf("🔧 Free heap before disconnect: %d bytes\n", ESP.getFreeHeap());

        // Send close frame properly
        client.close();

        // Wait a bit for clean disconnection
        delay(100);

        isConnected = false;
        deviceManager->setOnlineStatus(false);

        Serial.printf("🔧 Free heap after disconnect: %d bytes\n", ESP.getFreeHeap());
        Serial.println("✅ WebSocket disconnected cleanly");
    }
}

bool WSClient::isClientConnected()
{
    // Check both our internal state and the client's actual connection state
    bool clientAvailable = client.available();

    // If client reports unavailable but we think we're connected, update our state
    if (isConnected && !clientAvailable)
    {
        Serial.println("⚠ WebSocket client reports unavailable - updating connection state");
        isConnected = false;
        deviceManager->setOnlineStatus(false);
    }

    return isConnected && clientAvailable;
}

void WSClient::loop()
{
    if (isConnected)
    {
        // Poll for WebSocket messages and events
        client.poll();

        // Send heartbeat if needed (every 30 seconds per backend requirements)
        if (shouldSendHeartbeat())
        {
            sendHeartbeat();
        }

        // Check for connection health - if no pong received for too long, assume disconnected
        // This helps detect silent connection drops
        unsigned long timeSinceLastPong = millis() - lastPongReceived;
        unsigned long maxPongWait = HEARTBEAT_INTERVAL * 3; // 90 seconds max without pong

        if (lastPongReceived > 0 && timeSinceLastPong > maxPongWait)
        {
            Serial.println("⚠ No pong response for " + String(timeSinceLastPong / 1000) + "s - connection may be stale");
            Serial.println("🔄 Forcing WebSocket reconnection");
            disconnect();
        }
    }
    else
    {
        // Try to reconnect if needed with exponential backoff
        if (shouldRetryConnection())
        {
            Serial.println("🔄 Attempting WebSocket reconnection...");
            connect();
        }
    }
}

void WSClient::sendHeartbeat()
{
    if (!isClientConnected())
    {
        Serial.println("⚠ Cannot send heartbeat - WebSocket not connected");
        return;
    }

    // Send WebSocket ping frame
    client.ping();
    lastHeartbeat = millis();
    Serial.println("💓 Heartbeat sent");

    // Update device online status
    deviceManager->setOnlineStatus(true);

    // Send periodic status updates every 10 heartbeats (roughly every 5 minutes if heartbeat is every 30 seconds)
    static int heartbeatCount = 0;
    heartbeatCount++;

    if (heartbeatCount >= 10)
    {
        heartbeatCount = 0;
        Serial.println("📊 Sending periodic status updates...");
        sendDeviceStatus();
        sendLightingSystemStatus();
    }
}

bool WSClient::registerDevice()
{
    if (!isClientConnected())
    {
        Serial.println("❌ Cannot register device - WebSocket not connected");
        return false;
    }

    Serial.println("📋 Registering device with WebSocket server...");

    DeviceInfo deviceInfo = deviceManager->getDeviceInfo();

    JsonDocument doc;
    doc["event"] = "registerDevice";
    doc["data"]["deviceId"] = deviceInfo.deviceId;
    doc["data"]["macAddress"] = deviceInfo.macAddress;
    doc["data"]["ipAddress"] = WiFi.localIP().toString();
    doc["data"]["firmwareVersion"] = deviceInfo.firmwareVersion;
    doc["data"]["isProvisioned"] = deviceInfo.isProvisioned;

    if (!deviceInfo.isProvisioned)
    {
        doc["data"]["pairingCode"] = deviceInfo.pairingCode;
    }

    String message;
    serializeJson(doc, message);

    client.send(message);

    Serial.println("📤 Device registration message sent");
    Serial.println("🆔 Device ID: " + deviceInfo.deviceId);
    Serial.println("📡 MAC Address: " + deviceInfo.macAddress);

    if (!deviceInfo.isProvisioned)
    {
        Serial.println("🔑 Pairing Code: " + deviceInfo.pairingCode);
        Serial.println("📱 Share this pairing code with the mobile app to claim this device");
    }

    // Send initial status updates after registration
    sendDeviceStatus();
    sendLightingSystemStatus();

    return true;
}

void WSClient::sendMessage(const String &message)
{
    if (isClientConnected())
    {
        client.send(message);
    }
}

bool WSClient::shouldSendHeartbeat()
{
    return (millis() - lastHeartbeat) > HEARTBEAT_INTERVAL;
}

bool WSClient::shouldRetryConnection()
{
    const unsigned long MAX_RETRY_INTERVAL = 30000;    // Max 30 seconds
    const unsigned long RETRY_RESET_INTERVAL = 300000; // Reset retry count after 5 minutes of failures

    // Reset retry attempts if we've been disconnected for a long time
    if (millis() - lastRetryReset > RETRY_RESET_INTERVAL)
    {
        if (retryAttempts > 0)
        {
            Serial.println("🔄 Resetting WebSocket retry attempts after prolonged disconnection");
            retryAttempts = 0;
        }
        lastRetryReset = millis();
    }

    unsigned long baseInterval = REGISTRATION_RETRY_INTERVAL;
    unsigned long exponentialInterval = baseInterval * (1UL << retryAttempts);
    unsigned long retryInterval = (exponentialInterval < MAX_RETRY_INTERVAL) ? exponentialInterval : MAX_RETRY_INTERVAL;

    if ((millis() - lastConnectionAttempt) > retryInterval)
    {
        retryAttempts++;
        if (retryAttempts > 5)
            retryAttempts = 5; // Cap at 2^5 = 32 * base interval

        Serial.println("🔄 WebSocket retry attempt #" + String(retryAttempts) +
                       ", next retry in " + String((retryInterval * 2) / 1000) + "s");
        return true;
    }

    return false;
}

void WSClient::onMessageCallback(WebsocketsMessage message)
{
    Serial.println("📨 WebSocket message received");

    // Parse JSON message
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message.data());

    if (error)
    {
        Serial.println("❌ JSON parsing failed: " + String(error.c_str()));
        return;
    }

    // Check message type
    if (doc["event"].is<String>())
    {
        String event = doc["event"];
        Serial.println("📝 Event: " + event);

        if (event == "colorPalette")
        {
            handleColorPalette(doc);
        }
        else if (event == "deviceRegistered")
        {
            handleDeviceRegistered(doc);
        }
        else if (event == "deviceClaimed")
        {
            handleDeviceClaimed(doc);
        }
        else if (event == "setupComplete")
        {
            handleSetupComplete(doc);
        }
        else if (event == "lightingSystemConfig")
        {
            handleLightingSystemConfig(doc);
        }
        else if (event == "testLightingSystem")
        {
            handleTestLightingSystem(doc);
        }
        else if (event == "factoryReset")
        {
            handleFactoryReset(doc);
        }
        else if (event == "deviceStatusAck")
        {
            // Backend acknowledges our device status update - this is expected
            Serial.println("✅ Device status acknowledged by server");
        }
        else
        {
            Serial.println("⚠ Unknown event type: " + event);
        }
    }
    else
    {
        Serial.println("⚠ Message missing event field");
    }
}

void WSClient::onEventsCallback(WebsocketsEvent event, String data)
{
    switch (event)
    {
    case WebsocketsEvent::ConnectionOpened:
        Serial.println("🔗 WebSocket connection opened");
        isConnected = true;
        lastConnectionAttempt = millis(); // Reset retry timer on successful connection
        lastPongReceived = millis();      // Initialize pong timer to prevent immediate timeout

        // Reset retry attempts counter - connection is successful
        retryAttempts = 0;
        lastRetryReset = millis();

        Serial.println("✅ WebSocket connection established successfully (retry attempts reset)");
        break;

    case WebsocketsEvent::ConnectionClosed:
        Serial.println("🔌 WebSocket connection closed");
        if (!data.isEmpty())
        {
            Serial.println("📄 Close data: " + data);
        }

        // Log memory status when connection closes unexpectedly
        Serial.printf("💾 Free heap at disconnect: %d bytes\n", ESP.getFreeHeap());

        isConnected = false;
        deviceManager->setOnlineStatus(false);

        // Set retry timer to attempt reconnection
        lastConnectionAttempt = millis();
        break;

    case WebsocketsEvent::GotPing:
        Serial.println("🏓 Ping received from server");
        break;

    case WebsocketsEvent::GotPong:
        Serial.println("🏓 Pong received from server");
        // Update pong received time to track connection health
        lastPongReceived = millis();
        break;

    default:
        Serial.println("❓ Unknown WebSocket event: " + String((int)event));
        break;
    }
}

void WSClient::handleColorPalette(JsonDocument &doc)
{
    Serial.println("\n🎨 ===== COLOR PALETTE RECEIVED =====");

    // Use a temporary legacy palette to extract data from JSON
    LegacyColorPalette legacyPalette;

    // Extract palette data
    legacyPalette.messageId = doc["messageId"].as<String>();
    legacyPalette.senderId = doc["senderId"].as<String>();
    legacyPalette.senderName = doc["senderName"].as<String>();
    legacyPalette.timestamp = doc["timestamp"];

    // Extract colors
    JsonArray colors = doc["colors"];
    legacyPalette.colorCount = min((int)colors.size(), 10);

    Serial.println("📧 Message ID: " + legacyPalette.messageId);
    Serial.println("👤 From: " + legacyPalette.senderName + " (" + legacyPalette.senderId + ")");
    Serial.println("⏰ Timestamp: " + String(legacyPalette.timestamp));
    Serial.println("🌈 Number of colors: " + String(legacyPalette.colorCount));
    Serial.println();

    Serial.println("🎨 Color Palette:");
    Serial.println("+---------+----------+");
    Serial.println("| Color # | Hex Code |");
    Serial.println("+---------+----------+");

    for (int i = 0; i < legacyPalette.colorCount; i++)
    {
        String hexColor = colors[i]["hex"];
        legacyPalette.colors[i] = hexColor;

        // Format output nicely
        String colorNum = String(i + 1);
        if (colorNum.length() == 1)
            colorNum = " " + colorNum;

        Serial.println("|    " + colorNum + "    |  " + hexColor + "  |");
    }

    Serial.println("+---------+----------+");
    Serial.println();

    // Convert to the current palette format
    currentPalette = convertToLightPalette(legacyPalette);

    // Display the palette
    displayColorPaletteSerial();
    displayColorPaletteOnLights();

    Serial.println("🎨 =====================================\n");
}

void WSClient::handleDeviceRegistered(JsonDocument &doc)
{
    Serial.println("\n✅ ===== DEVICE REGISTERED =====");
    Serial.println("✅ Device successfully registered with server!");

    if (doc["data"]["deviceId"].is<String>())
    {
        String serverDeviceId = doc["data"]["deviceId"].as<String>();
        Serial.println("🆔 Server confirmed Device ID: " + serverDeviceId);
    }

    if (doc["data"]["pairingCode"].is<String>())
    {
        String pairingCode = doc["data"]["pairingCode"].as<String>();
        Serial.println("🔑 Pairing Code: " + pairingCode);
        Serial.println("📱 Use this code in the mobile app to claim this device");
    }

    Serial.println("✅ ================================\n");
}

void WSClient::handleDeviceClaimed(JsonDocument &doc)
{
    Serial.println("\n🔐 ===== DEVICE CLAIMED =====");

    String userEmail = doc["data"]["userEmail"].as<String>();
    String userName = doc["data"]["userName"].as<String>();

    Serial.println("🎉 Device has been successfully claimed!");
    Serial.println("👤 Owner: " + userName + " (" + userEmail + ")");

    // Mark device as provisioned
    deviceManager->setProvisioned(true);

    Serial.println("✅ Device is now provisioned and ready to use!");

    // Now that device is claimed by a user, we can start lighting system authentication
    if (lightManager && lightManager->requiresUserAuthentication())
    {
        Serial.println("🔐 Starting lighting system authentication...");

        // This will trigger user notifications (e.g., Nanoleaf button press) via the callback system
        if (lightManager->authenticateLightingSystem())
        {
            Serial.println("✅ Lighting system authentication completed");
        }
        else
        {
            Serial.println("⚠ Lighting system authentication failed - can retry later");
        }
    }

    Serial.println("🔐 ==============================\n");
}

void WSClient::handleSetupComplete(JsonDocument &doc)
{
    Serial.println("\n🎉 ===== SETUP COMPLETED =====");

    String status = doc["data"]["status"].as<String>();

    Serial.println("🎉 Device setup completed successfully!");
    Serial.println("📱 Device is now ready to receive color palettes!");
    Serial.println("🔗 Status: " + status);

    // Ensure device is marked as provisioned
    deviceManager->setProvisioned(true);

    Serial.println("🎉 ==============================\n");
}

void WSClient::handleLightingSystemConfig(JsonDocument &doc)
{
    Serial.println("\n⚡ ===== LIGHTING SYSTEM CONFIG =====");

    if (!lightManager)
    {
        Serial.println("❌ LightManager not available");
        return;
    }

    String systemType = doc["data"]["systemType"].as<String>();
    Serial.println("🔧 System Type: " + systemType);

    // Extract configuration parameters
    String hostAddress = doc["data"]["hostAddress"].as<String>();
    int port = doc["data"]["port"] | 80; // Default to 80 if not specified
    String authToken = doc["data"]["authToken"].as<String>();

    if (systemType == "nanoleaf")
    {
        Serial.println("🍃 Configuring Nanoleaf lighting system...");

        // For Nanoleaf, use mDNS discovery if no valid host address is provided
        if (hostAddress.length() == 0 || hostAddress == "null" || hostAddress == "undefined")
        {
            Serial.println("🔍 No host address provided - using mDNS discovery for Nanoleaf");
            // Configure without host address - let the LightManager use mDNS discovery
            bool success = lightManager->configure(systemType, "", 0, authToken);

            if (success)
            {
                Serial.println("✅ Nanoleaf system configured successfully via mDNS discovery!");

                // Send pre-auth status update so frontend can prompt user
                {
                    JsonDocument statusDoc;
                    statusDoc["event"] = "lightingSystemStatus";
                    JsonObject data = statusDoc["data"].to<JsonObject>();
                    data["deviceId"] = deviceManager->getDeviceId();
                    data["systemType"] = systemType;
                    data["status"] = "authentication_required";
                    data["details"] = "Press the button on your Nanoleaf controller.";
                    data["lastTest"] = millis();
                    String msg;
                    serializeJson(statusDoc, msg);
                    Serial.println("📤 Sending pre-auth lighting status: " + msg);
                    sendMessage(msg);
                }

                // Immediately start authentication process (which includes mDNS discovery)
                Serial.println("🔐 Starting Nanoleaf mDNS discovery and authentication...");
                Serial.println("🔍 This process will:");
                Serial.println("   1. Initialize mDNS");
                Serial.println("   2. Search for Nanoleaf devices on network");
                Serial.println("   3. Test connectivity to found devices");
                Serial.println("   4. Attempt authentication (may require button press)");
                Serial.println("⏳ Please wait, this may take 30-60 seconds...");

                Serial.println("🔬 DEBUG: About to call lightManager->authenticateLightingSystem()");
                bool authResult = lightManager->authenticateLightingSystem();
                Serial.println("🔬 DEBUG: lightManager->authenticateLightingSystem() returned: " + String(authResult ? "true" : "false"));

                if (authResult)
                {
                    Serial.println("✅ Nanoleaf mDNS discovery and authentication completed successfully!");
                }
                else
                {
                    Serial.println("⚠ Nanoleaf discovery/authentication failed");
                    Serial.println("💡 This could mean:");
                    Serial.println("   - No Nanoleaf devices found on network");
                    Serial.println("   - Devices found but authentication failed");
                    Serial.println("   - User action required (press hold button on Nanoleaf)");
                    Serial.println("   - Network/mDNS configuration issue");
                }

                // Send status update after authentication attempt
                sendLightingSystemStatus();
            }
            else
            {
                Serial.println("❌ Failed to configure Nanoleaf system via mDNS discovery");
            }
        }
        else
        {
            // Use provided host address
            Serial.println("🌐 Host Address: " + hostAddress);
            Serial.println("🔌 Port: " + String(port));
            if (authToken.length() > 0)
            {
                Serial.println("🔑 Auth Token: [REDACTED]");
            }

            bool success = lightManager->configure(systemType, hostAddress, port, authToken);

            if (success)
            {
                Serial.println("✅ Nanoleaf system configured successfully!");

                // For Nanoleaf, immediately start authentication process (which includes mDNS discovery)
                Serial.println("🔐 Starting Nanoleaf authentication and discovery...");
                Serial.println("🔍 This process will validate connection and authenticate");
                Serial.println("⏳ Please wait, this may take 10-30 seconds...");

                if (lightManager->authenticateLightingSystem())
                {
                    Serial.println("✅ Nanoleaf authentication and discovery completed successfully!");
                }
                else
                {
                    Serial.println("⚠ Nanoleaf authentication failed");
                    Serial.println("💡 This could mean:");
                    Serial.println("   - Invalid host address or port");
                    Serial.println("   - Device not reachable on network");
                    Serial.println("   - User action required (press hold button on Nanoleaf)");
                    Serial.println("   - Invalid or expired auth token");
                }

                // Send status update after authentication attempt
                sendLightingSystemStatus();
            }
            else
            {
                Serial.println("❌ Failed to configure Nanoleaf system");
            }
        }
    }
    else if (systemType == "wled")
    {
        Serial.println("🌈 Configuring WLED lighting system...");

        Serial.println("🌐 Host Address: " + hostAddress);
        Serial.println("🔌 Port: " + String(port));

        // Configure WLED system
        bool success = lightManager->configure(systemType, hostAddress, port);

        if (success)
        {
            Serial.println("✅ WLED system configured successfully!");
        }
        else
        {
            Serial.println("❌ Failed to configure WLED system");
        }

        // Send status update after WLED configuration
        sendLightingSystemStatus();
    }
    else if (systemType == "ws2812")
    {
        Serial.println("💡 Configuring WS2812 lighting system...");

        // For WS2812, we might get pin and numLEDs in customConfig
        JsonObject customConfig = doc["data"]["customConfig"];
        int pin = customConfig["pin"] | DEFAULT_LED_PIN;
        int numLEDs = customConfig["numLEDs"] | DEFAULT_NUM_LEDS;

        Serial.println("📍 Pin: " + String(pin));
        Serial.println("💡 Number of LEDs: " + String(numLEDs));

        // Configure WS2812 system (host and port are not used for direct GPIO)
        bool success = lightManager->configure(systemType, "", 0, "", customConfig);

        if (success)
        {
            Serial.println("✅ WS2812 system configured successfully!");
        }
        else
        {
            Serial.println("❌ Failed to configure WS2812 system");
        }

        // Send status update after WS2812 configuration
        sendLightingSystemStatus();
    }
    else
    {
        Serial.println("❌ Unknown lighting system type: " + systemType);
    }

    Serial.println("⚡ ==============================\n");
}

void WSClient::handleTestLightingSystem(JsonDocument &doc)
{
    Serial.println("\n🧪 ===== LIGHTING SYSTEM TEST =====");

    if (!lightManager)
    {
        Serial.println("❌ LightManager not available");

        // Send failure response
        sendMessage("{\"event\":\"lightingSystemTest\",\"data\":{\"deviceId\":\"" +
                    deviceManager->getDeviceId() + "\",\"success\":false,\"error\":\"LightManager not available\"}}");
        return;
    }

    String deviceId = doc["data"]["deviceId"].as<String>();
    Serial.println("🔍 Testing lighting system for device: " + deviceId);

    // Test the lighting system connection
    bool testSuccess = lightManager->testConnection();

    if (testSuccess)
    {
        Serial.println("✅ Lighting system test passed!");

        // Optionally show a test pattern
        Serial.println("💡 Displaying test pattern...");

        // Create a simple test palette
        ColorPalette testPalette;
        testPalette.colorCount = 3;
        testPalette.colors[0] = RGBColor{255, 0, 0}; // Red
        testPalette.colors[1] = RGBColor{0, 255, 0}; // Green
        testPalette.colors[2] = RGBColor{0, 0, 255}; // Blue

        lightManager->displayPalette(testPalette);

        // Send success response
        sendMessage("{\"event\":\"lightingSystemTest\",\"data\":{\"deviceId\":\"" +
                    deviceId + "\",\"success\":true}}");
    }
    else
    {
        Serial.println("❌ Lighting system test failed!");

        // Send failure response
        sendMessage("{\"event\":\"lightingSystemTest\",\"data\":{\"deviceId\":\"" +
                    deviceId + "\",\"success\":false,\"error\":\"Connection test failed\"}}");
    }

    Serial.println("🧪 ==============================\n");
}

void WSClient::displayColorPaletteSerial()
{
    Serial.println("💡 [LED SIMULATION] Displaying colors on light strip:");

    // Create a visual representation using ASCII
    Serial.print("   Strip: ");
    for (int i = 0; i < currentPalette.colorCount; i++)
    {
        String hexColor = LightControllerUtils::colorToHex(currentPalette.colors[i]);
        Serial.print("[" + hexColor + "]");
        if (i < currentPalette.colorCount - 1)
        {
            Serial.print("-");
        }
    }
    Serial.println();

    // Show RGB values
    Serial.println("   RGB Values:");
    for (int i = 0; i < currentPalette.colorCount; i++)
    {
        RGBColor color = currentPalette.colors[i];
        Serial.println("   Color " + String(i + 1) + ": RGB(" + String(color.r) + ", " + String(color.g) + ", " + String(color.b) + ")");
    }

    Serial.println("   💡 Colors displayed for demonstration");
    Serial.println("   🔧 In production, this would control physical LEDs");
}

void WSClient::setLightManager(LightManager *lightMgr)
{
    lightManager = lightMgr;
    Serial.println("💡 Light Manager connected to WebSocket client");

    // Set up user notification callback
    if (lightManager)
    {
        lightManager->setUserNotificationCallback([this](const String &action, const String &instructions, int timeout)
                                                  { handleUserNotification(action, instructions, timeout); });
    }
}

void WSClient::handleUserNotification(const String &action, const String &instructions, int timeout)
{
    Serial.println("🔔 Handling user notification: " + action);

    // Send notification to backend/mobile app via WebSocket
    if (isClientConnected())
    {
        JsonDocument notification;
        notification["event"] = "userActionRequired";
        notification["data"]["deviceId"] = deviceManager->getDeviceId();
        notification["data"]["action"] = action;
        notification["data"]["instructions"] = instructions;
        notification["data"]["timeout"] = timeout;
        notification["data"]["timestamp"] = millis();

        // Add more context for nanoleaf pairing
        if (action == "nanoleaf_pairing")
        {
            notification["data"]["type"] = "lighting_authentication";
            notification["data"]["systemType"] = "nanoleaf";
            notification["data"]["displayMessage"] = "Nanoleaf Authentication Required";
        }

        String message;
        serializeJson(notification, message);

        Serial.println("📤 Sending user notification to backend: " + message);
        sendMessage(message);
    }
    else
    {
        Serial.println("⚠ WebSocket not connected - cannot send user notification");
        // TODO: Could implement fallback methods here (e.g., temporary AP mode)
    }
}

void WSClient::displayColorPaletteOnLights()
{
    if (!lightManager || !lightManager->isReady())
    {
        Serial.println("⚠ No lighting system available, skipping physical display");
        return;
    }

    Serial.println("💡 Displaying palette on physical lighting system...");

    // currentPalette is already in the new ColorPalette format
    // Display on lights
    if (lightManager->displayPalette(currentPalette))
    {
        Serial.println("✅ Palette successfully displayed on lights");
    }
    else
    {
        Serial.println("❌ Failed to display palette on lights");
    }
}

::ColorPalette WSClient::convertToLightPalette(const LegacyColorPalette &legacyPalette)
{
    ColorPalette lightPalette;

    // Copy metadata
    lightPalette.name = "From " + legacyPalette.senderName;
    lightPalette.messageId = legacyPalette.messageId;
    lightPalette.senderName = legacyPalette.senderName;
    lightPalette.colorCount = min(legacyPalette.colorCount, MAX_COLORS);

    // Convert colors from hex strings to RGBColor structs
    for (int i = 0; i < lightPalette.colorCount; i++)
    {
        String hexColor = legacyPalette.colors[i];
        lightPalette.colors[i] = LightControllerUtils::hexToColor(hexColor);
    }

    return lightPalette;
}

bool WSClient::retryLightingAuthentication()
{
    if (!lightManager)
    {
        Serial.println("❌ No light manager available");
        return false;
    }

    if (!deviceManager->isProvisioned())
    {
        Serial.println("❌ Device must be paired with a user before lighting authentication");
        return false;
    }

    Serial.println("🔄 Retrying lighting system authentication...");

    if (lightManager->authenticateLightingSystem())
    {
        Serial.println("✅ Lighting authentication retry successful");

        // Send updated status after successful authentication
        sendLightingSystemStatus();

        return true;
    }
    else
    {
        Serial.println("❌ Lighting authentication retry failed");

        // Send status update to show failure
        sendLightingSystemStatus();

        return false;
    }
}

void WSClient::sendLightingSystemStatus()
{
    if (!isClientConnected() || !lightManager)
    {
        Serial.println("⚠ Cannot send lighting status - WebSocket not connected or no light manager");
        return;
    }

    // Get lighting system status/config for backend schema
    String systemType = lightManager->getCurrentSystemType();
    if (systemType.length() == 0 || systemType == "none")
    {
        Serial.println("📋 Skipping lighting system status - no lighting system configured yet");
        return;
    }

    Serial.println("📊 Sending lighting system status update...");

    JsonDocument statusDoc;
    statusDoc["event"] = "lightingSystemStatus";
    JsonObject data = statusDoc["data"].to<JsonObject>();
    data["deviceId"] = deviceManager->getDeviceId();
    data["systemType"] = systemType;

    // Status: working, authentication_required, error, unknown
    String status = "unknown";
    String details = "";
    if (lightManager->isReady())
    {
        status = "working";
    }
    else if (lightManager->requiresUserAuthentication())
    {
        status = "authentication_required";
        details = "User action required for authentication.";
    }
    else
    {
        String s = lightManager->getStatus();
        if (s == "error" || s == "Error" || s == "failed")
        {
            status = "error";
            details = s;
        }
        else
        {
            status = s.length() > 0 ? s : "unknown";
        }
    }
    data["status"] = status;
    if (details.length() > 0)
    {
        data["details"] = details;
    }

    // lastTest: use millis() as a placeholder, or get from LightManager if available
    data["lastTest"] = millis();

    String message;
    serializeJson(statusDoc, message);

    Serial.println("📤 Sending lighting status: " + message);
    sendMessage(message);
}

void WSClient::sendDeviceStatus()
{
    if (!isClientConnected())
    {
        Serial.println("⚠ Cannot send device status - WebSocket not connected");
        return;
    }

    Serial.println("📊 Sending device status update...");

    DeviceInfo deviceInfo = deviceManager->getDeviceInfo();

    JsonDocument statusDoc;
    statusDoc["event"] = "deviceStatus";
    statusDoc["data"]["deviceId"] = deviceInfo.deviceId;
    statusDoc["data"]["timestamp"] = millis();
    statusDoc["data"]["isOnline"] = true;
    statusDoc["data"]["isProvisioned"] = deviceInfo.isProvisioned;
    statusDoc["data"]["firmwareVersion"] = deviceInfo.firmwareVersion;
    statusDoc["data"]["ipAddress"] = WiFi.localIP().toString();
    statusDoc["data"]["macAddress"] = deviceInfo.macAddress;
    statusDoc["data"]["wifiRSSI"] = WiFi.RSSI();
    statusDoc["data"]["freeHeap"] = ESP.getFreeHeap();
    statusDoc["data"]["uptime"] = millis() / 1000;

    String message;
    serializeJson(statusDoc, message);

    Serial.println("📤 Sending device status: " + message);
    sendMessage(message);
}

void WSClient::handleFactoryReset(JsonDocument &doc)
{
    Serial.println("🔄 Factory reset command received via WebSocket");

    // Send acknowledgment back to backend
    if (isClientConnected())
    {
        JsonDocument response;
        response["event"] = "factoryResetAcknowledged";
        response["data"]["deviceId"] = deviceManager->getDeviceId();
        response["data"]["timestamp"] = millis();

        String message;
        serializeJson(response, message);
        sendMessage(message);

        Serial.println("📤 Sent factory reset acknowledgment");
    }

    // Give a moment for the message to be sent
    delay(500);

    // Perform the actual factory reset
    if (deviceManager)
    {
        deviceManager->resetDevice();
    }

    // Also reset lighting system configuration (Nanoleaf auth tokens, etc.)
    if (lightManager)
    {
        Serial.println("🔄 Resetting lighting system configuration...");
        lightManager->resetConfiguration();
    }

    // Reset will restart the device, so this code won't be reached
    Serial.println("🔄 Factory reset initiated, device will restart...");
}
