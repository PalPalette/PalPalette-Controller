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
#include <esp_task_wdt.h>
#include "config.h"
#include "core/WiFiManager.h"
#include "core/DeviceManager.h"
#include "core/WSClient.h"
#include "lighting/LightManager.h"

// Error Handling System
class ErrorHandler
{
private:
    static ErrorHandler *instance;
    ErrorCode lastError;
    uint8_t errorCounts[static_cast<uint8_t>(ErrorCode::UNKNOWN_ERROR) + 1];
    uint8_t totalErrorCount;
    unsigned long lastErrorTime;

public:
    static ErrorHandler *getInstance()
    {
        if (!instance)
        {
            instance = new ErrorHandler();
            if (!instance)
            {
                // Critical failure - cannot allocate error handler
                Serial.println("💀 CRITICAL: Failed to allocate ErrorHandler - system cannot continue");
                ESP.restart();
            }
        }
        return instance;
    }

    ErrorHandler() : lastError(ErrorCode::NONE), totalErrorCount(0), lastErrorTime(0)
    {
        memset(errorCounts, 0, sizeof(errorCounts));
    }

    void reportError(ErrorCode code, const String &message = "", const String &location = "")
    {
        lastError = code;
        lastErrorTime = millis();
        totalErrorCount++;
        errorCounts[static_cast<uint8_t>(code)]++;

        String errorMsg = "❌ ERROR [" + String(static_cast<uint8_t>(code)) + "] " + getErrorName(code);
        if (message.length() > 0)
        {
            errorMsg += ": " + message;
        }
        if (location.length() > 0)
        {
            errorMsg += " (at: " + location + ")";
        }

        Serial.println(errorMsg);
        Serial.println("📊 Total errors: " + String(totalErrorCount) +
                       ", This error count: " + String(errorCounts[static_cast<uint8_t>(code)]));
    }

    RecoveryStrategy getRecoveryStrategy(ErrorCode code)
    {
        uint8_t count = errorCounts[static_cast<uint8_t>(code)];

        // Progressive recovery based on error frequency
        switch (code)
        {
        case ErrorCode::WIFI_CONNECTION_FAILED:
            return (count < 3) ? RecoveryStrategy::RETRY_OPERATION : RecoveryStrategy::RESTART_COMPONENT;

        case ErrorCode::WEBSOCKET_CONNECTION_FAILED:
            return (count < 5) ? RecoveryStrategy::RETRY_OPERATION : RecoveryStrategy::RESTART_COMPONENT;

        case ErrorCode::MEMORY_ALLOCATION_FAILED:
            return RecoveryStrategy::SOFT_RESTART;

        case ErrorCode::DEVICE_REGISTRATION_FAILED:
            return (count < 3) ? RecoveryStrategy::RETRY_OPERATION : RecoveryStrategy::SOFT_RESTART;

        case ErrorCode::LIGHTING_SYSTEM_FAILED:
            return RecoveryStrategy::RESTART_COMPONENT;

        default:
            if (totalErrorCount > CRITICAL_ERROR_THRESHOLD)
            {
                return RecoveryStrategy::HARD_RESTART;
            }
            return (count < MAX_ERROR_RETRIES) ? RecoveryStrategy::RETRY_OPERATION : RecoveryStrategy::RESTART_COMPONENT;
        }
    }

    String getErrorName(ErrorCode code)
    {
        switch (code)
        {
        case ErrorCode::NONE:
            return "NONE";
        case ErrorCode::WIFI_CONNECTION_FAILED:
            return "WIFI_CONNECTION_FAILED";
        case ErrorCode::DEVICE_REGISTRATION_FAILED:
            return "DEVICE_REGISTRATION_FAILED";
        case ErrorCode::WEBSOCKET_CONNECTION_FAILED:
            return "WEBSOCKET_CONNECTION_FAILED";
        case ErrorCode::MEMORY_ALLOCATION_FAILED:
            return "MEMORY_ALLOCATION_FAILED";
        case ErrorCode::LIGHTING_SYSTEM_FAILED:
            return "LIGHTING_SYSTEM_FAILED";
        case ErrorCode::PREFERENCES_ACCESS_FAILED:
            return "PREFERENCES_ACCESS_FAILED";
        case ErrorCode::HTTP_REQUEST_FAILED:
            return "HTTP_REQUEST_FAILED";
        case ErrorCode::JSON_PARSING_FAILED:
            return "JSON_PARSING_FAILED";
        case ErrorCode::WATCHDOG_INITIALIZATION_FAILED:
            return "WATCHDOG_INITIALIZATION_FAILED";
        case ErrorCode::CAPTIVE_PORTAL_FAILED:
            return "CAPTIVE_PORTAL_FAILED";
        default:
            return "UNKNOWN_ERROR";
        }
    }

