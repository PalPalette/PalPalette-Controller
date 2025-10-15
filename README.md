# PalPalette ESP32 Controller - Version 2.0

## Overview

This is the completely refactored ESP32 firmware for the PalPalette system, designed for self-setup capability and open-source distribution. Users can now flash the firmware and configure devices independently without pre-generated credentials.

## Key Features

### 🔄 Self-Setup System

- **WiFi Captive Portal**: Automatic setup interface when no WiFi credentials are stored
- **Auto Device Registration**: Devices register themselves with the backend upon first connection
- **Pairing Code System**: 6-digit codes for easy device claiming via mobile app

### 🏗 Modular Architecture

- **Core System**: Fundamental ESP32 functionality and communication
  - **WiFiManager**: Handles WiFi connection and captive portal setup
  - **DeviceManager**: Manages device registration, status, and pairing
  - **WSClient**: WebSocket communication with backend server
- **Lighting System**: Modular architecture for different lighting hardware
  - **LightManager**: Orchestrates lighting operations and manages controllers
  - **LightController**: Abstract base class for all lighting systems
  - **Hardware Controllers**: Specific implementations (Nanoleaf, WS2812, etc.)
- **Error Handling**: Comprehensive error management and recovery
- **Config System**: Centralized configuration with error codes and constants

### 🌐 Network Features

- **Automatic WiFi Configuration**: Web-based setup interface
- **WebSocket Communication**: Real-time messaging with backend
- **HTTP Status Updates**: Regular device status reporting
- **Connection Recovery**: Automatic reconnection handling

## Hardware Compatibility

- **Primary Target**: Seeed XIAO ESP32C3
- **Secondary Target**: ESP8266 NodeMCU v2
- **Memory Requirements**: Minimum 4MB flash memory
- **WiFi Requirements**: 2.4GHz WiFi capability

## Installation

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- ESP32 or ESP8266 development board
- USB cable for flashing

### Flashing Firmware

1. **Clone Repository**:

   ```bash
   git clone <repository-url>
   cd PalPalette-2/controller
   ```

2. **Install Dependencies**:

   ```bash
   pio lib install
   ```

3. **Build and Upload**:

   ```bash
   # For ESP32 (default)
   pio run -t upload

   # For ESP8266
   pio run -e esp8266 -t upload
   ```

4. **Monitor Serial Output**:
   ```bash
   pio device monitor
   ```

## First-Time Setup

### Step 1: Initial Boot

1. Flash the firmware to your ESP32/ESP8266
2. Power on the device
3. Monitor serial output for setup information

### Step 2: WiFi & Lighting Configuration

1. Device will create a WiFi access point: `PalPalette-Setup-XXXXXX`
2. Connect to this network using password: `setup123`
3. Open a web browser and navigate to any website (captive portal will redirect)
4. Configure your settings:
   - **WiFi Credentials**: Enter your network SSID and password
   - **Server URL**: Optionally set a custom backend server
   - **Lighting System**: Select your lighting hardware (Nanoleaf, WS2812, etc.)
   - **Hardware Details**: Enter IP address or enable auto-discovery
5. Click "Save Settings & Connect"

### Step 3: Device Registration

1. Device automatically connects to WiFi
2. Registers with the backend server
3. Displays a 6-digit pairing code in serial monitor

### Step 4: Device Claiming & Lighting Authentication

1. Open the PalPalette mobile app
2. Use "Add Device" feature
3. Enter the 6-digit pairing code shown on device
4. **For Nanoleaf**: Press and hold the button on your Nanoleaf controller when prompted
5. **For other systems**: Follow the authentication instructions shown in the app
6. Device is now claimed and operational with lighting system connected

## Configuration

### Default Settings

- **AP SSID**: `PalPalette-Setup-XXXXXX` (XXXXXX = last 6 chars of MAC)
- **AP Password**: `setup123`
- **Default Server**: `ws://192.168.1.100:3001`
- **HTTP API Port**: `3000`
- **WebSocket Port**: `3001`

### Customization

Server URL can be customized during initial setup or by editing `config.h`:

```cpp
#define DEFAULT_SERVER_URL "ws://your-server.com:3001"
```

## API Integration

### Device Registration Endpoint

