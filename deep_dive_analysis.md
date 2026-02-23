# RubyFPV Deep Dive Analysis
## Radxa 3W + RunCam WifiLink v2 + RTL8812EU2 Setup

**Analysis Date:** 2026-02-23

---

## 1. Hardware Integration

### Ground Station: Radxa 3W
*   **SoC:** Rockchip (ARM-based)
*   **Rendering:** Uses DRM + Cairo (via `renderer/drm_core.cpp`, `renderer/render_engine_cairo.cpp`)
*   **Video Decoding:** Rockchip MPP (Media Process Platform) via `ruby_player_radxa`
*   **Radio:** RTL8812EU2 USB adapter
*   **Process:** `ruby_rt_station` (Router), `ruby_central` (UI/OSD)

### Air Unit: RunCam WifiLink v2
*   **Camera:** RunCam ISP with **Majestic** firmware
*   **Video Output:** H.264 stream via UDP port 5600 (multicast/UDP)
*   **Video Capture:** `video_source_majestic.cpp` reads UDP packets from Majestic
*   **Process:** `ruby_rt_vehicle` (Router), `ruby_tx_telemetry` (Telemetry TX)

### Radio Module: RTL8812EU2
*   **Driver:** `RADIO_HW_DRIVER_REALTEK_8812EU` (in `string_utils.c`)
*   **Transmission:** Raw packet injection via `radiotap` headers
*   **Supported Models:** `CARD_MODEL_BLUE_8812EU` and generic variants
*   **Frequencies:** 2.4GHz band (WiFi channels 1-14)

---

## 2. Video Pipeline (RunCam WifiLink → Ground)

### 2.1 Capture Phase (Vehicle - Air)
```
Majestic ISP (OpenIPC/RunCam)
    ↓ [UDP:5600 H.264 stream]
video_source_majestic.cpp reads UDP packets
    ↓
Parse NAL units (Start, Intermediate, End)
    ↓ [s_uOutputUDPNALFrameSegment]
VideoTxPacketsBuffer queues frames
    ↓ [Circular buffer: MAX_RXTX_BLOCKS_BUFFER blocks]
```

**Key Details:**
- Majestic outputs **RTP/UDP** video stream (not native H.264 file format).
- Ruby extracts NAL units from RTP packets via `_video_source_majestic_check_cores_affinities_balance()`.
- UDP buffer size: `MAX_PACKET_TOTAL_SIZE` (likely 1500 bytes MTU).
- Audio also captured: `s_uInputMajAudioBuffer[MAX_AUDIO_MAJ_BUFFER]` (4096 bytes).

### 2.2 Packetization & FEC (Vehicle Router)
```
VideoTxPacketsBuffer::sendAvailablePackets()
    ↓
For each video frame:
    - Split into BLOCKS (data packets + EC packets)
    - EC/FEC applied via `code/radio/fec.c` (Galois Field GF(2^8))
    - Generate retransmittable EC packets
    ↓ [t_packet_header_video_segment]
    ↓
RadioPacket (with t_packet_header):
    - vehicle_id_src/dest
    - stream_packet_idx (Block + Packet index)
    - packet_type: VIDEO
    - radio_link_packet_index (for link quality stats)
    ↓
Radio TX via RTL8812EU2
```

**Key Details:**
- **Block-based FEC:** Each block = (data_packets + ec_packets)
- **Data Packets:** Actual video NAL data
- **EC Packets:** Forward Error Correction packets (recoverable if ≤EC_packets lost)
- **Circular Buffer:** `MAX_RXTX_BLOCKS_BUFFER` old blocks (history for retransmissions)
- **Adaptive:** Bitrate/FEC ratio adjusts based on link feedback

### 2.3 Transmission (Vehicle Router)
```
ruby_rt_vehicle::_main_loop2()
    ↓
1. Read camera frame (video_sources_try_read_camera_frame)
2. If end-of-frame:
    - Send video packets (highest priority)
    - Send audio packets
    - Send telemetry/commands (lower priority)
    ↓
3. Call process_and_send_packets(true)
    ↓
4. Updates adaptive_video state (bitrate adjustment)
```

**Timing Optimization:**
- Router **pulls** video from Majestic, not waits for callbacks.
- Video prioritized over all other traffic (latency-critical).
- Loop timing: Expected 0.2ms per cycle (6ms margin allowed, see `_main_loop2` error check).

### 2.4 Reception (Ground - Radxa 3W)
```
ruby_rt_station::_main_loop_simple()
    ↓
1. _main_loop_try_recevive_data()
    - Radio RX thread reads packets from RTL8812EU2
    - Duplicate detection via `radio_duplicate_detection_init()`
    - FEC reconstruction if packets lost
    ↓
2. VideoProcessorRxList[i]->periodicLoopProcessor()
    - Reassemble video frames from packets
    - Handle out-of-order packets
    - Apply FEC recovery
    ↓
3. rx_video_output_periodic_loop()
    - Send video frames to ruby_player_radxa
    - MPP hardware decoder processes
    - Output to HDMI
    ↓
4. ruby_central (OSD) overlays telemetry/stats
```