    bool shouldPerformRecovery()
    {
        return (millis() - lastErrorTime > ERROR_RECOVERY_DELAY);
    }

    ErrorCode getLastError() { return lastError; }
    uint8_t getTotalErrorCount() { return totalErrorCount; }
    void clearError() { lastError = ErrorCode::NONE; }
};

ErrorHandler *ErrorHandler::instance = nullptr;

// Exponential Backoff System for Network Retries
class ExponentialBackoff
{
private:
    unsigned long initialDelay;
    unsigned long maxDelay;
    unsigned long currentDelay;
    unsigned int multiplier;
    unsigned long lastAttemptTime;
    int attemptCount;

public:
    ExponentialBackoff(unsigned long initial = INITIAL_RETRY_DELAY,
                       unsigned long maximum = MAX_RETRY_DELAY,
                       unsigned int mult = BACKOFF_MULTIPLIER)
        : initialDelay(initial), maxDelay(maximum), multiplier(mult),
          currentDelay(initial), lastAttemptTime(0), attemptCount(0) {}

    bool shouldRetry()
    {
        unsigned long now = millis();
        if (now - lastAttemptTime >= currentDelay)
        {
            return true;
        }
        return false;
    }

    void recordAttempt()
    {
        lastAttemptTime = millis();
        attemptCount++;

        // Calculate next delay with exponential backoff
        currentDelay = min(currentDelay * multiplier, maxDelay);

        Serial.println("📡 Network attempt #" + String(attemptCount) +
                       ", next retry in " + String(currentDelay / 1000) + " seconds");
    }

    void reset()
    {
        currentDelay = initialDelay;
        lastAttemptTime = 0;
        attemptCount = 0;
    }

    int getAttemptCount() { return attemptCount; }
    unsigned long getCurrentDelay() { return currentDelay; }
};

// Global objects
WiFiManager wifiManager;
DeviceManager deviceManager;
LightManager lightManager;
WSClient *wsClient = nullptr;

// Network retry backoff instances
ExponentialBackoff wifiRetryBackoff(2000, 30000, 2);    // WiFi: 2s -> 4s -> 8s -> 16s -> 30s
ExponentialBackoff wsRetryBackoff(1000, 60000, 2);      // WebSocket: 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 60s
ExponentialBackoff registrationBackoff(3000, 45000, 2); // Registration: 3s -> 6s -> 12s -> 24s -> 45s

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
unsigned long lastWatchdogFeed = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000; // 10 seconds

// Watchdog timer variables
bool watchdogInitialized = false;

// Helper function for repeating strings
String repeatString(const String &str, int count)
{
    String result = "";
    result.reserve(str.length() * count); // Pre-allocate memory to avoid fragmentation
    for (int i = 0; i < count; i++)
    {
        result += str;
    }
    return result;
}

// Memory health checking function
bool isMemoryHealthy()
{
    size_t freeHeap = ESP.getFreeHeap();
    const size_t MIN_SAFE_HEAP = 10000; // 10KB minimum for safe operation

    if (freeHeap < MIN_SAFE_HEAP)
    {
        Serial.println("⚠️ LOW MEMORY WARNING: " + String(freeHeap) + " bytes free (minimum: " + String(MIN_SAFE_HEAP) + ")");
        return false;
    }
    return true;
}

// Global watchdog feeding function - can be called from anywhere
void globalFeedWatchdog()
{
    if (watchdogInitialized)
    {
        esp_task_wdt_reset();
        // Optional: Add yield() to let other tasks run
        yield();
    }
}

