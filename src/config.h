#ifndef CONFIG_H
#define CONFIG_H

// Version information
#define FIRMWARE_VERSION "2.0.0"
#define DEVICE_TYPE "PalPalette"

// Default configuration values
#define DEFAULT_AP_SSID "PalPalette-Setup"
#define DEFAULT_AP_PASSWORD "setup123"
#define DEFAULT_SERVER_URL "ws://192.168.178.66:3001/ws" //"ws://cides06.gm.fh-koeln.de:3001/ws"

// Timing constants
#define WIFI_CONNECT_TIMEOUT 30000       // 30 seconds
#define HEARTBEAT_INTERVAL 30000         // 30 seconds
#define REGISTRATION_RETRY_INTERVAL 5000 // 5 seconds (initial retry delay)
#define STATUS_UPDATE_INTERVAL 60000     // 1 minute

// Network constants with exponential backoff
#define MAX_WIFI_RETRY_ATTEMPTS 3
#define INITIAL_RETRY_DELAY 1000      // 1 second initial delay
#define MAX_RETRY_DELAY 60000         // 60 seconds maximum delay
#define BACKOFF_MULTIPLIER 2          // Double delay each attempt
#define CAPTIVE_PORTAL_TIMEOUT 300000 // 5 minutes

// Watchdog Timer constants
#define WATCHDOG_TIMEOUT 30000      // 30 seconds - Longer timeout for mDNS and other network operations
#define WATCHDOG_FEED_INTERVAL 5000 // Feed watchdog every 5 seconds

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

// Error Handling System
enum class ErrorCode : uint8_t
{
    NONE = 0,
    WIFI_CONNECTION_FAILED = 1,
    DEVICE_REGISTRATION_FAILED = 2,
    WEBSOCKET_CONNECTION_FAILED = 3,
    MEMORY_ALLOCATION_FAILED = 4,
    LIGHTING_SYSTEM_FAILED = 5,
    PREFERENCES_ACCESS_FAILED = 6,
    HTTP_REQUEST_FAILED = 7,
    JSON_PARSING_FAILED = 8,
    WATCHDOG_INITIALIZATION_FAILED = 9,
    CAPTIVE_PORTAL_FAILED = 10,
    UNKNOWN_ERROR = 255
};

enum class RecoveryStrategy : uint8_t
{
    RETRY_OPERATION = 0,
    RESTART_COMPONENT = 1,
    SOFT_RESTART = 2,
    HARD_RESTART = 3,
    FACTORY_RESET = 4
};

// Error recovery configuration
#define MAX_ERROR_RETRIES 3
#define ERROR_RECOVERY_DELAY 2000  // 2 seconds between recovery attempts
#define CRITICAL_ERROR_THRESHOLD 5 // Number of errors before hard restart

// Global function declarations
extern void globalFeedWatchdog(); // Function to feed watchdog from anywhere in the code

// Storage keys for preferences
#define DEVICE_PREF_NAMESPACE "palpalette"
#define PREF_WIFI_SSID "wifi_ssid"
#define PREF_WIFI_PASSWORD "wifi_pass"
#define PREF_SERVER_URL "server_url"
#define PREF_DEVICE_ID "device_id"
#define PREF_IS_PROVISIONED "provisioned"
#define PREF_MAC_ADDRESS "mac_addr"

#endif
