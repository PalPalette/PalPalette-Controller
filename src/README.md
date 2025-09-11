# ESP32 Controller Code Structure

This document describes the organized structure of the PalPalette ESP32 controller firmware.

## Directory Structure

```
src/
├── main.ino                    # Main application entry point
├── config.h                    # Global configuration and constants
│
├── core/                       # Core system functionality
│   ├── DeviceManager.h/cpp     # Device identification and management
│   ├── WiFiManager.h/cpp       # WiFi connection and captive portal
│   └── WSClient.h/cpp          # WebSocket client for backend communication
│
└── lighting/                   # Lighting system management
    ├── LightManager.h/cpp      # Main lighting system manager
    ├── LightController.h/cpp   # Abstract base class for lighting controllers
    │
    └── controllers/            # Specific lighting system implementations
        ├── WS2812Controller.h/cpp     # WS2812B LED strip controller
        ├── WLEDController.h/cpp       # WLED system controller
        └── NanoleafController.h/cpp   # Nanoleaf panel controller
```

## Architecture Overview

### Core System (`src/core/`)

The core system handles the fundamental ESP32 functionality:

- **DeviceManager**: Manages device identity, pairing codes, and persistence
- **WiFiManager**: Handles WiFi connection, captive portal setup
- **WSClient**: WebSocket communication with the backend server

### Lighting System (`src/lighting/`)

The lighting system provides a modular architecture for different lighting hardware:

- **LightManager**: Orchestrates lighting operations and manages active controllers
- **LightController**: Abstract base class defining the interface for all lighting systems
- **Controllers**: Specific implementations for different lighting hardware

### Main Application (`src/main.ino`)

The main application file contains:

- System initialization
- State machine for device lifecycle
- Main loop coordination
- Serial command interface for debugging

## Key Design Principles

### 1. Separation of Concerns

- Core system functionality is separate from lighting-specific code
- Each lighting controller is independent and self-contained

### 2. Modular Architecture

- Easy to add new lighting controllers without affecting core system
- Controllers implement a common interface for consistent behavior

### 3. Clear Dependencies

- Core system has no dependencies on lighting system
- Lighting controllers depend only on base classes
- Main application coordinates between systems

### 4. Maintainability

- Related files are grouped together
- Clear naming conventions
- Organized include paths

## Adding New Lighting Controllers

To add a new lighting system:

1. Create new files in `src/lighting/controllers/`:

   - `YourController.h`
   - `YourController.cpp`

2. Inherit from `LightController` base class
3. Implement all virtual methods
4. Add to the factory in `LightController.cpp`
5. Update documentation

## Include Path Examples

```cpp
// From main.ino
#include "core/WiFiManager.h"
#include "lighting/LightManager.h"

// From lighting controllers
#include "../LightController.h"

// From core files
#include "../config.h"
```

## Benefits of This Structure

1. **Easier Navigation**: Related files are grouped together
2. **Better Maintainability**: Changes to lighting don't affect core system
3. **Scalability**: Easy to add new features and controllers
4. **Clear Architecture**: Obvious separation between system layers
5. **Reduced Complexity**: Each directory has a focused responsibility
