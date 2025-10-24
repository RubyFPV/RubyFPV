#include "video_source_wifi_direct.h"
#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/utils.h"
#include "../base/wifi_link.h"
#include "../common/string_utils.h"
#include "shared_vars.h"
#include "timers.h"

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

static wifi_direct_state_t s_WiFiDirectState;
static bool s_bInitialized = false;

bool video_source_wifi_direct_init()
{
    if (s_bInitialized) {
        return true;
    }
    
    memset(&s_WiFiDirectState, 0, sizeof(wifi_direct_state_t));
    s_WiFiDirectState.udp_socket = -1;
    
    // Default configuration
    s_WiFiDirectState.config.enabled = false;
    strcpy(s_WiFiDirectState.config.vtx_ip, "192.168.1.1");
    s_WiFiDirectState.config.udp_port = WIFI_DIRECT_DEFAULT_PORT;
    s_WiFiDirectState.config.is_multicast = false;
    strcpy(s_WiFiDirectState.config.multicast_group, "239.255.0.1");
    
    s_bInitialized = true;
    log_line("[WiFiDirect] Video source initialized");
    return true;
}

bool video_source_wifi_direct_start()
{
    if (!s_bInitialized) {
        if (!video_source_wifi_direct_init()) {
            return false;
        }
    }
    
    if (!s_WiFiDirectState.config.enabled) {
        log_line("[WiFiDirect] Video source disabled in config");
        return false;
    }
    
    if (s_WiFiDirectState.is_running) {
        log_line("[WiFiDirect] Video source already running");
        return true;
    }
    
    // Create UDP socket
    s_WiFiDirectState.udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_WiFiDirectState.udp_socket < 0) {
        log_softerror_and_alarm("[WiFiDirect] Failed to create UDP socket: %s", strerror(errno));
        return false;
    }
    
    // Set socket to non-blocking
    int flags = fcntl(s_WiFiDirectState.udp_socket, F_GETFL, 0);
    if (flags < 0) {
        log_softerror_and_alarm("[WiFiDirect] Failed to get socket flags: %s", strerror(errno));
        close(s_WiFiDirectState.udp_socket);
        s_WiFiDirectState.udp_socket = -1;
        return false;
    }
    
    if (fcntl(s_WiFiDirectState.udp_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_softerror_and_alarm("[WiFiDirect] Failed to set socket non-blocking: %s", strerror(errno));
        close(s_WiFiDirectState.udp_socket);
        s_WiFiDirectState.udp_socket = -1;
        return false;
    }
    
    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = WIFI_DIRECT_RECV_TIMEOUT_MS * 1000;
    if (setsockopt(s_WiFiDirectState.udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        log_line("[WiFiDirect] Warning: Failed to set receive timeout: %s", strerror(errno));
    }
    
    // Set receive buffer size for better performance
    int recv_buffer_size = 256 * 1024;  // 256KB
    if (setsockopt(s_WiFiDirectState.udp_socket, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size)) < 0) {
        log_line("[WiFiDirect] Warning: Failed to set receive buffer size: %s", strerror(errno));
    }
    
    // Bind to local port
    memset(&s_WiFiDirectState.local_addr, 0, sizeof(s_WiFiDirectState.local_addr));
    s_WiFiDirectState.local_addr.sin_family = AF_INET;
    s_WiFiDirectState.local_addr.sin_port = htons(s_WiFiDirectState.config.udp_port);
    s_WiFiDirectState.local_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(s_WiFiDirectState.udp_socket, (struct sockaddr*)&s_WiFiDirectState.local_addr, sizeof(s_WiFiDirectState.local_addr)) < 0) {
        log_softerror_and_alarm("[WiFiDirect] Failed to bind UDP socket to port %d: %s", 
                                s_WiFiDirectState.config.udp_port, strerror(errno));
        close(s_WiFiDirectState.udp_socket);
        s_WiFiDirectState.udp_socket = -1;
        return false;
    }
    
    // Join multicast group if configured
    if (s_WiFiDirectState.config.is_multicast) {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(s_WiFiDirectState.config.multicast_group);
        mreq.imr_interface.s_addr = INADDR_ANY;
        
        if (setsockopt(s_WiFiDirectState.udp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            log_softerror_and_alarm("[WiFiDirect] Failed to join multicast group %s: %s", 
                                    s_WiFiDirectState.config.multicast_group, strerror(errno));
            close(s_WiFiDirectState.udp_socket);
            s_WiFiDirectState.udp_socket = -1;
            return false;
        }
        
        log_line("[WiFiDirect] Joined multicast group %s", s_WiFiDirectState.config.multicast_group);
    }
    
    // Set up VTX address
    memset(&s_WiFiDirectState.vtx_addr, 0, sizeof(s_WiFiDirectState.vtx_addr));
    s_WiFiDirectState.vtx_addr.sin_family = AF_INET;
    s_WiFiDirectState.vtx_addr.sin_port = htons(s_WiFiDirectState.config.udp_port);
    
    if (s_WiFiDirectState.config.is_multicast) {
        s_WiFiDirectState.vtx_addr.sin_addr.s_addr = inet_addr(s_WiFiDirectState.config.multicast_group);
    } else {
        s_WiFiDirectState.vtx_addr.sin_addr.s_addr = inet_addr(s_WiFiDirectState.config.vtx_ip);
    }
    
    // Reset statistics
    s_WiFiDirectState.packets_received = 0;
    s_WiFiDirectState.bytes_received = 0;
    s_WiFiDirectState.packets_dropped = 0;
    s_WiFiDirectState.last_receive_time_ms = 0;
    
    s_WiFiDirectState.is_running = true;
    s_WiFiDirectState.is_connected = false;
    
    log_line("[WiFiDirect] Video source started - listening on UDP port %d", s_WiFiDirectState.config.udp_port);
    if (s_WiFiDirectState.config.is_multicast) {
        log_line("[WiFiDirect] Multicast mode: group %s", s_WiFiDirectState.config.multicast_group);
    } else {
        log_line("[WiFiDirect] Unicast mode: expecting video from %s", s_WiFiDirectState.config.vtx_ip);
    }
    
    return true;
}

