# PalPalette ESP32 Firmware Documentation

## Overview

The PalPalette ESP32 firmware is a modular Arduino-based system that enables ESP32 devices to self-register with the backend, receive color palettes via WebSocket, and display them on WS2812B LED strips. The firmware implements a state machine architecture for reliable device lifecycle management.

## Architecture

### Technology Stack

- **Platform**: ESP32 microcontroller
- **Framework**: Arduino Core for ESP32
- **Language**: C++ with Arduino libraries
- **Communication**: WiFi + WebSocket
- **Storage**: ESP32 Preferences (EEPROM)
- **LED Control**: WS2812B addressable LEDs

### Hardware Requirements

- **ESP32 Development Board** (ESP32-WROOM-32 recommended)
- **WS2812B LED Strip** (30-144 LEDs supported)
- **Power Supply** (5V, adequate amperage for LED count)
- **Pull-up Resistor** (10kΩ for button, optional)

### Wiring Diagram

```
ESP32 Pin Configuration:
├── GPIO 2  → LED Strip Data Pin (WS2812B)
├── GPIO 0  → Button (optional, with 10kΩ pull-up)
├── 3.3V    → LED Strip VCC (for small strips)
├── GND     → LED Strip GND + Button GND
└── USB     → Programming and power
```

## Modular Architecture

### File Structure

```
controller/src/
├── main.ino           # Main application entry point
├── config.h           # Configuration constants
├── WiFiManager.h      # WiFi connection management
├── WiFiManager.cpp    # WiFi implementation
├── DeviceManager.h    # Device registration and pairing
├── DeviceManager.cpp  # Device implementation
├── WSClient.h         # WebSocket client communication
├── WSClient.cpp       # WebSocket implementation
└── LEDController.h    # LED strip control (optional)
```

### Core Modules

#### 1. Main Application (`main.ino`)

Central coordinator implementing a 7-state state machine.

**State Machine States:**

1. `BOOT` - Initialize hardware and load configuration
2. `WIFI_SETUP` - Connect to WiFi or start captive portal
3. `DEVICE_REGISTRATION` - Register device with backend
4. `PAIRING_MODE` - Wait for user to claim device
5. `OPERATIONAL` - Normal operation, ready for colors
6. `ERROR` - Handle errors and recovery
7. `DEEP_SLEEP` - Low power mode (planned)

```cpp
typedef enum {
  STATE_BOOT,
  STATE_WIFI_SETUP,
  STATE_DEVICE_REGISTRATION,
  STATE_PAIRING_MODE,
  STATE_OPERATIONAL,
  STATE_ERROR,
  STATE_DEEP_SLEEP
} DeviceState;
```

#### 2. WiFi Manager (`WiFiManager.h/.cpp`)

Handles WiFi connection with captive portal fallback.

**Key Features:**

- Automatic connection to saved WiFi networks
- Captive portal for WiFi configuration
- Connection retry logic with backoff
- WiFi status monitoring and reconnection

**Public Interface:**

```cpp
class WiFiManager {
public:
  void begin();
  bool connectToWiFi();
  bool isConnected();
  void startCaptivePortal();
  void handleCaptivePortal();
  String getLocalIP();
  int getSignalStrength();
};
```

#### 3. Device Manager (`DeviceManager.h/.cpp`)

Manages device registration, pairing, and persistent state.

**Key Features:**

- Device self-registration via MAC address
- Pairing code generation and management
- Persistent device state in EEPROM
- Owner information management

**Public Interface:**

```cpp
class DeviceManager {
public:
  void begin();
  bool registerDevice();
  String getPairingCode();
  bool isProvisioned();
  void markAsProvisioned(String ownerId, String ownerName);
  void resetToFactory();
  DeviceInfo getDeviceInfo();
};

struct DeviceInfo {
  String deviceId;
  String macAddress;
  String pairingCode;
  bool isProvisioned;
  String ownerId;
  String ownerName;
};
```

#### 4. WebSocket Client (`WSClient.h/.cpp`)

Real-time communication with backend server.

**Key Features:**

- WebSocket connection management
- Message parsing and handling
- Automatic reconnection logic
- Device status reporting

