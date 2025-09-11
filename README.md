# PalPalette ESP32 Controller - Version 2.0

## Overview

This is the completely refactored ESP32 firmware for the PalPalette system, designed for self-setup capability and open-source distribution. Users can now flash the firmware and configure devices independently without pre-generated credentials.

## Key Features

### 🔄 Self-Setup System

- **WiFi Captive Portal**: Automatic setup interface when no WiFi credentials are stored
- **Auto Device Registration**: Devices register themselves with the backend upon first connection
- **Pairing Code System**: 6-digit codes for easy device claiming via mobile app

### 🏗 Modular Architecture

- **WiFiManager**: Handles WiFi connection and captive portal setup
- **DeviceManager**: Manages device registration, status, and pairing
- **WSClient**: WebSocket communication with backend server
- **Config System**: Centralized configuration and constants

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

### Step 2: WiFi Configuration

1. Device will create a WiFi access point: `PalPalette-Setup-XXXXXX`
2. Connect to this network using password: `setup123`
3. Open a web browser and navigate to any website (captive portal will redirect)
4. Enter your WiFi credentials and optionally set a custom server URL
5. Click "Save Settings & Connect"

### Step 3: Device Registration

1. Device automatically connects to WiFi
2. Registers with the backend server
3. Displays a 6-digit pairing code in serial monitor

### Step 4: Device Claiming

1. Open the PalPalette mobile app
2. Use "Add Device" feature
3. Enter the 6-digit pairing code shown on device
4. Device is now claimed and operational

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

- **registerDevice**: Device registration with backend
- **deviceClaimed**: User claims device with pairing code
- **colorPalette**: Receive color palettes for display
- **setupComplete**: Finalize device setup process

## Debugging

### Serial Commands

The device supports several debug commands via serial monitor:

- `status` - Show full system status
- `wifi` - Show WiFi connection information
- `reset` - Reset all device settings and restart
- `restart` - Restart the device
- `help` - Show available commands

### Status Indicators

Monitor the serial output for these status messages:

- `📶 WiFi connected successfully!` - WiFi connection established
- `✅ Device registered with HTTP API` - Backend registration successful
- `🔌 WebSocket connected successfully!` - Real-time communication active
- `🔑 Pairing Code: XXXXXX` - Code for mobile app claiming
- `🎉 Device has been claimed!` - Device successfully claimed by user

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
├── main.ino              # Main firmware and state machine
├── config.h              # Configuration constants
├── WiFiManager.h/.cpp     # WiFi and captive portal management
├── DeviceManager.h/.cpp   # Device registration and status
└── WSClient.h/.cpp        # WebSocket communication
```

### Memory Management

- Uses ESP32 Preferences for persistent storage
- Minimal memory footprint for WebSocket buffers
- Efficient JSON parsing with ArduinoJson

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

- WiFi-related features: Add to `WiFiManager`
- Device features: Add to `DeviceManager`
- Communication features: Add to `WSClient`
- New functionality: Create new modules following existing patterns

## Version History

### Version 2.0.0 (Current)

- Complete refactor to modular architecture
- Self-setup capability with captive portal
- Automatic device registration
- Pairing code system for claiming
- WebSocket communication with backend
- Robust state machine operation

### Version 1.0.0 (Legacy)

- Monolithic firmware with hardcoded credentials
- Manual device setup required
- Direct WebSocket connection only
- Pre-generated device IDs

---

For more information about the complete PalPalette system, see the main project README.