void video_source_wifi_direct_stop()
{
    if (!s_WiFiDirectState.is_running) {
        return;
    }
    
    log_line("[WiFiDirect] Stopping video source");
    
    // Leave multicast group if joined
    if (s_WiFiDirectState.config.is_multicast && s_WiFiDirectState.udp_socket >= 0) {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(s_WiFiDirectState.config.multicast_group);
        mreq.imr_interface.s_addr = INADDR_ANY;
        
        if (setsockopt(s_WiFiDirectState.udp_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            log_line("[WiFiDirect] Warning: Failed to leave multicast group: %s", strerror(errno));
        }
    }
    
    // Close socket
    if (s_WiFiDirectState.udp_socket >= 0) {
        close(s_WiFiDirectState.udp_socket);
        s_WiFiDirectState.udp_socket = -1;
    }
    
    s_WiFiDirectState.is_running = false;
    s_WiFiDirectState.is_connected = false;
    
    log_line("[WiFiDirect] Video source stopped - received %u packets, %u bytes, dropped %u",
             s_WiFiDirectState.packets_received, s_WiFiDirectState.bytes_received, s_WiFiDirectState.packets_dropped);
}

bool video_source_wifi_direct_is_started()
{
    return s_WiFiDirectState.is_running;
}

bool video_source_wifi_direct_has_data()
{
    if (!s_WiFiDirectState.is_running || s_WiFiDirectState.udp_socket < 0) {
        return false;
    }
    
    // Check if we have received data recently (within last 1 second)
    u32 now_ms = get_current_timestamp_ms();
    if (s_WiFiDirectState.last_receive_time_ms > 0 && 
        (now_ms - s_WiFiDirectState.last_receive_time_ms) > 1000) {
        if (s_WiFiDirectState.is_connected) {
            log_line("[WiFiDirect] No data received for 1 second, marking as disconnected");
            s_WiFiDirectState.is_connected = false;
        }
    }
    
    // Use select to check if data is available
    fd_set readfds;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;  // No wait, just check
    
    FD_ZERO(&readfds);
    FD_SET(s_WiFiDirectState.udp_socket, &readfds);
    
    int result = select(s_WiFiDirectState.udp_socket + 1, &readfds, NULL, NULL, &tv);
    return (result > 0 && FD_ISSET(s_WiFiDirectState.udp_socket, &readfds));
}

int video_source_wifi_direct_read(u8* buffer, int max_size)
{
    if (!s_WiFiDirectState.is_running || s_WiFiDirectState.udp_socket < 0 || !buffer || max_size <= 0) {
        return -1;
    }
    
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    // Receive data
    int received = recvfrom(s_WiFiDirectState.udp_socket, s_WiFiDirectState.recv_buffer, 
                           WIFI_DIRECT_MAX_PACKET_SIZE, 0,
                           (struct sockaddr*)&from_addr, &from_len);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available (non-blocking socket)
            return 0;
        }
        
        log_softerror_and_alarm("[WiFiDirect] Failed to receive data: %s", strerror(errno));
        return -1;
    }
    
    if (received == 0) {
        // Connection closed
        log_line("[WiFiDirect] Connection closed by remote");
        return -1;
    }
    
    // Verify sender (if not multicast)
    if (!s_WiFiDirectState.config.is_multicast) {
        char sender_ip[32];
        inet_ntop(AF_INET, &from_addr.sin_addr, sender_ip, sizeof(sender_ip));
        
        if (strcmp(sender_ip, s_WiFiDirectState.config.vtx_ip) != 0) {
            log_line("[WiFiDirect] Ignoring packet from unexpected sender %s (expected %s)",
                     sender_ip, s_WiFiDirectState.config.vtx_ip);
            s_WiFiDirectState.packets_dropped++;
            return 0;
        }
    }
    
    // Mark as connected if this is first packet
    if (!s_WiFiDirectState.is_connected) {
        char sender_ip[32];
        inet_ntop(AF_INET, &from_addr.sin_addr, sender_ip, sizeof(sender_ip));
        log_line("[WiFiDirect] Connected - receiving video from %s:%d",
                 sender_ip, ntohs(from_addr.sin_port));
        s_WiFiDirectState.is_connected = true;
    }
    
    // Copy data to output buffer
    int bytes_to_copy = (received > max_size) ? max_size : received;
    memcpy(buffer, s_WiFiDirectState.recv_buffer, bytes_to_copy);
    
    // Update statistics
    s_WiFiDirectState.packets_received++;
    s_WiFiDirectState.bytes_received += received;
    s_WiFiDirectState.last_receive_time_ms = get_current_timestamp_ms();
    
    if (received > max_size) {
        log_line("[WiFiDirect] Warning: Packet size %d exceeds buffer size %d", received, max_size);
        s_WiFiDirectState.packets_dropped++;
    }
    
    return bytes_to_copy;
}