**Public Interface:**

```cpp
class WSClient {
public:
  void begin(String host, int port);
  bool connect();
  bool isConnected();
  void loop();
  void sendMessage(String message);
  void onMessage(std::function<void(String)> callback);
  void onEvent(std::function<void(WStype_t, uint8_t*)> callback);
};
```

#### 5. Configuration (`config.h`)

Centralized configuration constants.

```cpp
#ifndef CONFIG_H
#define CONFIG_H

// Network Configuration
#define API_BASE_URL "http://192.168.1.100:3000"
#define WS_HOST "192.168.1.100"
#define WS_PORT 3001
#define USE_SSL false

// Hardware Configuration
#define LED_PIN 2
#define LED_COUNT 30
#define BUTTON_PIN 0

// WiFi Configuration
#define WIFI_CONNECT_TIMEOUT 20000
#define WIFI_RETRY_DELAY 5000
#define CAPTIVE_PORTAL_TIMEOUT 300000  // 5 minutes

// Device Configuration
#define DEVICE_NAME "PalPalette Device"
#define PAIRING_CODE_LENGTH 6
#define HEARTBEAT_INTERVAL 30000

// Debug Configuration
#define DEBUG_SERIAL true
#define SERIAL_BAUD 115200

#endif
```

## State Machine Implementation

### State Transitions

```
BOOT → WIFI_SETUP → DEVICE_REGISTRATION → PAIRING_MODE → OPERATIONAL
  ↓         ↓              ↓                  ↓             ↓
ERROR ← ERROR ←     ERROR ←           ERROR ← ERROR
  ↓
WIFI_SETUP (retry)
```

### Main Loop Logic

```cpp
void loop() {
  static unsigned long lastStateTime = 0;
  unsigned long currentTime = millis();

  // Handle state machine
  handleStateMachine();

  // Handle WiFi in all states
  if (wifiManager.isConnected()) {
    wsClient.loop();  // Process WebSocket messages
  } else {
    // Attempt reconnection if needed
    if (currentTime - lastStateTime > WIFI_RETRY_DELAY) {
      currentState = STATE_WIFI_SETUP;
    }
  }

  // Handle button input (optional)
  handleButton();

  // Update LEDs based on state
  updateStatusLEDs();

  delay(100);  // Prevent watchdog timeout
}
```

### State Handlers

```cpp
void handleStateMachine() {
  switch (currentState) {
    case STATE_BOOT:
      handleBootState();
      break;
    case STATE_WIFI_SETUP:
      handleWiFiSetupState();
      break;
    case STATE_DEVICE_REGISTRATION:
      handleDeviceRegistrationState();
      break;
    case STATE_PAIRING_MODE:
      handlePairingModeState();
      break;
    case STATE_OPERATIONAL:
      handleOperationalState();
      break;
    case STATE_ERROR:
      handleErrorState();
      break;
  }
}
```

## Device Lifecycle

### 1. Initial Boot Sequence

```cpp
void handleBootState() {
  Serial.println("=== PalPalette Device Starting ===");

  // Initialize hardware
  pinMode(LED_PIN, OUTPUT);
  if (BUTTON_PIN >= 0) {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
  }

  // Initialize LED strip
  strip.begin();
  strip.clear();
  strip.show();

  // Initialize managers
  wifiManager.begin();
  deviceManager.begin();

  // Show boot animation
  showBootAnimation();

  currentState = STATE_WIFI_SETUP;
}
```

### 2. WiFi Connection

```cpp
void handleWiFiSetupState() {
  if (!wifiManager.isConnected()) {
    Serial.println("Connecting to WiFi...");

    if (wifiManager.connectToWiFi()) {
      Serial.println("WiFi connected!");
      Serial.println("IP: " + wifiManager.getLocalIP());
      currentState = STATE_DEVICE_REGISTRATION;
    } else {
      Serial.println("WiFi connection failed, starting captive portal");
      wifiManager.startCaptivePortal();
      // Stay in this state until WiFi is configured
    }
  } else {
    currentState = STATE_DEVICE_REGISTRATION;
  }
}
```

### 3. Device Registration