---

## 3. Packet Structure (radiopackets2.h)

```c
typedef struct {
    u32 uCRC;                      // Packet validation + Datarate on RX
    u8 packet_flags;               // Component ID routing
    u8 packet_type;                // 1..150: component types
    u32 stream_packet_idx;         // High 4 bits: Stream ID, Low 28: Packet index
    u16 packet_flags_extended;     // Link priority (high/low capacity, ACK flags)
    u16 total_length;              // Header + payload size
    u16 radio_link_packet_index;   // Per-link monotonic counter
    u32 vehicle_id_src;            // Source vehicle ID
    u32 vehicle_id_dest;           // Dest vehicle ID (0 = broadcast)
} t_packet_header;  // Total: 24 bytes
```

**Routing Logic:**
- **vehicle_id_src/dest:** Identifies which vehicle sending/receiving
- **packet_flags:** Sub-component ID (e.g., 0x01 = telemetry, 0x02 = video)
- **stream_packet_idx:** Detects lost packets via monotonic counter
- **radio_link_packet_index:** Per-radio-interface loss detection

---

## 4. Adaptive Video Algorithm

**Files:** `adaptive_video.cpp`, `video_tx_buffers.cpp`

### Feedback Loop (Ground → Air)
```
Ground (ruby_rt_station) detects:
    - Packet loss rate
    - RTL8812EU2 signal quality (RSSI, noise)
    - Jitter/latency
    ↓ [shared_mem_radio_stats]
    ↓
Send COMMAND packet (high priority) to vehicle
    ↓
Vehicle (ruby_rt_vehicle) receives:
    - Adjust bitrate (higher = less data, lower = more redundancy)
    - Adjust FEC ratio (more EC packets if link quality drops)
    - Adjust keyframe interval (faster recovery on frame loss)
    ↓
Majestic reconfigured (H.264 encoding params)
```

---

## 5. CPU Core Affinity (Radxa 3W)

**Optimization:** Pin processes to specific CPU cores to avoid cache thrashing.

```
Majestic (ISP/Encoder)    → Core 0
ruby_rt_vehicle (Router)  → Core 1
radio_rx (RX thread)      → Core 0 (alternates, see _video_source_majestic_move_ruby_to_other_cores)
```

**Radxa 3W has 4 cores** (likely A55 ARM), so:
- Core 0: Majestic
- Core 1: Ruby Router
- Core 2: Radio RX
- Core 3: Idle/System

---

## 6. Radio Link Parameters (RTL8812EU2)

### Modulation & Datarate
**File:** `radiolink.c`

```c
int sRadioDataRate_bps = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS; 
// Positive: classic bitrate (bps)
// Negative: MCS index (e.g., -1 = MCS0)
```

### Supported Rates (2.4GHz, RTL8812EU2)
- **Legacy (CCK):** 1, 2, 5.5, 11 Mbps
- **OFDM (6-54 Mbps):** 6, 9, 12, 18, 24, 36, 48, 54 Mbps
- **HT (MCS 0-15):** 6.5 - 150 Mbps (depending on bandwidth/GI)

**Adaptive Selection:**
- Link quality poor? Drop to HT-MCS7 (~65 Mbps)
- Link quality good? Bump to HT-MCS15 (~150 Mbps)

---

## 7. Telemetry & RC Links

### Telemetry (Vehicle → Ground)
```
Flight Controller (UART) → ruby_tx_telemetry
    ↓ [MAVLink/LTM parser]
    ↓ [Packetize into Ruby packets]
    ↓
ruby_rt_vehicle (Router) → RTL8812EU2
    ↓
Ground receives → ruby_rx_telemetry
    ↓ [Parse MAVLink/LTM]
    ↓
Display in ruby_central OSD
```

### RC (Ground → Vehicle)
```
Joystick (USB) → ruby_tx_rc
    ↓ [Read joystick state]
    ↓
ruby_rt_station (Router) → RTL8812EU2
    ↓
Vehicle receives → ruby_rx_commands
    ↓ [Parse RC data]
    ↓
Flight Controller (UART)
```

---

## 8. Timing Analysis

### Video Frame Latency
```
Capture (Majestic)     : 0.0 ms (continuous stream)
↓ [Buffer in UDP]       : ~5-10 ms (one UDP buffer cycle)
↓ Parse NAL            : ~1 ms
↓ FEC encode           : ~2-5 ms (depends on frame size)
↓ Radio TX             : ~10-20 ms (bitrate dependent)
↓ Radio RX             : ~1 ms
↓ FEC decode           : ~2-5 ms
↓ Video player (MPP)   : ~5-10 ms
↓ Render OSD           : ~1-2 ms
━━━━━━━━━━━━━━━━━━━━━━━━━━━
Total Latency          : ~28-53 ms (at good link quality)
```

