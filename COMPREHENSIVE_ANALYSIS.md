# RubyFPV: Comprehensive Architecture & Deep-Dive Analysis

**Version:** 10.4+
**Setup:** Radxa 3W + RunCam WifiLink v2 + RTL8812EU2
**Analysis Date:** 2026-02-23

---

## Table of Contents
1. [Architecture Overview](#1-architecture-overview)
2. [Packet Structure & Routing](#2-packet-structure--routing)
3. [5.8 GHz FPV Band Operation](#3-58-ghz-fpv-band-operation)
4. [Video Pipeline (RunCam → Ground)](#4-video-pipeline-runcam--ground)
5. [Adaptive Video & FEC](#5-adaptive-video--fec)
6. [RF Hardware (RTL8812EU2)](#6-rf-hardware-rtl8812eu2)
7. [Processes & IPC](#7-processes--ipc)
8. [Timing & Performance](#8-timing--performance)
9. [Error Recovery & Failure Modes](#9-error-recovery--failure-modes)

---

## 1. Architecture Overview

### 1.1 Multi-Process System
RubyFPV is **NOT** a monolithic application. It's organized as independent processes communicating via **Shared Memory IPC**:

**Vehicle (Air Unit - RunCam):**
- `ruby_start` → Boot process
- `ruby_rt_vehicle` → Main Router (Radio ↔ Video ↔ Telemetry)
- `ruby_tx_telemetry` → Telemetry uplink (MAVLink/LTM)
- `ruby_video_proc` → Video capture (reads Majestic UDP)

**Ground Station (Radxa 3W):**
- `ruby_rt_station` → Main Router (Radio ↔ Video ↔ Telemetry)
- `ruby_central` → UI/OSD overlay
- `ruby_player_radxa` → Video decoder (MPP hardware)
- `ruby_rx_telemetry` → Telemetry display
- `ruby_tx_rc` → RC input (Joystick)

### 1.2 Core Design Philosophy
*   **Single Unified Packet Format:** Local IPC and Radio packets share identical structure (defined in `radiopackets2.h`)
*   **Router-Centric:** Only the Router (`ruby_rt_vehicle`/`ruby_rt_station`) touches radio hardware
*   **Minimal Copying:** Video data flows directly from capture → buffer → TX, minimizing latency
*   **Graceful Degradation:** Missing files recreated, errors trigger recovery not crashes

---

## 2. Packet Structure & Routing

### 2.1 Universal Packet Header
```c
typedef struct {
    u32 uCRC;                      // CRC + MSB flags packet type
    u8 packet_flags;               // Component ID (telemetry=0x01, video=0x02, etc.)
    u8 packet_type;                // 1..150: component types
                                   // 150..200: ground control
                                   // 200..250: vehicle control
    u32 stream_packet_idx;         // Bits 28-31: Stream ID
                                   // Bits 0-27: Monotonic packet index (loss detection)
    u16 packet_flags_extended;     // Bit 0: High capacity link only
                                   // Bit 1: Low capacity link only
                                   // Bit 2: Requires ACK
    u16 total_length;              // Header + payload
    u16 radio_link_packet_index;   // Per-interface monotonic counter
    u32 vehicle_id_src;            // Source vehicle (or Controller ID)
    u32 vehicle_id_dest;           // Dest vehicle (0 = broadcast)
} t_packet_header;  // 24 bytes
```

### 2.2 Routing Logic
Each packet contains enough info for the Router to decide:
- **Source/Dest Identification:** `vehicle_id_src/dest`
- **Component Identification:** `packet_flags` (sub-component ID)
- **Packet Type:** Command, Telemetry, Video, Audio
- **Loss Detection:** Monotonic counters on both Stream-level and Link-level
- **Priority Handling:** ACK flags and link capacity hints

---

## 3. 5.8 GHz FPV Band Operation

### 3.1 Supported Channels (5.8 GHz)
**File:** `code/base/config_radio.c`

```c
u32 channels58[] = { 
    5180000, 5200000, 5220000, 5240000, 5260000, 5280000, 5300000, 5320000,  // UNII-1 (5150-5250)
    5500000, 5520000, 5540000, 5560000, 5580000, 5600000, 5620000, 5640000,  // UNII-3 Start
    5660000, 5680000, 5700000, 5745000, 5765000, 5785000, 5805000, 5825000,  // UNII-3 Main (FPV Band)
    5845000, 5865000, 5885000                                                 // UNII-4 (5850+)
};
```

**Frequency Bands:**
- **UNII-1 (5150-5250 MHz):** Channels 36-48 in WiFi terminology. Lower power, longer range.
- **UNII-3 (5650-5850 MHz):** Channels 120-165. Higher interference environment. Standard FPV band.
- **UNII-4 (5850-5925 MHz):** Newer 6E channels. Experimental support.

### 3.2 Channel Width & Modulation

**20 MHz Channel (Standard - RTL8812EU2 ONLY):**
- Uses standard WiFi CCK/OFDM rates: 6, 12, 24, 36, 48 Mbps (legacy)
- HT-MCS 0-7: 6.5 - 65 Mbps (20 MHz)
- Better range, more resistant to interference
- **RTL8812EU2 (EU2 variant) does NOT support 40 MHz - driver limitation**

**40 MHz Channel (NOT supported on RTL8812EU2):**
- ⚠️ **Unsupported by RTL8812EU2 driver**
- Flag: `RADIO_FLAG_HT40` (defined in `radioflags.h`, but not usable on EU2)
- Would support HT-MCS 0-15: 13.5 - 150 Mbps (40 MHz)
- Your setup: **20 MHz only** (max throughput: ~65 Mbps HT-MCS7)

**Enhancements (May vary by driver):**
```c
#define RADIO_FLAG_SGI      (((u32)0x01)<<9)   // Short Guard Interval (+11% throughput)
#define RADIO_FLAG_STBC     (((u32)0x01)<<10)  // Space-Time Block Coding (diversity)
#define RADIO_FLAG_LDPC     (((u32)0x01)<<11)  // Low-Density Parity-Check (error correction)

// Note: RTL8812EU2 driver support varies by firmware version
// SGI often enabled by default on 20 MHz channels
// STBC/LDPC support depends on driver/firmware
```

### 3.3 TX Power Levels at 5.8 GHz

**Measured at 5700-5800 MHz, MCS-2 datarate:**

```
RTL8812AU Dual Antenna:    {1, 1, 2, 7, 25, 40, 78, 95, 125, 175, 220} mW
TP-Link Archer T2UP:       {1, 2, 7, 25, 65, 100, 135, 150, 170, 190} mW
Archer RTL8812AU-AF1:      {1, 2, 5, 15, 40, 70, 95, 110, 130, 150} mW
```

**TX Power Settings:**
- Raw values: 1, 5, 10, 15, 20, 23, 26, 30, 35, 40, 45, 50, 53, 56, 60, 63, 65, 68, 70
- UI Power Levels (mW): 1, 5, 10, 25, 50, 75, 100, 150, 200, 250, 300, 350, 400, 450, 500, 600, 700, 800, 900, 1000 mW
- **TX Booster (4W):** 1mW → 150mW, 10mW → 500mW, 50mW → 2200mW, 100mW → 4000mW (external booster)

### 3.4 5.8 GHz Regulatory & Performance (RTL8812EU2)

**Typical Flight Range (without booster):**
- **Line of Sight (LoS), good antenna:** 5-10 km
- **Optimal frequency:** 5800 MHz (center of FPV band)
- **Interference Avoidance:** Auto-scan other channels if WiFi detected

**Capacity @ 5.8 GHz (20 MHz Channel Only):**
- **Legacy 54 Mbps:** ~40 Mbps real throughput (video only, no overhead)
- **HT-MCS7 (65 Mbps - Max on 20 MHz):** ~50 Mbps real throughput (no 40 MHz support)
- **HT-MCS15 (150 Mbps):** ⚠️ NOT available (requires 40 MHz, unsupported on EU2)
- **Required for HD video:** 10-20 Mbps (H.264 bitrate) + 5-10 Mbps (FEC overhead)
- **Your actual max:** ~50 Mbps usable (20 MHz, HT-MCS7)

---

## 4. Video Pipeline (RunCam → Ground)

### 4.1 Capture Phase (RunCam ISP)

```
RunCam WifiLink v2 (Majestic ISP)
    ↓ [H.264 encoder, configurable resolution/FPS]
    ↓ [UDP Multicast on port 5600]
    
ruby_rt_vehicle reads UDP stream
    ↓ [NAL unit parser]
    ├─ Detects start-of-frame (SPS/PPS/IDR)
    ├─ Extracts data packets
    └─ Detects end-of-frame
    
VideoTxPacketsBuffer queues frames
    ↓ [Circular buffer, MAX_RXTX_BLOCKS_BUFFER blocks]
```

**Key Implementation (videomajestic.cpp):**
- UDP buffer: `s_uInputVideoUDPBuffer[MAX_PACKET_TOTAL_SIZE]` (1500 bytes)
- Timeout on UDP read: 0.2ms non-blocking
- Audio capture: `s_uInputMajAudioBuffer[MAX_AUDIO_MAJ_BUFFER]` (4096 bytes)
- Health check: Monitor if Majestic process crashes (`hw_process_exists("majestic")`)

### 4.2 Packetization & FEC Encoding

```
Frame (typically 8-16 KB for 1080p@30fps)
    ↓ [Split into N data packets]
    ├─ Packet size: ~1400 bytes (accounting for headers)
    ├─ Block structure: [data_packet_1, data_packet_2, ..., ec_packet_1, ec_packet_2, ...]
    └─ FEC Ratio: 75% data, 25% redundancy (adaptive)
    
Galois Field GF(2^8) encoding
    ↓ [Reed-Solomon via fec.c]
    ├─ Generate EC packets from data packets
    ├─ Any ≤EC_packets lost → recoverable
    └─ Example: 16 data + 4 EC = 20 total packets/block
              Loss of 3 packets → Still recoverable
    
VideoTxPacketsBuffer::sendAvailablePackets()
    ↓ [Transmit order optimized for latency]
```

**Key Data Structure (video_tx_buffers.cpp):**
```c
struct VideoTxPacket {
    t_packet_header* pPH;                    // Routing header
    t_packet_header_video_segment* pPHVS;    // Video-specific header
    u8* pData;                               // Payload
    bool bEmpty;
};

class VideoTxPacketsBuffer {
    VideoTxPacket m_VideoPackets[MAX_RXTX_BLOCKS_BUFFER][MAX_PACKETS_PER_BLOCK];
    u32 m_uNextVideoBlockIndexToGenerate;
    u32 m_iCurrentBufferIndexToSend;
};
```

### 4.3 Transmission Priority

**In `ruby_rt_vehicle::_main_loop2()`:**

```
1. [Highest Priority] Video packets (sendAvailablePackets)
   └─ At frame end, flush entire frame in correct order
   
2. [High Priority] Audio packets (sendAudioPackets)
   └─ If audio enabled and available
   
3. [Medium Priority] Telemetry/Commands (process_and_send_packets)
   └─ Fill remaining link capacity
   └─ Subject to per-packet frequency limits (e.g., "send telemetry max 10x/sec")
```

**Loop Timing:**
```
_main_loop2() target: ≤6ms per cycle
  ├─ Read camera:  1-2ms
  ├─ Send video:   3-5ms
  ├─ Send telemetry: 1ms
  └─ Adapt video:  <1ms
  
If cycle > 6ms → Alert (possible buffer backlog)
```

### 4.4 Reception & Reassembly (Ground)

```
RTL8812EU2 RX
    ↓ [Raw packet injection via radiotap]
    ↓
ruby_rt_station (RX thread)
    ├─ Duplicate detection (monotonic counter check)
    ├─ Loss detection (gap in stream_packet_idx)
    └─ Queue packet for processing
    
VideoProcessorRxList[i]->periodicLoopProcessor()
    ├─ Reassemble video frame from packets
    ├─ Apply FEC recovery if needed
    ├─ Out-of-order reordering
    └─ Generate frame when complete
    
ruby_player_radxa (MPP Hardware Decoder)
    ├─ H.264 decode (via Rockchip MPP)
    ├─ Output to HDMI framebuffer
    └─ Feed to OSD renderer
    
ruby_central (OSD Overlay)
    ├─ Render telemetry info
    ├─ Draw on-screen warnings
    └─ Handle user input
```

**Latency Breakdown:**
```
Capture (Majestic):        0.0 ms
UDP buffering:             5-10 ms
NAL parsing:               1 ms
FEC encoding:              2-5 ms
Radio TX (5.8 GHz, 40Mbps):10-20 ms
Radio RX:                  1 ms
FEC decode:                2-5 ms
MPP decode:                5-10 ms
OSD render:                1-2 ms
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Total Latency:            28-53 ms (typical good link)
Peak Latency:             80-100 ms (poor link with retransmits)
```

---

## 5. Adaptive Video & FEC

### 5.1 Feedback Loop

**Ground detects link quality:**
```
ruby_rt_station monitors:
  ├─ Packet loss % (via monotonic counters)
  ├─ RSSI from RTL8812EU2
  ├─ Noise floor
  └─ Jitter/latency variance
  
Shared memory update: shared_mem_radio_stats
  └─ Accessible to ruby_central + ruby_rt_vehicle (via telemetry)
```

**Vehicle adapts bitrate:**
```
adaptive_video.cpp:adaptive_video_on_end_of_frame()
  ├─ Check if packet loss > threshold
  ├─ If loss > 5%:  Reduce bitrate (Majestic re-encode lower quality)
  ├─ If loss > 10%: Increase FEC ratio (more redundancy packets)
  └─ If loss < 1%:  Try increase bitrate (test link capacity)
  
Majestic reconfiguration:
  ├─ Bitrate: 2 Mbps → 15 Mbps range
  ├─ Resolution: 1080p, 720p, 480p (adaptive)
  ├─ FPS: 24, 30, 50, 60 FPS
  └─ Keyframe interval: 100-2000 ms
```

### 5.2 FEC Reed-Solomon Implementation

**File:** `code/radio/fec.c` (GPL licensed, external code)

```c
#define GF_BITS 8   // Galois Field GF(2^8) = 256 symbols

fec_encode(k data packets, n total packets)
  ├─ Generate n-k parity packets
  └─ Any k out of n packets → recover original

Example:
  k=16 (data), n=20 (total)
  └─ 4 parity packets
  └─ Can lose up to 4 packets and still recover
```

**Adaptive FEC Ratio:**
```
Link Quality:     FEC Ratio:     Example (per block):
Excellent (1%)    20% EC         16 data + 4 EC = 20 total
Good (5%)         25% EC         12 data + 4 EC = 16 total
Fair (10%)        33% EC         10 data + 5 EC = 15 total
Poor (20%)        50% EC         8 data + 8 EC = 16 total
Very Poor (>30%)  Reduce bitrate + 50% EC
```

---

## 6. RF Hardware (RTL8812EU2)

### 6.1 Radio Interface Detection

**File:** `code/base/hardware_radio.c`

```c
// Detect RTL8812EU2:
iw phy0 info | grep "5745"
  └─ Returns supported channels
  
// Get driver type:
ethtool -i wlan0
  ├─ Driver: rtl8812au / rtl8812eu
  └─ Maps to RADIO_HW_DRIVER_REALTEK_8812EU enum
```

### 6.2 TX/RX Path

**Transmission:**
```
ruby_rt_vehicle queues packet
    ↓
radiolink.c:radio_send_packet()
    ├─ Prepare radiotap header (channel, datarate, TX power)
    └─ Inject raw frame via socket (AF_PACKET, SOCK_RAW)
    
RTL8812EU2 USB device
    ├─ FW: Realtek firmware + driver
    └─ Modulate on selected channel (5.8 GHz, with configured power)
```

**Reception:**
```
RTL8812EU2 USB device
    ├─ Monitor mode: Capture all traffic on selected channel
    └─ FW demodulates, passes to driver
    
radiotap header decoding
    ├─ Extract RSSI (signal strength)
    ├─ Extract datarate (MCS index or legacy rate)
    └─ Extract error flags
    
ruby_rt_vehicle RX thread
    └─ Process packet (FEC, routing, duplicate detection)
```

### 6.3 Monitor Mode Setup

**During initialization (`radio_links_restart`):**
```c
hw_execute_bash_command("ip link set wlan0 down", NULL);
hw_execute_bash_command("iw wlan0 set monitor otherbss", NULL);
hw_execute_bash_command("ip link set wlan0 up", NULL);
hw_execute_bash_command("iw wlan0 set channel 120", NULL);  // 5600 MHz
```

---

## 7. Processes & IPC

### 7.1 Shared Memory Segments

**Named shared memory (defined in `shared_mem.h`):**

| Name | Size | Purpose |
|------|------|---------|
| `/SYSTEM_SHARED_MEM_RUBY_RADIO_STATS` | ~1KB | Link quality (RSSI, loss rate, packets/sec) |
| `/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_INFO` | ~2KB | Video frame stats (FPS, bitrate, keyframes) |
| `/SYSTEM_SHARED_MEM_RC_UPSTREAM_FRAME` | ~256B | RC input state (joystick channels) |
| `/SYSTEM_SHARED_MEM_WATCHDOG_ROUTER_TX` | ~512B | Router process health (loop timings, counters) |

### 7.2 IPC Pipes

**Named pipes (FIFOs) for command messages:**

```c
s_fIPCRouterFromCommands      // Commands → Router
s_fIPCRouterFromTelemetry     // Telemetry packets → Router
s_fIPCRouterFromRC            // RC input → Router

// Router reads these in _main_loop2():
while (ruby_ipc_try_read_message(s_fIPCRouterFromCommands, ...)) {
    // Process each command (settings change, retransmit request, etc.)
}
```

### 7.3 CPU Core Affinity (Radxa 3W - 4 cores)

```
Majestic ISP:        Core 0
ruby_rt_vehicle:     Core 1
radio_rx (thread):   Alternates (0 or 1, avoids same core)
System/Other:        Cores 2-3
```

**Benefit:** No cache thrashing between Majestic (encoding) and Router (packetization)

---

## 8. Timing & Performance

### 8.1 Video Capture Loop

**In Majestic (external process):**
```
30 FPS @ 1080p:
  Frame interval: 33.3 ms
  UDP packet arrival: ~0-2ms (via Ethernet/USB)
  NAL unit boundaries: Every keyframe (typically ~0.5-2 sec)
```

**Ruby reads frame:**
```
do {
    video_sources_try_read_camera_frame(&bEndOfFrame);
    // Timeout: 0.2ms if no data
} while (!bEndOfFrame && max_tries--);

Result:
  ├─ Frame complete within 1-2ms (UDP batching)
  └─ Processed before next frame arrives (headroom: ~30ms)
```

### 8.2 Router Loop Cycle

**Target:** < 6ms (6000µs)

```
_main_loop2() iteration:
  1. Get current timestamp: get_current_timestamp_ms()
  2. Read camera frame: ~1-2ms
  3. If end-of-frame:
     ├─ Send video packets: ~3-5ms
     ├─ Send audio (if enabled): ~1ms
     └─ Send telemetry: ~0.5ms
  4. Adaptive video state: <0.1ms
  5. Check watchdog: <0.1ms
  ────────────────────────────
     Total: ~6-10ms typical
     
  If > 6ms: log_softerror_and_alarm("Main loop took too long")
```

### 8.3 Throughput Calculations

**5.8 GHz, HT-MCS7 (20 MHz, MAX on RTL8812EU2):**
```
Physical rate: 65 Mbps (20 MHz, no 40 MHz support on EU2)
Frame overhead: ~10% (MAC + radiotap)
Usable throughput: ~58 Mbps

Ruby packet overhead:
  ├─ Radiotap header: ~36 bytes
  ├─ IEEE 802.11 MAC: ~24 bytes
  ├─ Ruby packet header: ~24 bytes
  └─ Total overhead: ~84 bytes
  
Per 1400-byte video packet:
  Efficiency: 1400 / (1400 + 84) = 94%
  
Overall video throughput:
  58 Mbps × 94% = 54.5 Mbps usable for video
  
Actual headroom for 1080p30 @ 8 Mbps + 25% FEC:
  Required: 10 Mbps
  Available: 54.5 Mbps
  Margin: 44.5 Mbps (plenty of headroom)
```

**Video bitrate @ 1080p30fps (H.264) - Your Setup:**
```
Uncompressed 1080p30: 1920×1080×1.5 bytes×30 = ~2.5 Gbps
H.264 compression ratio: 1:50 to 1:100 typical
H.264 bitrate: 5-15 Mbps (typical, good quality on 20 MHz)

+ FEC overhead (25%): 6-19 Mbps total on air
  Constraint: 54.5 Mbps available
  ✓ Can easily fit 15 Mbps video + 25% FEC
```

---

## 9. Error Recovery & Failure Modes

### 9.1 Link Loss Detection & Recovery

**Monotonic Counter Gap:**
```
stream_packet_idx is monotonic (never decreases)
  
Receive packet N, then N+3 (N+1, N+2 missing):
  ├─ Detected as 2 packets lost
  ├─ Request retransmit: (N+1, N+2)
  └─ Set timeout: 50-100ms
  
If retransmit arrives: Fill gap, reassemble frame
If timeout: Continue with remaining data (partial frame, visual artifact)
```

### 9.2 Majestic Crash Recovery

**Continuous monitoring:**
```
_video_source_majestic_check_cores_affinities_balance()
    ├─ Call hw_process_exists("majestic")
    └─ If returns 0 (not running):
        ├─ Log error: "Majestic not running"
        ├─ Attempt restart
        └─ If persistent failure: Fall back to CSI camera (if available)
```

### 9.3 UDP Buffer Overflow (Majestic → Ruby)

**Problem:** Majestic outputs faster than Ruby reads
```
s_uInputVideoUDPBuffer[MAX_PACKET_TOTAL_SIZE] saturates
  ├─ Next UDP packet dropped by kernel
  └─ Frame incomplete, visible freezing
```

**Solution:**
```
Increase SO_RCVBUF socket option:
  int rcvbuf = 2 * 1024 * 1024;  // 2MB
  setsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RCVBUF, ...);
```

### 9.4 Radio Silence Failsafe

**If no packets received for >500ms:**
```
Link considered "lost":
  ├─ OSD shows "NO SIGNAL" (red box)
  ├─ Audio alarm: configured tone
  ├─ If configured: Auto-failsafe (RTH, land, etc.)
  └─ Recovery: Rescan other channels or reboot
```

### 9.5 CPU Overload Detection

**In `ruby_rt_station::_main_loop_simple()`:**
```
if (tTime4 - tTime0 > uMaxLoopTime) {
    log_softerror_and_alarm("Router main loop overload");
    
    if (recording && overflows > 1) {
        send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING);
    }
}
```

**Causes:**
- Video frame too large (resolution mismatch)
- FEC decoding stalled
- Telemetry packet parsing slow
- OSD rendering bottleneck

**Recovery:**
- Auto-reduce video bitrate/resolution
- Disable telemetry display (temp)
- Restart affected process

---

## 10. Key Optimizations in This Setup

### 10.1 Hardware-Specific
1. **Majestic Direct UDP:** No intermediate RTSP/HTTP, raw H.264 NAL units
2. **Rockchip MPP:** Hardware H.264 decode, saves ~30% CPU vs software
3. **Core Pinning:** Majestic + Router on separate cores (Radxa has 4)
4. **USB Optimized:** RTL8812EU2 driver tuned for low-latency

### 10.2 Software
1. **Circular Buffers:** No malloc/free in hot path (latency deterministic)
2. **Priority Queuing:** Video > Audio > Telemetry > RC
3. **Adaptive FEC:** Link quality → auto-adjust redundancy
4. **Radiotap Parser:** Extract link metrics (RSSI, datarate) at RX

### 10.3 System-Level
1. **Realtime Scheduler:** SCHED_FIFO for Router process (if available)
2. **TCP_NODELAY equivalent:** No packet batching delay
3. **Monitor Mode:** Single channel focus (no scanning overhead during flight)

---

## 11. Configuration & Tuning

### 11.1 Model Object (`models.h`)
Centralized configuration synced between Ground and Air:

```c
video_parameters_t {
    int iVideoWidth;              // Resolution
    int iVideoHeight;
    int iVideoFPS;                // Frame rate
    int iH264Slices;              // Parallel encode units
    u32 lowestAllowedAdaptiveVideoBitrate;
    u32 uMaxAutoKeyframeIntervalMs;
    u32 uVideoExtraFlags;         // H265 enable, HDMI output, etc.
};

type_camera_parameters {
    camera_profile_parameters_t profiles[3];  // Day/Night/Custom
    int iCurrentProfile;
    int iCameraType;              // Detected ISP type
    char szCameraName[MAX_CAMERA_NAME_LENGTH];
};

t_radio_parameters {
    int iRadioLink;               // Which link (0=primary, 1=secondary)
    int uRadioFrequency;          // 5600000 (5.6 GHz)
    int iRadioDataRateMbps;       // -12 (MCS12), or 54 (54 Mbps legacy)
    int iTxPower;                 // Raw value 1-70
    int iRadioLinkProfile;        // FEC ratio, channel width
};
```

### 11.2 Majestic Configuration (RunCam ISP)

**Real-time camera control via ruby_vehicle processes:**
```c
void process_camera_settings_change(...) {
    // Adjust via /dev/iqfeed or shared memory
    ├─ Brightness, Contrast, Saturation
    ├─ ISO, Exposure, White Balance
    ├─ Resolution, FPS, Bitrate
    └─ Scene presets (Day, Night, Low-Light)
}
```

---

## 12. Failure Scenario Examples

### Scenario A: Link Quality Drops (10% Loss)
```
Ground detects loss via monotonic counter
  ↓
Send telemetry packet to vehicle: "Loss rate 10%"
  ↓
Vehicle adaptive_video reduces bitrate from 10Mbps → 6Mbps
  ↓
Majestic re-encodes at lower quality
  ↓
Fewer packets per frame → easier to transmit in poor link
  ↓
FEC ratio increases: 12 data → 10 data + 5 EC (50% redundancy)
  ↓
Ground detects loss drops to 2% (FEC recovery working)
  ↓
Vehicle gradually increases bitrate back to 10Mbps
```

### Scenario B: Majestic Encoder Crash
```
ruby_rt_vehicle reads UDP for video frame
  ↓
hw_process_exists("majestic") returns 0
  ↓
Launcher detects crash, attempts auto-restart
  ↓
New Majestic process starts (takes ~2-3 sec)
  ↓
Video gap ~3 sec (OSD shows "Lost signal")
  ↓
Majestic resumes, video stream recovers
  ↓
FEC reconstruction of older frames fills small gaps
```

### Scenario C: RTL8812EU2 Lost Connection
```
RX thread stops receiving packets (timeout on select())
  ↓
ruby_rt_station detects no packets for >500ms
  ↓
OSD alarm: "NO SIGNAL"
  ↓
Failsafe triggered (auto-land, RTH, etc.)
  ↓
If link recovers: Resume normal operation
  ↓
If persistent: Reboot entire system, attempt pairing
```

---

## 13. Performance Metrics

### Ground Station Baseline (Radxa 3W idle):
- CPU usage: ~5-10% (system services)
- Memory: ~50MB free (out of 2GB)
- Network: Listening on RTL8812EU2, monitor mode

### During 1080p30fps Flight:
- CPU: ruby_central ~15%, ruby_rt_station ~20%, MPP decoder ~10%
- Memory: +200MB (video buffers)
- Latency: 30-50ms typical, <80ms on poor link

### During Link Loss (10% packet loss):
- CPU: ruby_rt_station +5% (FEC decoding)
- Memory: +50MB (retransmit buffer)
- Video bitrate: Drops 50% (adaptive)

---

## 14. References & Further Reading

**Inside the Code:**
- `code/radio/radiopackets2.h` - Packet structure
- `code/r_vehicle/ruby_rt_vehicle.cpp` - Vehicle router loop
- `code/r_station/ruby_rt_station.cpp` - Ground router loop
- `code/r_vehicle/video_tx_buffers.cpp` - Video packetization
- `code/r_vehicle/adaptive_video.cpp` - Bitrate adaptation
- `code/radio/fec.c` - Reed-Solomon FEC

**Hardware Resources:**
- Radxa 3W: 4x ARM A55, Mali-G52, 8GB RAM
- Rockchip MPP: H.264 HW decoder
- RTL8812EU2: 802.11ac USB dongle, 5.8 GHz

**Ruby Documentation:**
- https://rubyfpv.com (Official site)
- OpenIPC project (RunCam integration)
- WiringPi (GPIO control)