```
POST http://server:3000/devices/register
{
  "macAddress": "AA:BB:CC:DD:EE:FF",
  "deviceType": "PalPalette",
  "firmwareVersion": "2.0.0",
  "ipAddress": "192.168.1.123"
}
```

### WebSocket Events

**Outgoing Messages (Device → Backend):**

- **registerDevice**: Device registration with backend
- **deviceStatus**: Regular status updates and heartbeat
- **lightingCapabilities**: Report supported lighting systems
- **authenticationRequest**: Request lighting system authentication
- **errorReport**: Report system errors for monitoring

**Incoming Messages (Backend → Device):**

- **deviceClaimed**: User claims device with pairing code
- **colorPalette**: Receive color palettes for display
- **lightingConfig**: Configure connected lighting system
- **systemCommand**: Administrative commands (restart, reset, etc.)
- **authenticationResponse**: Authentication tokens and configuration

## Debugging

### Serial Commands

The device supports comprehensive debug commands via serial monitor:

- `status` - Show full system status including lighting system
- `wifi` - Show WiFi connection and network information
- `lighting` - Show lighting system status and configuration
- `memory` - Display memory usage and health information
- `errors` - Show recent error history and recovery attempts
- `reset` - Factory reset all device settings and restart
- `restart` - Soft restart the device
- `watchdog` - Show watchdog timer status and statistics
- `help` - Show all available commands

### Status Indicators

Monitor the serial output for these status messages:

- `📶 WiFi connected successfully!` - WiFi connection established
- `✅ Device registered with HTTP API` - Backend registration successful
- `🔌 WebSocket connected successfully!` - Real-time communication active
- `� Lighting system ready` - Hardware controller initialized
- `🍃 Nanoleaf device discovered` - Automatic mDNS discovery successful
- `🔐 Authentication required` - User action needed for lighting system
- `�🔑 Pairing Code: XXXXXX` - Code for mobile app claiming
- `🎉 Device has been claimed!` - Device successfully claimed by user
- `🎨 Color palette received` - New colors being displayed
- `⚠ Error recovery in progress` - System handling errors automatically
- `🔧 Watchdog fed` - System stability monitoring active

## Architecture Details

### State Machine

The firmware uses a state machine for robust operation:

1. **INIT** - System initialization
2. **WIFI_SETUP** - Captive portal or stored credentials
3. **WIFI_CONNECTING** - Attempting WiFi connection
4. **DEVICE_REGISTRATION** - Registering with backend
5. **WAITING_FOR_CLAIM** - Awaiting user claiming via app
6. **OPERATIONAL** - Normal operation, receiving color palettes
7. **ERROR** - Error state with recovery attempts

### File Structure

```
src/
├── main.ino                    # Main firmware with state machine and error handling
├── config.h                    # Enhanced configuration with error codes and constants
├── core/                       # Core system modules
│   ├── DeviceManager.h/.cpp    # Device registration and persistent storage
│   ├── WiFiManager.h/.cpp      # WiFi connection and captive portal
│   └── WSClient.h/.cpp         # WebSocket communication and message handling
└── lighting/                   # Modular lighting system
    ├── LightManager.h/.cpp     # Lighting orchestration and configuration
    ├── LightController.h/.cpp  # Abstract base class and factory
    └── controllers/            # Hardware-specific implementations
        └── NanoleafController.h/.cpp  # Nanoleaf panels with mDNS discovery
```

### Lighting System Architecture

The firmware now includes a comprehensive modular lighting system supporting multiple hardware types:

#### 🎯 **Supported Lighting Hardware**

- **Nanoleaf Panels**: Aurora, Canvas, Shapes with automatic mDNS discovery
- **WS2812 LED Strips**: Addressable RGB LED strips (planned)
- **Generic RGB**: Standard RGB controllers (planned)

#### 🔧 **Key Features**

- **Auto-Discovery**: Automatic detection of Nanoleaf devices on the network
- **Authentication Management**: Secure token-based authentication with Nanoleaf
- **Dynamic Configuration**: Runtime configuration through mobile app
- **Color Palette Display**: Smooth transitions and animations
- **Panel Layout Mapping**: Automatic panel arrangement detection

### Error Handling & Recovery

Advanced error management system with sophisticated recovery strategies:

#### 📊 **Error Types**