```cpp
void handleDeviceRegistrationState() {
  if (deviceManager.isProvisioned()) {
    // Device already claimed, skip to operational
    Serial.println("Device already provisioned");
    currentState = STATE_OPERATIONAL;
    return;
  }

  if (deviceManager.registerDevice()) {
    Serial.println("Device registered successfully");
    Serial.println("Pairing Code: " + deviceManager.getPairingCode());
    currentState = STATE_PAIRING_MODE;
  } else {
    Serial.println("Device registration failed");
    currentState = STATE_ERROR;
  }
}
```

### 4. Pairing Mode

```cpp
void handlePairingModeState() {
  // Show pairing code on LEDs (optional)
  showPairingCode(deviceManager.getPairingCode());

  // Wait for WebSocket notification that device was claimed
  // This is handled in WebSocket message callback

  // Timeout after 15 minutes
  static unsigned long pairingStartTime = millis();
  if (millis() - pairingStartTime > 900000) {  // 15 minutes
    Serial.println("Pairing timeout, generating new code");
    deviceManager.registerDevice();  // Generate new pairing code
    pairingStartTime = millis();
  }
}
```

### 5. Operational Mode

```cpp
void handleOperationalState() {
  // Device is claimed and ready for color palettes
  // Handle incoming color messages via WebSocket
  // Show idle animation or last received colors

  showIdleAnimation();

  // Send periodic heartbeat
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }
}
```

## WebSocket Communication

### Message Handling

```cpp
void handleWebSocketMessage(String payload) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload);

  String messageType = doc["type"];

  if (messageType == "deviceClaimed") {
    handleDeviceClaimed(doc["data"]);
  } else if (messageType == "colorPalette") {
    handleColorPalette(doc["data"]);
  } else if (messageType == "resetDevice") {
    handleDeviceReset();
  } else {
    Serial.println("Unknown message type: " + messageType);
  }
}
```

### Device Claimed Handler

```cpp
void handleDeviceClaimed(JsonObject data) {
  String ownerId = data["ownerId"];
  String ownerName = data["ownerName"];

  Serial.println("Device claimed by: " + ownerName);

  // Save provisioned state
  deviceManager.markAsProvisioned(ownerId, ownerName);

  // Show success animation
  showSuccessAnimation();

  // Transition to operational state
  currentState = STATE_OPERATIONAL;
}
```

### Color Palette Handler

```cpp
void handleColorPalette(JsonObject data) {
  JsonArray colors = data["colors"];
  String name = data["name"] | "Unnamed";
  int duration = data["duration"] | 5000;

  Serial.println("Received color palette: " + name);

  // Convert JSON colors to LED format
  uint32_t ledColors[LED_COUNT];
  int colorCount = min((int)colors.size(), LED_COUNT);

  for (int i = 0; i < colorCount; i++) {
    uint8_t r = colors[i]["r"];
    uint8_t g = colors[i]["g"];
    uint8_t b = colors[i]["b"];
    ledColors[i] = strip.Color(r, g, b);
  }

  // Display colors on LED strip
  displayColorPalette(ledColors, colorCount, duration);
}
```

## LED Control System

### Basic LED Operations

```cpp
void displayColorPalette(uint32_t* colors, int count, int duration) {
  // Clear strip
  strip.clear();

  // Set colors
  for (int i = 0; i < count && i < LED_COUNT; i++) {
    strip.setPixelColor(i, colors[i]);
  }

  // Fade in effect
  fadeIn(duration / 4);

  // Hold colors
  delay(duration / 2);

  // Fade out effect
  fadeOut(duration / 4);
}

void fadeIn(int duration) {
  for (int brightness = 0; brightness <= 255; brightness += 5) {
    strip.setBrightness(brightness);
    strip.show();
    delay(duration / 51);  // 51 steps (255/5)
  }
}

void fadeOut(int duration) {
  for (int brightness = 255; brightness >= 0; brightness -= 5) {
    strip.setBrightness(brightness);
    strip.show();
    delay(duration / 51);
  }
}
```

### Status Animations

