#include "wifi_link.h"
#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hardware_procs.h"
#include "utils.h"
#include "../common/string_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

static wifi_link_config_t s_WifiConfig;
static wifi_link_status_t s_WifiStatus;
static char s_WifiInterface[32] = "wlan0";
static bool s_bInitialized = false;
static bool s_bRunning = false;

// Helper function to execute system commands
static int wifi_execute_command(const char* command)
{
    log_line("[WiFiLink] Executing: %s", command);
    
    int result = system(command);
    if (result != 0) {
        log_softerror_and_alarm("[WiFiLink] Command failed with code %d: %s", WEXITSTATUS(result), command);
        return -1;
    }
    
    return 0;
}

// Helper function to check if interface exists
static bool wifi_interface_exists(const char* interface)
{
    char szCommand[256];
    sprintf(szCommand, "ip link show %s > /dev/null 2>&1", interface);
    return (system(szCommand) == 0);
}

// Helper function to bring interface up/down
static bool wifi_interface_set_state(const char* interface, bool up)
{
    char szCommand[256];
    sprintf(szCommand, "ip link set %s %s", interface, up ? "up" : "down");
    return (wifi_execute_command(szCommand) == 0);
}

bool wifi_link_init()
{
    if (s_bInitialized) {
        return true;
    }
    
    log_line("[WiFiLink] Initializing WiFi link subsystem");
    
    // Initialize configuration with defaults
    memset(&s_WifiConfig, 0, sizeof(s_WifiConfig));
    s_WifiConfig.mode = WIFI_LINK_MODE_NONE;
    strcpy(s_WifiConfig.ssid, "RubyFPV_Direct");
    strcpy(s_WifiConfig.password, "rubyfpv123");
    s_WifiConfig.channel = 6;
    strcpy(s_WifiConfig.ip_address, "192.168.42.1");
    strcpy(s_WifiConfig.netmask, "255.255.255.0");
    s_WifiConfig.dhcp_enabled = 1;
    strcpy(s_WifiConfig.dhcp_start, "192.168.42.100");
    strcpy(s_WifiConfig.dhcp_end, "192.168.42.200");
    s_WifiConfig.port = 5600;
    
    // Initialize status
    memset(&s_WifiStatus, 0, sizeof(s_WifiStatus));
    s_WifiStatus.status = WIFI_LINK_STATUS_DISCONNECTED;
    
    // Check if WiFi interface exists
    if (!wifi_interface_exists(s_WifiInterface)) {
        log_softerror_and_alarm("[WiFiLink] WiFi interface %s not found", s_WifiInterface);
        // Try common alternatives
        if (wifi_interface_exists("wlan1")) {
            strcpy(s_WifiInterface, "wlan1");
            log_line("[WiFiLink] Using alternative interface wlan1");
        } else {
            return false;
        }
    }
    
    s_bInitialized = true;
    log_line("[WiFiLink] WiFi link subsystem initialized using interface %s", s_WifiInterface);
    return true;
}

bool wifi_link_configure(const wifi_link_config_t* config)
{
    if (!config) {
        return false;
    }
    
    if (!s_bInitialized) {
        if (!wifi_link_init()) {
            return false;
        }
    }
    
    if (s_bRunning) {
        log_line("[WiFiLink] Stopping current WiFi link before reconfiguration");
        wifi_link_stop();
    }
    
    memcpy(&s_WifiConfig, config, sizeof(wifi_link_config_t));
    
    log_line("[WiFiLink] WiFi link configured - Mode: %d, SSID: %s, Channel: %d, IP: %s, Port: %d",
             s_WifiConfig.mode, s_WifiConfig.ssid, s_WifiConfig.channel, 
             s_WifiConfig.ip_address, s_WifiConfig.port);
    
    return true;
}