- WiFi connection failures with exponential backoff
- Device registration timeouts with retry logic
- WebSocket disconnections with automatic reconnection
- Lighting system authentication failures
- Memory allocation and watchdog timer issues

#### 🔄 **Recovery Strategies**

- **Retry Operation**: Simple retry with backoff
- **Component Restart**: Restart specific subsystems
- **Soft Restart**: Clear state and restart state machine
- **Hard Restart**: Full device restart
- **Factory Reset**: Complete configuration reset

### Memory & Performance Management

- **ESP32 Preferences**: Persistent storage for device configuration
- **Watchdog Timer**: 30-second system stability monitoring
- **Memory Health**: Continuous free heap monitoring
- **Optimized Loops**: State-based loop delays for power efficiency
- **Efficient JSON**: Optimized parsing with ArduinoJson v7

## Troubleshooting

### Common Issues

**Device won't connect to WiFi**:

- Ensure 2.4GHz WiFi network
- Check password accuracy
- Verify signal strength
- Try factory reset: Send `reset` command via serial

**Can't access setup portal**:

- Ensure device is in AP mode (check serial output)
- Connect to `PalPalette-Setup-XXXXXX` network
- Use password `setup123`
- Try accessing `192.168.4.1` directly

**Backend registration fails**:

- Verify server URL in setup
- Check server is running and accessible
- Ensure firewall allows connections on port 3000
- Check serial monitor for HTTP error codes

**Pairing code not working**:

- Ensure device is in WAITING_FOR_CLAIM state
- Check pairing code hasn't expired
- Verify mobile app is connected to same network/server
- Try resetting device and reclaiming

**Lighting system not connecting**:

- **For Nanoleaf**: Ensure panels are powered and on same network
- **For Nanoleaf**: Press and hold controller button during authentication
- Check lighting system IP address in setup portal
- Verify lighting hardware is supported and compatible
- Try mDNS discovery: Leave IP address empty for auto-discovery
- Monitor serial output for specific error messages

**System stability issues**:

- Check memory usage with `memory` serial command
- Monitor error logs with `errors` serial command
- Verify watchdog timer is functioning with `watchdog` command
- Check for repeated error/recovery cycles in serial output

### Factory Reset

To completely reset the device:

1. Connect via serial monitor
2. Send command: `reset`
3. Device will clear all settings and restart
4. Follow first-time setup process

## Development

### Building Custom Firmware

1. Modify configuration in `config.h`
2. Update version number in `config.h`
3. Build and test on device
4. Update this README with any changes

### Adding Features

The modular architecture makes adding features straightforward:

- **WiFi-related features**: Add to `WiFiManager` class
- **Device features**: Add to `DeviceManager` class
- **Communication features**: Add to `WSClient` class
- **New lighting hardware**: Implement `LightController` interface
- **Error handling**: Add error codes to `config.h` and recovery strategies
- **New functionality**: Create new modules following existing patterns

### Adding New Lighting Controllers

To add support for new lighting hardware:

1. **Create Controller Class**: Extend `LightController` base class
2. **Implement Interface**: Provide all required methods (initialize, displayPalette, etc.)
3. **Register with Factory**: Add to `LightControllerFactory` in `LightController.cpp`
4. **Add Configuration**: Update setup portal HTML in `WiFiManager.cpp`
5. **Test Integration**: Verify with debug commands and WebSocket events

## Version History

### Version 2.0.0 (Current)

- **Complete Modular Architecture**: Separated core, lighting, and error handling systems
- **Advanced Lighting Support**: Nanoleaf panels with mDNS auto-discovery
- **Comprehensive Error Handling**: Recovery strategies and exponential backoff
- **Self-Setup Capability**: Enhanced captive portal with lighting configuration
- **Watchdog Timer System**: Automatic system stability monitoring
- **Memory Management**: Health monitoring and optimization
- **Enhanced Debugging**: Comprehensive serial command interface
- **WebSocket Communication**: Bidirectional messaging with backend
- **Robust State Machine**: 7-state operation with error recovery

### Version 1.0.0 (Legacy)

- Monolithic firmware with hardcoded credentials
- Manual device setup required
- Direct WebSocket connection only
- Pre-generated device IDs

---

For more information about the complete PalPalette system, see the main project README.