```cpp
void showBootAnimation() {
  // Rainbow animation during boot
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.ColorHSV(i * 65536 / LED_COUNT, 255, 255));
    strip.show();
    delay(50);
  }
  delay(1000);
  strip.clear();
  strip.show();
}

void showPairingCode(String code) {
  // Flash LED count equal to each digit
  for (int i = 0; i < code.length(); i++) {
    int digit = code.charAt(i) - '0';

    // Flash LEDs 'digit' times
    for (int flash = 0; flash < digit; flash++) {
      strip.fill(strip.Color(0, 0, 255), 0, LED_COUNT);  // Blue
      strip.show();
      delay(200);
      strip.clear();
      strip.show();
      delay(200);
    }

    delay(1000);  // Pause between digits
  }
}

void showIdleAnimation() {
  // Breathing effect when idle
  static int brightness = 0;
  static int direction = 1;

  brightness += direction * 2;
  if (brightness >= 100) direction = -1;
  if (brightness <= 0) direction = 1;

  strip.fill(strip.Color(brightness, brightness, brightness), 0, 3);
  strip.show();
  delay(50);
}
```

## Persistent Storage

### EEPROM Management

```cpp
void DeviceManager::saveDeviceState() {
  preferences.begin("palpalette", false);

  preferences.putString("deviceId", deviceInfo.deviceId);
  preferences.putString("macAddress", deviceInfo.macAddress);
  preferences.putString("pairingCode", deviceInfo.pairingCode);
  preferences.putBool("isProvisioned", deviceInfo.isProvisioned);
  preferences.putString("ownerId", deviceInfo.ownerId);
  preferences.putString("ownerName", deviceInfo.ownerName);

  preferences.end();
}

void DeviceManager::loadDeviceState() {
  preferences.begin("palpalette", true);

  deviceInfo.deviceId = preferences.getString("deviceId", "");
  deviceInfo.macAddress = preferences.getString("macAddress", WiFi.macAddress());
  deviceInfo.pairingCode = preferences.getString("pairingCode", "");
  deviceInfo.isProvisioned = preferences.getBool("isProvisioned", false);
  deviceInfo.ownerId = preferences.getString("ownerId", "");
  deviceInfo.ownerName = preferences.getString("ownerName", "");

  preferences.end();
}
```

## Error Handling

### Watchdog Timer

```cpp
void setup() {
  // Enable watchdog timer (8 seconds)
  esp_task_wdt_init(8, true);
  esp_task_wdt_add(NULL);
}

void loop() {
  // Feed the watchdog
  esp_task_wdt_reset();

  // Main loop code...
}
```

### Error Recovery

```cpp
void handleErrorState() {
  static int errorCount = 0;
  static unsigned long lastErrorTime = 0;

  errorCount++;
  Serial.println("Error state, count: " + String(errorCount));

  // Show error animation
  showErrorAnimation();

  // Try to recover based on error count
  if (errorCount < 3) {
    // Soft recovery - retry WiFi
    delay(5000);
    currentState = STATE_WIFI_SETUP;
  } else if (errorCount < 5) {
    // Medium recovery - restart registration
    delay(10000);
    currentState = STATE_DEVICE_REGISTRATION;
  } else {
    // Hard recovery - factory reset
    Serial.println("Multiple errors, performing factory reset");
    deviceManager.resetToFactory();
    ESP.restart();
  }

  lastErrorTime = millis();
}
```

## Development and Testing

### Debug Output

```cpp
#ifdef DEBUG_SERIAL
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(format, ...) Serial.printf(format, __VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(format, ...)
#endif
```

### Memory Monitoring

```cpp
void printMemoryInfo() {
  DEBUG_PRINTF("Free heap: %d bytes\n", ESP.getFreeHeap());
  DEBUG_PRINTF("Min free heap: %d bytes\n", ESP.getMinFreeHeap());
  DEBUG_PRINTF("Max alloc heap: %d bytes\n", ESP.getMaxAllocHeap());
}
```

### Network Diagnostics