bool wifi_link_start()
{
    if (!s_bInitialized) {
        log_softerror_and_alarm("[WiFiLink] WiFi link not initialized");
        return false;
    }
    
    if (s_bRunning) {
        log_line("[WiFiLink] WiFi link already running");
        return true;
    }
    
    if (s_WifiConfig.mode == WIFI_LINK_MODE_NONE) {
        log_line("[WiFiLink] WiFi link mode is NONE, not starting");
        return false;
    }
    
    log_line("[WiFiLink] Starting WiFi link in %s mode",
             s_WifiConfig.mode == WIFI_LINK_MODE_AP ? "AP" : "Client");
    
    // Kill any existing processes that might interfere
    wifi_execute_command("pkill -9 hostapd > /dev/null 2>&1");
    wifi_execute_command("pkill -9 wpa_supplicant > /dev/null 2>&1");
    wifi_execute_command("pkill -9 dnsmasq > /dev/null 2>&1");
    hardware_sleep_ms(100);
    
    // Bring interface down first
    wifi_interface_set_state(s_WifiInterface, false);
    hardware_sleep_ms(100);
    
    // Set interface to managed mode
    char szCommand[512];
    sprintf(szCommand, "iw dev %s set type managed", s_WifiInterface);
    wifi_execute_command(szCommand);
    
    // Bring interface up
    wifi_interface_set_state(s_WifiInterface, true);
    hardware_sleep_ms(100);
    
    if (s_WifiConfig.mode == WIFI_LINK_MODE_AP) {
        // Create hostapd configuration
        FILE* fp = fopen("/tmp/ruby_hostapd.conf", "w");
        if (!fp) {
            log_softerror_and_alarm("[WiFiLink] Failed to create hostapd config");
            return false;
        }
        
        fprintf(fp, "interface=%s\n", s_WifiInterface);
        fprintf(fp, "driver=nl80211\n");
        fprintf(fp, "ssid=%s\n", s_WifiConfig.ssid);
        fprintf(fp, "hw_mode=g\n");
        fprintf(fp, "channel=%d\n", s_WifiConfig.channel);
        fprintf(fp, "wmm_enabled=0\n");
        fprintf(fp, "macaddr_acl=0\n");
        fprintf(fp, "auth_algs=1\n");
        fprintf(fp, "ignore_broadcast_ssid=0\n");
        
        if (strlen(s_WifiConfig.password) >= 8) {
            fprintf(fp, "wpa=2\n");
            fprintf(fp, "wpa_passphrase=%s\n", s_WifiConfig.password);
            fprintf(fp, "wpa_key_mgmt=WPA-PSK\n");
            fprintf(fp, "wpa_pairwise=TKIP\n");
            fprintf(fp, "rsn_pairwise=CCMP\n");
        }
        
        fclose(fp);
        
        // Start hostapd
        sprintf(szCommand, "hostapd -B /tmp/ruby_hostapd.conf > /dev/null 2>&1");
        if (wifi_execute_command(szCommand) != 0) {
            log_softerror_and_alarm("[WiFiLink] Failed to start hostapd");
            return false;
        }
        
        hardware_sleep_ms(2000); // Wait for AP to start
        
        // Configure IP address
        sprintf(szCommand, "ip addr add %s/%d dev %s", 
                s_WifiConfig.ip_address, 24, s_WifiInterface);
        wifi_execute_command(szCommand);
        
        // Start DHCP server if enabled
        if (s_WifiConfig.dhcp_enabled) {
            // Create dnsmasq configuration
            fp = fopen("/tmp/ruby_dnsmasq.conf", "w");
            if (fp) {
                fprintf(fp, "interface=%s\n", s_WifiInterface);
                fprintf(fp, "dhcp-range=%s,%s,12h\n", 
                        s_WifiConfig.dhcp_start, s_WifiConfig.dhcp_end);
                fprintf(fp, "dhcp-option=3,%s\n", s_WifiConfig.ip_address); // Gateway
                fprintf(fp, "dhcp-option=6,8.8.8.8,8.8.4.4\n"); // DNS
                fprintf(fp, "server=8.8.8.8\n");
                fprintf(fp, "log-queries\n");
                fprintf(fp, "log-dhcp\n");
                fclose(fp);
                
                sprintf(szCommand, "dnsmasq -C /tmp/ruby_dnsmasq.conf > /dev/null 2>&1");
                wifi_execute_command(szCommand);
            }
        }
        
        s_WifiStatus.status = WIFI_LINK_STATUS_CONNECTED;
        strcpy(s_WifiStatus.current_ip, s_WifiConfig.ip_address);
        
    } else if (s_WifiConfig.mode == WIFI_LINK_MODE_CLIENT) {
        // Create wpa_supplicant configuration
        FILE* fp = fopen("/tmp/ruby_wpa_supplicant.conf", "w");
        if (!fp) {
            log_softerror_and_alarm("[WiFiLink] Failed to create wpa_supplicant config");
            return false;
        }
        
        fprintf(fp, "ctrl_interface=/var/run/wpa_supplicant\n");
        fprintf(fp, "update_config=1\n");
        fprintf(fp, "network={\n");
        fprintf(fp, "    ssid=\"%s\"\n", s_WifiConfig.ssid);
        if (strlen(s_WifiConfig.password) >= 8) {
            fprintf(fp, "    psk=\"%s\"\n", s_WifiConfig.password);
        } else {
            fprintf(fp, "    key_mgmt=NONE\n");
        }
        fprintf(fp, "}\n");
        fclose(fp);
        
        // Start wpa_supplicant
        sprintf(szCommand, "wpa_supplicant -B -i %s -c /tmp/ruby_wpa_supplicant.conf > /dev/null 2>&1",
                s_WifiInterface);
        if (wifi_execute_command(szCommand) != 0) {
            log_softerror_and_alarm("[WiFiLink] Failed to start wpa_supplicant");
            return false;
        }
        
        s_WifiStatus.status = WIFI_LINK_STATUS_CONNECTING;
        
        // Wait for connection
        int retries = 30; // 30 seconds timeout
        while (retries > 0) {
            hardware_sleep_ms(1000);
            
            // Check if connected
            sprintf(szCommand, "wpa_cli -i %s status | grep 'wpa_state=COMPLETED' > /dev/null 2>&1",
                    s_WifiInterface);
            if (system(szCommand) == 0) {
                log_line("[WiFiLink] Connected to WiFi network");
                break;
            }
            
            retries--;
        }
        
        if (retries == 0) {
            log_softerror_and_alarm("[WiFiLink] Failed to connect to WiFi network");
            wifi_execute_command("pkill -9 wpa_supplicant > /dev/null 2>&1");
            s_WifiStatus.status = WIFI_LINK_STATUS_ERROR;
            return false;
        }
        
        // Get IP address via DHCP
        sprintf(szCommand, "dhclient %s > /dev/null 2>&1", s_WifiInterface);
        wifi_execute_command(szCommand);
        
        // Get assigned IP address
        char szOutput[256];
        sprintf(szCommand, "ip addr show %s | grep 'inet ' | awk '{print $2}' | cut -d'/' -f1",
                s_WifiInterface);
        FILE* fp_cmd = popen(szCommand, "r");
        if (fp_cmd) {
            if (fgets(szOutput, sizeof(szOutput), fp_cmd)) {
                szOutput[strcspn(szOutput, "\n")] = 0;
                strcpy(s_WifiStatus.current_ip, szOutput);
            }
            pclose(fp_cmd);
        }
        
        s_WifiStatus.status = WIFI_LINK_STATUS_CONNECTED;
    }
    
    s_bRunning = true;
    s_WifiStatus.last_activity_ms = get_current_timestamp_ms();
    
    log_line("[WiFiLink] WiFi link started successfully - IP: %s", s_WifiStatus.current_ip);
    return true;
}

