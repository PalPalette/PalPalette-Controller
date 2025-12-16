# ESP32 Firmware Structure

## Directory Layout

```
src/
├── main.ino                 # Application entry point & state machine
├── config.h                 # Configuration constants
├── core/                    # Core system components
│   ├── DeviceManager        # Device identity & persistence
│   ├── WiFiManager          # WiFi & captive portal
│   └── WSClient             # WebSocket backend communication
└── lighting/                # Lighting system
    ├── LightManager         # Orchestrates lighting operations
    ├── LightController      # Base class interface
    └── controllers/         # Hardware implementations
        ├── NanoleafController
        ├── WLEDController
        └── WS2812Controller
```

## Key Components

**DeviceManager** - Device ID, pairing codes, provisioning state  
**WiFiManager** - Network connection, AP mode for setup, credential storage  
**WSClient** - WebSocket connection, handles backend events (colorPalette, factoryReset, etc.)  
**LightManager** - Manages active controller, configuration persistence  
**LightController** - Abstract interface for lighting hardware implementations

## Adding New Lighting Hardware

1. Create `YourController.h/cpp` in `lighting/controllers/`
2. Inherit from `LightController` and implement virtual methods
3. Add to factory in `LightController.cpp`

## Architecture

- Core system is independent of lighting implementations
- Lighting controllers are modular and swappable
- Main application coordinates between core and lighting systems
- Clear separation enables easy testing and maintenance