```cpp
void printNetworkInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN("WiFi Status: Connected");
    DEBUG_PRINTLN("IP Address: " + WiFi.localIP().toString());
    DEBUG_PRINTLN("Signal Strength: " + String(WiFi.RSSI()) + " dBm");
    DEBUG_PRINTLN("Gateway: " + WiFi.gatewayIP().toString());
  } else {
    DEBUG_PRINTLN("WiFi Status: Disconnected");
  }
}
```

## Production Deployment

### Firmware Configuration

```cpp
// Production config.h settings
#define API_BASE_URL "https://api.palpalette.com"
#define WS_HOST "api.palpalette.com"
#define WS_PORT 3001
#define USE_SSL true
#define DEBUG_SERIAL false
```

### Over-the-Air Updates (Planned)

```cpp
#include <ESP32httpUpdate.h>

void checkForUpdates() {
  String firmwareVersion = "1.0.0";
  String updateURL = API_BASE_URL + "/firmware/check?version=" + firmwareVersion;

  // Check for updates and download if available
  // Implement secure OTA update mechanism
}
```

## Troubleshooting

### Common Issues

#### WiFi Connection Problems

```cpp
// Diagnostic function
void diagnoseWiFi() {
  DEBUG_PRINTLN("WiFi Diagnosis:");
  DEBUG_PRINTLN("Status: " + String(WiFi.status()));
  DEBUG_PRINTLN("SSID: " + WiFi.SSID());
  DEBUG_PRINTLN("RSSI: " + String(WiFi.RSSI()));
  DEBUG_PRINTLN("MAC: " + WiFi.macAddress());

  // Try different channels
  // Check interference
  // Verify credentials
}
```

#### WebSocket Connection Issues

```cpp
void diagnoseWebSocket() {
  DEBUG_PRINTLN("WebSocket Diagnosis:");
  DEBUG_PRINTLN("Connected: " + String(wsClient.isConnected()));
  DEBUG_PRINTLN("Host: " + String(WS_HOST));
  DEBUG_PRINTLN("Port: " + String(WS_PORT));

  // Test connectivity with ping
  // Check firewall settings
  // Verify backend status
}
```

#### LED Strip Problems

```cpp
void testLEDStrip() {
  DEBUG_PRINTLN("Testing LED strip...");

  // Test each color channel
  strip.fill(strip.Color(255, 0, 0), 0, LED_COUNT);  // Red
  strip.show();
  delay(1000);

  strip.fill(strip.Color(0, 255, 0), 0, LED_COUNT);  // Green
  strip.show();
  delay(1000);

  strip.fill(strip.Color(0, 0, 255), 0, LED_COUNT);  // Blue
  strip.show();
  delay(1000);

  strip.clear();
  strip.show();
}
```

## Performance Optimization

### Memory Management

```cpp
// Use static buffers where possible
static char jsonBuffer[1024];
static uint32_t colorBuffer[LED_COUNT];

// Avoid String concatenation in loops
String buildMessage() {
  String msg;
  msg.reserve(256);  // Pre-allocate capacity
  msg += "{\"type\":\"status\",";
  msg += "\"data\":{";
  msg += "\"macAddress\":\"" + WiFi.macAddress() + "\",";
  msg += "\"isOnline\":true}}";
  return msg;
}
```

### Power Management

```cpp
// Reduce CPU frequency for battery operation
void setupPowerSaving() {
  setCpuFrequencyMhz(80);  // Reduce from 240MHz to 80MHz

  // Configure LED brightness based on power source
  int brightness = (powerSource == BATTERY) ? 50 : 255;
  strip.setBrightness(brightness);
}
```

## Future Enhancements

### Planned Features

- Over-the-air firmware updates
- Multiple LED strip support
- Audio reactive colors
- Temperature and humidity sensors
- Battery operation with sleep modes
- Mesh networking for device groups
- Local color generation algorithms
- Integration with smart home systems

### Hardware Expansions

- ESP32-S3 support for USB-C
- External flash for color storage
- Microphone for audio reactive effects
- Accelerometer for motion-based colors
- Display for status information
- Additional GPIO for sensors

This documentation provides a comprehensive guide to the PalPalette ESP32 firmware architecture, implementation details, and development practices. The modular design ensures maintainability while the state machine provides reliable operation in various network conditions.