void video_source_wifi_direct_set_config(const wifi_direct_config_t* config)
{
    if (!config) {
        return;
    }
    
    bool was_running = s_WiFiDirectState.is_running;
    
    // Stop if running
    if (was_running) {
        video_source_wifi_direct_stop();
    }
    
    // Update configuration
    memcpy(&s_WiFiDirectState.config, config, sizeof(wifi_direct_config_t));
    
    log_line("[WiFiDirect] Configuration updated - enabled: %s, VTX IP: %s, port: %d, multicast: %s",
             config->enabled ? "yes" : "no",
             config->vtx_ip,
             config->udp_port,
             config->is_multicast ? config->multicast_group : "no");
    
    // Restart if was running
    if (was_running && config->enabled) {
        video_source_wifi_direct_start();
    }
}

void video_source_wifi_direct_get_stats(u32* packets_received, u32* bytes_received, u32* packets_dropped)
{
    if (packets_received) {
        *packets_received = s_WiFiDirectState.packets_received;
    }
    if (bytes_received) {
        *bytes_received = s_WiFiDirectState.bytes_received;
    }
    if (packets_dropped) {
        *packets_dropped = s_WiFiDirectState.packets_dropped;
    }
}

u32 video_source_wifi_direct_get_last_receive_time()
{
    return s_WiFiDirectState.last_receive_time_ms;
}

// Update WiFi Direct stats in model
void video_source_wifi_direct_update_stats()
{
    if (!g_pCurrentModel || !s_bInitialized) {
        return;
    }
    
    // Update runtime stats in model
    g_pCurrentModel->wifi_direct_params.uBytesReceived = s_WiFiDirectState.bytes_received;
    g_pCurrentModel->wifi_direct_params.uPacketsReceived = s_WiFiDirectState.packets_received;
    g_pCurrentModel->wifi_direct_params.uPacketsDropped = s_WiFiDirectState.packets_dropped;
    g_pCurrentModel->wifi_direct_params.uLastActivityTime = s_WiFiDirectState.last_receive_time_ms;
    
    // Get WiFi signal strength if connected
    if (s_WiFiDirectState.is_connected && wifi_link_is_connected()) {
        wifi_link_status_t wifiStatus;
        wifi_link_get_status(&wifiStatus);
        g_pCurrentModel->wifi_direct_params.iSignalStrength = wifiStatus.signal_strength;
    } else {
        g_pCurrentModel->wifi_direct_params.iSignalStrength = -127; // No signal
    }
}