// Global cleanup function for emergency shutdowns and resets
void performGlobalCleanup()
{
    Serial.println("🧹 Performing global system cleanup...");

    // Disable watchdog to prevent reset during cleanup
    disableWatchdog();

    // Clean up WebSocket client
    if (wsClient != nullptr)
    {
        Serial.println("  - Cleaning up WebSocket client");
        delete wsClient;
        wsClient = nullptr;
    }

    // Stop WiFi AP mode if active
    if (wifiManager.isInAPMode())
    {
        Serial.println("  - Stopping WiFi AP mode");
        wifiManager.stopAPMode();
    }

    // Disconnect WiFi
    if (wifiManager.isConnected())
    {
        Serial.println("  - Disconnecting WiFi");
        WiFi.disconnect(true);
    }

    Serial.println("✅ Global cleanup completed");
} // Watchdog Timer Management Functions
bool initializeWatchdog()
{
    Serial.println("🐕 Initializing watchdog timer...");

    // Configure watchdog timer
    esp_err_t result = esp_task_wdt_init(WATCHDOG_TIMEOUT / 1000, true); // Convert to seconds

    if (result != ESP_OK)
    {
        Serial.println("❌ Failed to initialize watchdog timer: " + String(result));
        return false;
    }

    // Add current task to watchdog
    result = esp_task_wdt_add(NULL);
    if (result != ESP_OK)
    {
        Serial.println("❌ Failed to add task to watchdog: " + String(result));
        return false;
    }

    watchdogInitialized = true;
    lastWatchdogFeed = millis();

    Serial.println("✅ Watchdog timer initialized successfully");
    Serial.println("🐕 Timeout: " + String(WATCHDOG_TIMEOUT) + "ms, Feed interval: " + String(WATCHDOG_FEED_INTERVAL) + "ms");

    return true;
}

void feedWatchdog()
{
    if (!watchdogInitialized)
    {
        return;
    }

    unsigned long currentTime = millis();
    if (currentTime - lastWatchdogFeed >= WATCHDOG_FEED_INTERVAL)
    {
        esp_task_wdt_reset();
        lastWatchdogFeed = currentTime;

        // Only log occasionally to avoid spam
        static unsigned long lastWatchdogLog = 0;
        if (currentTime - lastWatchdogLog > 30000) // Log every 30 seconds
        {
            Serial.println("🐕 Watchdog fed (system healthy)");
            lastWatchdogLog = currentTime;
        }
    }
}

void disableWatchdog()
{
    if (watchdogInitialized)
    {
        Serial.println("🐕 Disabling watchdog timer for cleanup...");
        esp_task_wdt_delete(NULL);
        esp_task_wdt_deinit();
        watchdogInitialized = false;
    }
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

    // Initialize watchdog timer early in setup
    if (!initializeWatchdog())
    {
        ErrorHandler::getInstance()->reportError(ErrorCode::WATCHDOG_INITIALIZATION_FAILED,
                                                 "Watchdog timer initialization failed",
                                                 "setup");
        Serial.println("⚠ Continuing without watchdog protection");
    }

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

// Get optimal loop delay based on current device state
unsigned long getOptimalLoopDelay()
{
    switch (currentState)
    {
    case STATE_INIT:
        return 50; // Fast initialization

    case STATE_WIFI_SETUP:
        return 100; // Moderate for captive portal handling

    case STATE_WIFI_CONNECTING:
        return 500; // Slower during connection attempts

    case STATE_DEVICE_REGISTRATION:
        return 250; // Moderate for registration process

    case STATE_WAITING_FOR_CLAIM:
        return 1000; // Slow when just waiting

    case STATE_OPERATIONAL:
        return 200; // Moderate for normal operation

    case STATE_ERROR:
        return 2000; // Very slow during error recovery

    default:
        return 100; // Safe default
    }
}

void loop()
{
    // Feed watchdog timer to prevent resets
    feedWatchdog();

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

    // Allow other tasks to run and prevent blocking
    yield();

    // Dynamic delay based on current state to optimize performance
    unsigned long loopDelay = getOptimalLoopDelay();
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
    static bool attemptInProgress = false;

    // Start first attempt immediately
    if (!attemptInProgress)
    {
        connectStartTime = millis();
        attemptInProgress = true;
        wifiRetryBackoff.recordAttempt();
        Serial.println("📶 Attempting WiFi connection (attempt #" + String(wifiRetryBackoff.getAttemptCount()) + ")...");
    }

    if (wifiManager.connectToWiFi())
    {
        // Success! Reset backoff and proceed
        wifiRetryBackoff.reset();
        attemptInProgress = false;

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

        setState(STATE_DEVICE_REGISTRATION);
    }
    else
    {
        // Check if we should retry with exponential backoff
        if (wifiRetryBackoff.shouldRetry())
        {
            // Too many failed attempts?
            if (wifiRetryBackoff.getAttemptCount() >= MAX_WIFI_RETRY_ATTEMPTS)
            {
                ErrorHandler::getInstance()->reportError(ErrorCode::WIFI_CONNECTION_FAILED,
                                                         "Max WiFi retry attempts reached (" + String(MAX_WIFI_RETRY_ATTEMPTS) + ")",
                                                         "handleWiFiConnecting");
                wifiRetryBackoff.reset();
                attemptInProgress = false;
                setState(STATE_ERROR);
                return;
            }

            // Try again
            attemptInProgress = false;
        }

        // Also check for overall timeout
        if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT)
        {
            ErrorHandler::getInstance()->reportError(ErrorCode::WIFI_CONNECTION_FAILED,
                                                     "WiFi connection timeout after " + String(WIFI_CONNECT_TIMEOUT / 1000) + " seconds",
                                                     "handleWiFiConnecting");
            wifiRetryBackoff.reset();
            attemptInProgress = false;
            setState(STATE_ERROR);
        }
    }
}