void wifi_link_stop()
{
    if (!s_bRunning) {
        return;
    }
    
    log_line("[WiFiLink] Stopping WiFi link");
    
    // Kill processes
    wifi_execute_command("pkill -9 hostapd > /dev/null 2>&1");
    wifi_execute_command("pkill -9 wpa_supplicant > /dev/null 2>&1");
    wifi_execute_command("pkill -9 dnsmasq > /dev/null 2>&1");
    wifi_execute_command("pkill -9 dhclient > /dev/null 2>&1");
    
    // Remove IP address
    char szCommand[256];
    sprintf(szCommand, "ip addr flush dev %s", s_WifiInterface);
    wifi_execute_command(szCommand);
    
    // Bring interface down
    wifi_interface_set_state(s_WifiInterface, false);
    
    // Clean up temp files
    unlink("/tmp/ruby_hostapd.conf");
    unlink("/tmp/ruby_wpa_supplicant.conf");
    unlink("/tmp/ruby_dnsmasq.conf");
    
    s_bRunning = false;
    s_WifiStatus.status = WIFI_LINK_STATUS_DISCONNECTED;
    memset(s_WifiStatus.current_ip, 0, sizeof(s_WifiStatus.current_ip));
    s_WifiStatus.num_clients = 0;
    
    log_line("[WiFiLink] WiFi link stopped");
}

