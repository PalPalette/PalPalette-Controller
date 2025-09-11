#ifndef CONFIG_H
#define CONFIG_H

// Version information
#define FIRMWARE_VERSION "2.0.0"
#define DEVICE_TYPE "PalPalette"

// Default configuration values
#define DEFAULT_AP_SSID "PalPalette-Setup"
#define DEFAULT_AP_PASSWORD "setup123"
#define DEFAULT_SERVER_URL "ws://192.168.178.64:3001/ws" //"ws://cides06.gm.fh-koeln.de:3001/ws"

// Timing constants
#define WIFI_CONNECT_TIMEOUT 30000       // 30 seconds
#define HEARTBEAT_INTERVAL 30000         // 30 seconds
#define REGISTRATION_RETRY_INTERVAL 5000 // 5 seconds
#define STATUS_UPDATE_INTERVAL 60000     // 1 minute

// Network constants
#define MAX_WIFI_RETRY_ATTEMPTS 3
#define CAPTIVE_PORTAL_TIMEOUT 300000 // 5 minutes

// Hardware pins (if needed for future LED integration)
#define LED_DATA_PIN 2
#define LED_COUNT 10

// WS2812 default configuration
#define DEFAULT_LED_PIN 2
#define DEFAULT_NUM_LEDS 10

// Debug flags
// DEBUG_LIGHT_CONTROLLER is defined in platformio.ini build_flags
#define DEBUG_DEVICE_MANAGER
#define DEBUG_WIFI_MANAGER

// Storage keys for preferences
#define DEVICE_PREF_NAMESPACE "palpalette"
#define PREF_WIFI_SSID "wifi_ssid"
#define PREF_WIFI_PASSWORD "wifi_pass"
#define PREF_SERVER_URL "server_url"
#define PREF_DEVICE_ID "device_id"
#define PREF_IS_PROVISIONED "provisioned"
#define PREF_MAC_ADDRESS "mac_addr"

#endif
