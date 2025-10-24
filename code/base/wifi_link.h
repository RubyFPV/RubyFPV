#pragma once
#include "base.h"

// WiFi Direct Link Management
// Provides functions to set up WiFi networks for direct VTX to VRX video streaming

// WiFi link modes
#define WIFI_LINK_MODE_NONE       0
#define WIFI_LINK_MODE_AP         1  // VTX creates its own WiFi network
#define WIFI_LINK_MODE_CLIENT     2  // VRX connects to existing network
#define WIFI_LINK_MODE_MESH       3  // Future: mesh networking

// WiFi link status
#define WIFI_LINK_STATUS_DISCONNECTED  0
#define WIFI_LINK_STATUS_CONNECTING    1
#define WIFI_LINK_STATUS_CONNECTED     2
#define WIFI_LINK_STATUS_ERROR         3

typedef struct {
    int mode;                    // WIFI_LINK_MODE_*
    char ssid[64];              // Network SSID
    char password[128];         // Network password (WPA2)
    int channel;                // WiFi channel (1-14)
    char ip_address[32];        // IP address (for AP mode)
    char netmask[32];           // Network mask
    int dhcp_enabled;           // Enable DHCP server (AP mode)
    char dhcp_start[32];        // DHCP range start
    char dhcp_end[32];          // DHCP range end
    int port;                   // UDP port for video
} wifi_link_config_t;

typedef struct {
    int status;                 // WIFI_LINK_STATUS_*
    char current_ip[32];        // Current IP address
    char connected_clients[8][32]; // Connected client IPs (AP mode)
    int num_clients;            // Number of connected clients
    int signal_strength;        // Signal strength (dBm)
    u32 bytes_sent;            // Total bytes sent
    u32 bytes_received;        // Total bytes received
    u32 last_activity_ms;      // Last activity timestamp
} wifi_link_status_t;

// Initialize WiFi link subsystem
bool wifi_link_init();

// Configure WiFi link
bool wifi_link_configure(const wifi_link_config_t* config);

// Start WiFi link (AP or Client mode)
bool wifi_link_start();

// Stop WiFi link
void wifi_link_stop();

// Get current WiFi link status
void wifi_link_get_status(wifi_link_status_t* status);

// Check if WiFi link is connected
bool wifi_link_is_connected();

// Get WiFi interface name (e.g., "wlan0")
const char* wifi_link_get_interface();

// Set WiFi interface to use
bool wifi_link_set_interface(const char* interface);

// Scan for available networks (client mode)
int wifi_link_scan_networks(char* buffer, int max_size);

// Get supported WiFi channels
int wifi_link_get_supported_channels(int* channels, int max_channels);

// Save/load configuration
bool wifi_link_save_config(const char* filename);
bool wifi_link_load_config(const char* filename);
