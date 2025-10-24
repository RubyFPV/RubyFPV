#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// WiFi Direct video source for direct VTX to VRX video streaming
// This allows RubyFPV to receive video directly over WiFi without using radio links

#define WIFI_DIRECT_DEFAULT_PORT 5600
#define WIFI_DIRECT_MAX_PACKET_SIZE 1400
#define WIFI_DIRECT_RECV_TIMEOUT_MS 100

typedef struct {
    bool enabled;
    char vtx_ip[32];
    int udp_port;
    bool is_multicast;
    char multicast_group[32];
} wifi_direct_config_t;

typedef struct {
    int udp_socket;
    struct sockaddr_in vtx_addr;
    struct sockaddr_in local_addr;
    
    // Statistics
    u32 packets_received;
    u32 bytes_received;
    u32 packets_dropped;
    u32 last_receive_time_ms;
    
    // Configuration
    wifi_direct_config_t config;
    
    // Buffer for receiving data
    u8 recv_buffer[WIFI_DIRECT_MAX_PACKET_SIZE];
    
    // State
    bool is_running;
    bool is_connected;
} wifi_direct_state_t;

// Function declarations
bool video_source_wifi_direct_init();
bool video_source_wifi_direct_start();
void video_source_wifi_direct_stop();
bool video_source_wifi_direct_is_started();
bool video_source_wifi_direct_has_data();
int video_source_wifi_direct_read(u8* buffer, int max_size);
void video_source_wifi_direct_set_config(const wifi_direct_config_t* config);
void video_source_wifi_direct_get_stats(u32* packets_received, u32* bytes_received, u32* packets_dropped);
u32 video_source_wifi_direct_get_last_receive_time();
void video_source_wifi_direct_update_stats();