void wifi_link_get_status(wifi_link_status_t* status)
{
    if (!status) {
        return;
    }
    
    // Update signal strength if connected
    if (s_bRunning && s_WifiStatus.status == WIFI_LINK_STATUS_CONNECTED) {
        // Get signal strength
        char szCommand[256];
        char szOutput[256];
        
        if (s_WifiConfig.mode == WIFI_LINK_MODE_CLIENT) {
            // Client mode - get signal from wpa_cli
            sprintf(szCommand, "wpa_cli -i %s signal_poll | grep RSSI | awk -F= '{print $2}'", s_WifiInterface);
            FILE* fp = popen(szCommand, "r");
            if (fp) {
                if (fgets(szOutput, sizeof(szOutput), fp)) {
                    szOutput[strcspn(szOutput, "\n")] = 0;
                    int rssi = atoi(szOutput);
                    if (rssi != 0 && rssi > -127 && rssi < 0) {
                        s_WifiStatus.signal_strength = rssi;
                    }
                }
                pclose(fp);
            }
        } else if (s_WifiConfig.mode == WIFI_LINK_MODE_AP) {
            // AP mode - get average signal from connected clients
            sprintf(szCommand, "iw dev %s station dump | grep 'signal:' | awk '{sum+=$2; n++} END {if(n>0) print sum/n; else print -127}'", s_WifiInterface);
            FILE* fp = popen(szCommand, "r");
            if (fp) {
                if (fgets(szOutput, sizeof(szOutput), fp)) {
                    szOutput[strcspn(szOutput, "\n")] = 0;
                    int rssi = atoi(szOutput);
                    if (rssi != 0 && rssi > -127 && rssi < 0) {
                        s_WifiStatus.signal_strength = rssi;
                    }
                }
                pclose(fp);
            }
        }
        
        // Update byte counters
        sprintf(szCommand, "cat /sys/class/net/%s/statistics/rx_bytes 2>/dev/null", s_WifiInterface);
        FILE* fp = popen(szCommand, "r");
        if (fp) {
            if (fgets(szOutput, sizeof(szOutput), fp)) {
                u64 rx_bytes = strtoull(szOutput, NULL, 10);
                if (rx_bytes > 0) {
                    s_WifiStatus.bytes_received = (u32)(rx_bytes & 0xFFFFFFFF);
                }
            }
            pclose(fp);
        }
        
        sprintf(szCommand, "cat /sys/class/net/%s/statistics/tx_bytes 2>/dev/null", s_WifiInterface);
        fp = popen(szCommand, "r");
        if (fp) {
            if (fgets(szOutput, sizeof(szOutput), fp)) {
                u64 tx_bytes = strtoull(szOutput, NULL, 10);
                if (tx_bytes > 0) {
                    s_WifiStatus.bytes_sent = (u32)(tx_bytes & 0xFFFFFFFF);
                }
            }
            pclose(fp);
        }
    }
    
    memcpy(status, &s_WifiStatus, sizeof(wifi_link_status_t));
    
    // Update connected clients for AP mode
    if (s_bRunning && s_WifiConfig.mode == WIFI_LINK_MODE_AP) {
        s_WifiStatus.num_clients = 0;
        
        // Get connected stations
        char szCommand[256];
        sprintf(szCommand, "iw dev %s station dump | grep Station | awk '{print $2}'", s_WifiInterface);
        FILE* fp = popen(szCommand, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp) && s_WifiStatus.num_clients < 8) {
                line[strcspn(line, "\n")] = 0;
                if (strlen(line) > 0) {
                    // Get IP address for MAC
                    char szArpCmd[256];
                    sprintf(szArpCmd, "arp -n | grep %s | awk '{print $1}'", line);
                    FILE* fp_arp = popen(szArpCmd, "r");
                    if (fp_arp) {
                        if (fgets(line, sizeof(line), fp_arp)) {
                            line[strcspn(line, "\n")] = 0;
                            strcpy(s_WifiStatus.connected_clients[s_WifiStatus.num_clients], line);
                            s_WifiStatus.num_clients++;
                        }
                        pclose(fp_arp);
                    }
                }
            }
            pclose(fp);
        }
        
        memcpy(status, &s_WifiStatus, sizeof(wifi_link_status_t));
    }
}

bool wifi_link_is_connected()
{
    return (s_WifiStatus.status == WIFI_LINK_STATUS_CONNECTED);
}

const char* wifi_link_get_interface()
{
    return s_WifiInterface;
}

bool wifi_link_set_interface(const char* interface)
{
    if (!interface || strlen(interface) == 0) {
        return false;
    }
    
    if (s_bRunning) {
        log_softerror_and_alarm("[WiFiLink] Cannot change interface while WiFi link is running");
        return false;
    }
    
    if (!wifi_interface_exists(interface)) {
        log_softerror_and_alarm("[WiFiLink] Interface %s does not exist", interface);
        return false;
    }
    
    strncpy(s_WifiInterface, interface, sizeof(s_WifiInterface) - 1);
    s_WifiInterface[sizeof(s_WifiInterface) - 1] = 0;
    
    log_line("[WiFiLink] WiFi interface set to %s", s_WifiInterface);
    return true;
}

