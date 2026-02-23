# RubyFPV Codebase Analysis

**Repository:** `RubyFPV/RubyFPV`
**Analysis Date:** 2026-02-23

## 1. Overview
RubyFPV is a comprehensive digital FPV system supporting video, telemetry, and RC control over multiple radio links. It follows a **split architecture** (Vehicle vs. Station) and uses a **multi-process design** with shared memory IPC.

The codebase is primarily **C++** (with some C), structured to support multiple hardware platforms:
*   **Raspberry Pi** (Broadcom MMAL/OpenMAX)
*   **Radxa** (Rockchip MPP/DRM/Cairo)
*   **OpenIPC** (SSC338Q/HiSilicon)

## 2. Directory Structure

*   `code/base`: Core abstractions (Config, Hardware, Shared Memory, Encryption).
*   `code/common`: Shared utilities (String handling, Radio stats).
*   `code/radio`: The custom radio protocol implementation (Packets, FEC, RX/TX).
*   `code/r_vehicle`: **Vehicle-side** logic (Video TX, Telemetry TX, Radio negotiation).
*   `code/r_station`: **Station-side** logic (Video RX, Telemetry RX, RC TX).
*   `code/r_central`: **Ground Station UI/OSD** (The visual interface, Menu, Rendering).
*   `code/renderer`: Graphics abstraction (Raw framebuffer, Cairo, SDL2).
*   `code/r_player`: Video playback engine (Radxa MPP).

## 3. Architecture & Data Flow

### 3.1 Multi-Process Design
RubyFPV does not run as a single monolithic application. Instead, it launches several specialized processes that communicate via **Shared Memory** (`code/base/shared_mem.h`).

**Vehicle Processes:**
1.  `ruby_start`: Bootstrapping and process management.
2.  `ruby_rt_vehicle`: **Main Router**. Handles radio negotiation, packet routing, and video transmission loop.
3.  `ruby_tx_telemetry`: Dedicated process for sending telemetry data to the ground.
4.  `ruby_video_proc`: Video capture and processing control.

**Station Processes:**
1.  `ruby_rt_station`: **Main Router**. Receives radio packets, reassembles video streams, handles diversity/relaying.
2.  `ruby_rx_telemetry`: Receives and parses telemetry from the vehicle.
3.  `ruby_tx_rc`: Reads joystick inputs and sends RC packets to the vehicle.
4.  `ruby_central`: **The UI/OSD**. Renders the video overlay, menus, and handles user input.
5.  `ruby_player_radxa` (Radxa only): Decodes and displays the video stream.

### 3.2 Inter-Process Communication (IPC)
*   **Mechanism:** Named Shared Memory (e.g., `/SYSTEM_SHARED_MEM_RUBY_RADIO_STATS`).
*   **Data Shared:**
    *   Radio Link Stats (RSSI, SNR, Packet Loss).
    *   Video Decoding Stats (Keyframes, FPS, Bitrate).
    *   RC Input State (Joystick positions).
    *   Commands (Between UI and Router).

### 3.3 The Radio Protocol (`code/radio`)
*   **Custom Protocol:** Not standard Wi-Fi. It uses raw packet injection (`radiotap`).
*   **FEC (Forward Error Correction):** Implemented in `fec.c` to recover lost video packets.
*   **Diversity:** Supports multiple radio interfaces receiving the same stream (`radio_rx.c`).
*   **Encryption:** Supported via `encr.c`.

### 3.4 Video Pipeline
*   **Capture:**
    *   *Pi:* `raspivid` (via MMAL).
    *   *OpenIPC:* Shared memory/Stream access.
    *   *Radxa:* V4L2/MPP.
*   **Transmission:**
    *   Video is split into packets (`radiopackets2.h`).
    *   Sent via `processor_tx_video.cpp` in `ruby_rt_vehicle`.
*   **Reception:**
    *   Reassembled in `ruby_rt_station` (`video_rx_buffers.cpp`).
    *   Passed to the decoder (Player) or rendered directly.

## 4. Build System
The `Makefile` detects the platform via `RUBY_BUILD_ENV`:
*   **Pi (Default):** Links against `/opt/vc/lib` (Broadcom specific).
*   **Radxa:** Links against `libdrm`, `libcairo`, `librockchip_mpp`, `libSDL2`.
*   **OpenIPC:** Uses a minimal build set for embedded cameras.

## 5. Key Entry Points
*   **Station UI:** `code/r_central/ruby_central.cpp`
*   **Vehicle Main:** `code/r_vehicle/ruby_rt_vehicle.cpp`
*   **Station Router:** `code/r_station/ruby_rt_station.cpp`

## 6. Observations
*   **Complexity:** The codebase is complex due to low-level hardware optimizations and the custom radio protocol.
*   **Portability:** While originally Pi-centric, the abstraction in `renderer/` and `base/hardware` has allowed porting to Radxa and OpenIPC.
*   **Concurrency:** Heavy use of `pthread` and non-blocking I/O (`select`/`poll`) is evident in the router loops.