### Critical Loop Timing (Vehicle Router)
```
_main_loop2() cycle: ~6-10ms
  1. Read camera    : 1-2 ms
  2. Send video     : 3-5 ms
  3. Send telemetry : 1 ms
  4. Adaptive logic  : <1 ms
```

If cycle > 6ms, alarm triggered (see `_main_loop2` error handling).

---

## 9. Failure Scenarios & Recovery

### Link Loss Detection
**File:** `radio_duplicate_det.c`

- **Gap Detection:** Monotonic `stream_packet_idx` identifies missing blocks
- **Timeout:** If no packet > 500ms, trigger link-down event
- **Recovery:** Request retransmit of last EC-able block

### Video Freezing
```
If Ground RX no video for >1 sec:
    1. Trigger "Link Lost" OSD warning
    2. Request last keyframe retransmit
    3. If still no video: drop to lower bitrate
    4. If still nothing: gray screen + "NO SIGNAL"
```

### Majestic Crash/Restart
**File:** `video_source_majestic.cpp`

```c
_video_source_majestic_check_cores_affinities_balance()
    ↓
If hw_process_exists("majestic") returns 0:
    - Log error
    - Attempt restart (daemon-like)
    - Rebalance CPU cores
    ↓
If restart fails repeatedly:
    - Fall back to CSI camera (if available)
    - Notify central "Camera source unavailable"
```

---

## 10. Buffer Overflows & Congestion

### UDP Buffer Overflow (Majestic → Ruby)
```
s_uInputVideoUDPBuffer[MAX_PACKET_TOTAL_SIZE]
    If Majestic outputs faster than Ruby reads:
        → Frame drops
        → UDP socket buffer overflows
        → Visible freezes/jitter
    Recovery: Increase UDP SO_RCVBUF socket option
```

### Video TX Buffer Saturation
```
VideoTxPacketsBuffer (circular, MAX_RXTX_BLOCKS_BUFFER blocks)
    If router can't send packets fast enough:
        → Block until space available
        → May stall other processes (telemetry, RC)
    Adaptive solution: Drop bitrate → fewer packets → lower congestion
```

---

## 11. Key Optimizations in This Setup

1. **Majestic Direct UDP Read:** No intermediate process, raw H.264 → Router
2. **Core Pinning:** Majestic + Router on separate cores (Radxa 3W has 4)
3. **FEC Adaptive:** Link quality → auto-adjust redundancy ratio
4. **Priority Queuing:** Video > Audio > Telemetry > RC (via `process_and_send_packets`)
5. **Hardware Decode (MPP):** Offload H.264 decoding to GPU, reduce CPU load

---

## 12. Configuration Files

### Model Object (`code/base/models.h`)
- `video_parameters_t`: Resolution, FPS, Bitrate, Keyframe interval
- `camera_profile_parameters_t`: Gain, exposure, white balance (Majestic params)
- `radio_params_t`: Frequency, modulation, FEC ratio
- All synced Vehicle ↔ Ground via `Model` serialization

---

## Summary: Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│ RunCam WifiLink v2 (OpenIPC Air Unit)                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Majestic ISP (H.264 encoder) → UDP:5600             │   │
│  └────────────────┬─────────────────────────────────────┘   │
│                   ↓                                           │
│  ruby_rt_vehicle (Router)                                    │
│  ├─ Reads Majestic UDP stream (NAL units)                   │
│  ├─ Applies FEC (Galois Field GF(2^8))                      │
│  ├─ Queues video + telemetry packets                        │
│  └─ Sends via RTL8812EU2 (2.4GHz)                           │
└─────────────────────────────────────────────────────────────┘
                        ↓ [Radio Link]
                   RTL8812EU2 (2.4GHz)
                        ↓
┌─────────────────────────────────────────────────────────────┐
│ Radxa 3W (Ground Station)                                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ ruby_rt_station (Router)                             │   │
│  │ ├─ Receives video packets via RTL8812EU2             │   │
│  │ ├─ FEC reconstruction                                │   │
│  │ ├─ Duplicate detection                               │   │
│  │ └─ Routes to players/telemetry                       │   │
│  └────────────────┬─────────────────────────────────────┘   │
│                   ├─ ruby_player_radxa (MPP decoder) → HDMI │
│                   ├─ ruby_rx_telemetry (MAVLink parsing)     │
│                   ├─ ruby_tx_rc (Joystick input)             │
│                   └─ ruby_central (OSD overlay)              │
└─────────────────────────────────────────────────────────────┘
```