int wifi_link_scan_networks(char* buffer, int max_size)
{
    if (!buffer || max_size <= 0) {
        return -1;
    }
    
    if (!s_bInitialized) {
        if (!wifi_link_init()) {
            return -1;
        }
    }
    
    if (s_bRunning) {
        log_line("[WiFiLink] Cannot scan while WiFi link is running");
        return -1;
    }
    
    // Bring interface up
    wifi_interface_set_state(s_WifiInterface, true);
    hardware_sleep_ms(100);
    
    // Scan for networks
    char szCommand[256];
    sprintf(szCommand, "iw dev %s scan | grep -E 'SSID:|signal:' | paste -d ' ' - - | awk '{print $2\" \"$4}' | head -20",
            s_WifiInterface);
    
    FILE* fp = popen(szCommand, "r");
    if (!fp) {
        return -1;
    }
    
    int count = 0;
    int written = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), fp) && written < max_size - 1) {
        int len = strlen(line);
        if (written + len < max_size) {
            strcpy(buffer + written, line);
            written += len;
            count++;
        }
    }
    
    pclose(fp);
    buffer[written] = 0;
    
    return count;
}

int wifi_link_get_supported_channels(int* channels, int max_channels)
{
    if (!channels || max_channels <= 0) {
        return -1;
    }
    
    if (!s_bInitialized) {
        if (!wifi_link_init()) {
            return -1;
        }
    }
    
    // For now, return standard 2.4GHz channels
    int standard_channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    int count = sizeof(standard_channels) / sizeof(standard_channels[0]);
    
    if (count > max_channels) {
        count = max_channels;
    }
    
    memcpy(channels, standard_channels, count * sizeof(int));
    return count;
}

bool wifi_link_save_config(const char* filename)
{
    if (!filename) {
        return false;
    }
    
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        log_softerror_and_alarm("[WiFiLink] Failed to save config to %s", filename);
        return false;
    }
    
    fprintf(fp, "# RubyFPV WiFi Direct Configuration\n");
    fprintf(fp, "mode=%d\n", s_WifiConfig.mode);
    fprintf(fp, "ssid=%s\n", s_WifiConfig.ssid);
    fprintf(fp, "password=%s\n", s_WifiConfig.password);
    fprintf(fp, "channel=%d\n", s_WifiConfig.channel);
    fprintf(fp, "ip_address=%s\n", s_WifiConfig.ip_address);
    fprintf(fp, "netmask=%s\n", s_WifiConfig.netmask);
    fprintf(fp, "dhcp_enabled=%d\n", s_WifiConfig.dhcp_enabled);
    fprintf(fp, "dhcp_start=%s\n", s_WifiConfig.dhcp_start);
    fprintf(fp, "dhcp_end=%s\n", s_WifiConfig.dhcp_end);
    fprintf(fp, "port=%d\n", s_WifiConfig.port);
    
    fclose(fp);
    
    log_line("[WiFiLink] Configuration saved to %s", filename);
    return true;
}

bool wifi_link_load_config(const char* filename)
{
    if (!filename) {
        return false;
    }
    
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        log_line("[WiFiLink] Config file %s not found, using defaults", filename);
        return false;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        char key[64], value[192];
        if (sscanf(line, "%[^=]=%s", key, value) == 2) {
            if (strcmp(key, "mode") == 0) {
                s_WifiConfig.mode = atoi(value);
            } else if (strcmp(key, "ssid") == 0) {
                strncpy(s_WifiConfig.ssid, value, sizeof(s_WifiConfig.ssid) - 1);
            } else if (strcmp(key, "password") == 0) {
                strncpy(s_WifiConfig.password, value, sizeof(s_WifiConfig.password) - 1);
            } else if (strcmp(key, "channel") == 0) {
                s_WifiConfig.channel = atoi(value);
            } else if (strcmp(key, "ip_address") == 0) {
                strncpy(s_WifiConfig.ip_address, value, sizeof(s_WifiConfig.ip_address) - 1);
            } else if (strcmp(key, "netmask") == 0) {
                strncpy(s_WifiConfig.netmask, value, sizeof(s_WifiConfig.netmask) - 1);
            } else if (strcmp(key, "dhcp_enabled") == 0) {
                s_WifiConfig.dhcp_enabled = atoi(value);
            } else if (strcmp(key, "dhcp_start") == 0) {
                strncpy(s_WifiConfig.dhcp_start, value, sizeof(s_WifiConfig.dhcp_start) - 1);
            } else if (strcmp(key, "dhcp_end") == 0) {
                strncpy(s_WifiConfig.dhcp_end, value, sizeof(s_WifiConfig.dhcp_end) - 1);
            } else if (strcmp(key, "port") == 0) {
                s_WifiConfig.port = atoi(value);
            }
        }
    }
    
    fclose(fp);
    
    log_line("[WiFiLink] Configuration loaded from %s", filename);
    return true;
}