void handleDeviceRegistration()
{
    static bool registrationAttempted = false;
    static bool registrationSuccessful = false;

    // If registration was already successful, don't retry
    if (registrationSuccessful)
    {
        return;
    }

    if (!registrationAttempted)
    {
        Serial.println("📡 Starting minimal device registration process...");

        // First perform minimal registration with HTTP API (only MAC address)
        String serverUrl = wifiManager.getServerURL();
        if (deviceManager.registerMinimalWithServer(serverUrl))
        {
            Serial.println("✅ Device registered minimally with HTTP API");

            // Initialize WebSocket client with proper cleanup
            if (wsClient != nullptr)
            {
                Serial.println("🔄 Cleaning up existing WebSocket client");
                delete wsClient;
                wsClient = nullptr;
            }

            // Check memory health before allocation
            if (!isMemoryHealthy())
            {
                ErrorHandler::getInstance()->reportError(ErrorCode::MEMORY_ALLOCATION_FAILED,
                                                         "Insufficient memory for WebSocket client allocation",
                                                         "handleDeviceRegistration");
                setState(STATE_ERROR);
                return;
            }

            wsClient = new WSClient(&deviceManager, &lightManager);

            // Check allocation success
            if (wsClient == nullptr)
            {
                ErrorHandler::getInstance()->reportError(ErrorCode::MEMORY_ALLOCATION_FAILED,
                                                         "Failed to allocate WebSocket client",
                                                         "handleDeviceRegistration");
                setState(STATE_ERROR);
                return;
            }

            wsClient->begin(serverUrl);

            // Attempt WebSocket connection
            if (wsClient->connect())
            {
                Serial.println("✅ WebSocket connection established");
                registrationSuccessful = true; // Mark as successful

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
                ErrorHandler::getInstance()->reportError(ErrorCode::WEBSOCKET_CONNECTION_FAILED,
                                                         "WebSocket connection failed after successful device registration",
                                                         "handleDeviceRegistration");
                setState(STATE_ERROR);
            }
        }
        else
        {
            ErrorHandler::getInstance()->reportError(ErrorCode::DEVICE_REGISTRATION_FAILED,
                                                     "HTTP registration with server failed",
                                                     "handleDeviceRegistration");
            setState(STATE_ERROR);
        }

        registrationAttempted = true;
    }

    // Only reset registration attempt flag after delay if it failed (not successful)
    if (!registrationSuccessful && millis() - stateChangeTime > REGISTRATION_RETRY_INTERVAL)
    {
        Serial.println("⏰ Retrying device registration after failure...");
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
    ErrorHandler *errorHandler = ErrorHandler::getInstance();

    // Only process errors if enough time has passed since last error handling
    if (!errorHandler->shouldPerformRecovery())
    {
        return;
    }

    ErrorCode lastError = errorHandler->getLastError();
    RecoveryStrategy strategy = errorHandler->getRecoveryStrategy(lastError);

    Serial.println("🔧 Executing recovery strategy: " + String(static_cast<uint8_t>(strategy)));

    switch (strategy)
    {
    case RecoveryStrategy::RETRY_OPERATION:
        Serial.println("🔄 Retrying operation...");
        // Try to recover based on current state
        if (currentState == STATE_WIFI_CONNECTING)
        {
            setState(STATE_WIFI_SETUP);
        }
        else if (currentState == STATE_DEVICE_REGISTRATION)
        {
            setState(STATE_WIFI_CONNECTING);
        }
        else
        {
            setState(STATE_WIFI_SETUP);
        }
        break;

    case RecoveryStrategy::RESTART_COMPONENT:
        Serial.println("🔄 Restarting affected component...");
        if (lastError == ErrorCode::WIFI_CONNECTION_FAILED)
        {
            wifiManager.stopAPMode();
            setState(STATE_WIFI_SETUP);
        }
        else if (lastError == ErrorCode::WEBSOCKET_CONNECTION_FAILED)
        {
            if (wsClient != nullptr)
            {
                delete wsClient;
                wsClient = nullptr;
            }
            setState(STATE_DEVICE_REGISTRATION);
        }
        else
        {
            setState(STATE_WIFI_SETUP);
        }
        break;

    case RecoveryStrategy::SOFT_RESTART:
        Serial.println("🔄 Performing soft restart...");
        performGlobalCleanup();
        delay(2000);
        ESP.restart();
        break;

    case RecoveryStrategy::HARD_RESTART:
        Serial.println("💀 Critical errors detected - performing hard restart with factory reset");
        errorHandler->reportError(ErrorCode::UNKNOWN_ERROR, "Too many critical errors", "handleError");
        performGlobalCleanup();
        deviceManager.resetDevice();
        wifiManager.clearWiFiCredentials();
        delay(3000);
        ESP.restart();
        break;

    case RecoveryStrategy::FACTORY_RESET:
        Serial.println("🏭 Performing factory reset...");
        performGlobalCleanup();
        deviceManager.resetDevice();
        wifiManager.clearWiFiCredentials();
        delay(3000);
        ESP.restart();
        break;

    default:
        Serial.println("⚠ Unknown recovery strategy, defaulting to soft restart");
        setState(STATE_WIFI_SETUP);
        break;
    }

    errorHandler->clearError();
}

void handlePeriodicTasks()
{
    // Check WiFi connection periodically
    if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL)
    {
        if (currentState >= STATE_DEVICE_REGISTRATION && !wifiManager.isConnected())
        {
            ErrorHandler::getInstance()->reportError(ErrorCode::WIFI_CONNECTION_FAILED,
                                                     "WiFi connection lost during operation",
                                                     "handlePeriodicTasks");
            setState(STATE_ERROR);
        }
        lastWiFiCheck = millis();
    }

    // Update device status periodically (if registered and connected)
    // Add grace period after registration to allow backend to process
    const unsigned long REGISTRATION_GRACE_PERIOD = 10000; // 10 seconds
    bool pastGracePeriod = (currentState > STATE_DEVICE_REGISTRATION) ||
                           (millis() - stateChangeTime > REGISTRATION_GRACE_PERIOD);

    if (currentState >= STATE_DEVICE_REGISTRATION && pastGracePeriod && deviceManager.shouldUpdateStatus())
    {
        if (wifiManager.isConnected())
        {
            String serverUrl = wifiManager.getServerURL();
            if (deviceManager.updateStatus(serverUrl))
            {
                Serial.println("📊 Device status updated successfully");
            }
            else
            {
                Serial.println("⚠ Device status update failed - backend may not be ready yet");
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
            Serial.println("🔄 Resetting device with proper cleanup...");
            performGlobalCleanup();
            deviceManager.resetDevice();
            wifiManager.clearWiFiCredentials();
            delay(1000); // Give time for cleanup
            ESP.restart();
        }
        else if (command == "restart")
        {
            Serial.println("🔄 Restarting device with cleanup...");
            performGlobalCleanup();
            delay(1000); // Give time for cleanup
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
