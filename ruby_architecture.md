# RubyFPV Architecture & Development Guide

**Based on:** RubyFPV Development Documentation
**Date:** 2026-02-23

## 1. Development Guidelines

### Coding Philosophy
*   **Language:** C/C++ mix.
*   **Performance:** C is preferred in critical paths to avoid vtable lookups.
*   **Simplicity:** Avoid complex OOP/polymorphism unless strictly necessary.
*   **Robustness:**
    *   **No Exceptions:** `try/catch` is avoided.
    *   **Error Handling:** Functions must check input parameters.
    *   **Degraded State:** Missing files/errors should trigger recovery or degraded mode, not a crash.
    *   **Self-Healing:** If a config file is missing, recreate it with defaults.
*   **Naming Convention:** Hungarian notation (e.g., `iValue`, `pPointer`, `szString`) for readability.

## 2. Process Architecture
RubyFPV distinguishes between **Controller** (Ground Station) and **Vehicle** (Air Unit).

### 2.1 Boot Process (`ruby_start`)
*   **Role:** The single entry point on boot.
*   **Logic:** Checks hardware/config to decide if the device is a Controller or a Vehicle.
*   **Action:** Launches the appropriate set of processes (`ruby_controller` or `ruby_vehicle`) and then exits.

### 2.2 Core Routers (`ruby_rt_station` / `ruby_rt_vehicle`)
These are the most critical components.
*   **Role:** Message Routing.
*   **Scope:** Handles BOTH local IPC (between processes) and remote Radio links.
*   **Unified Message Format:**
    *   All messages (Local or Over-the-Air) share the same structure and header.
    *   Defined in `radiopackets2.h`.
    *   This eliminates translation overhead and bugs.

### 2.3 Message Routing Logic
The Common Header (`radiopackets2.h`) contains fields for routing:
*   **Source/Destination:** Identifies the component sending/receiving.
*   **Component IDs:** Each process/module has a unique ID.
*   **Packet Types:** Distinguishes between Video, Audio, Telemetry, RC, Commands, etc.
*   **Priority:** Packets can be marked high priority (e.g. video retransmission requests).

## 3. Data Flow Example (Serial/UART)
RubyFPV acts as a **Transparent Bidirectional Serial Link**:
1.  **Controller Input:** Ingests serial data from a UART port.
2.  **Packetization:** Wraps data into a standard Ruby packet.
3.  **Routing:** Sends packet to the Router -> Radio -> Vehicle Router.
4.  **Reconstruction:** Vehicle Router extracts data -> Output to matching UART port on the vehicle.

## 4. Command & Configuration Flow
*   **Model Object:** The single source of truth for vehicle settings.
*   **Persistence:** Changes are saved to local storage immediately.
*   **Propagation:**
    *   User changes setting in UI.
    *   UI updates local `Model` and persists it.
    *   UI sends a Command Packet (via IPC) to the Router.
    *   Router sends it over air.
    *   Vehicle Router receives it, updates its own `Model`, and applies the change (e.g., reconfigures camera).

## 5. Packet Structure (`radiopackets2.h`)

```c
typedef struct {
    u32 uCRC;                   // CRC for packet/header. MSB 0x00 identifies Ruby radio packets.
                                // Updated with datarate on reception.
    u8 packet_flags;            // Routing flags (Component ID, etc.)
    u8 packet_type;             // 1..150: Component packets
                                // 150..200: Controller control packets
                                // 200..250: Vehicle control packets
    u32 stream_packet_idx;      // High 4 bits: Stream ID (0..15)
                                // Low 28 bits: Monotonically increasing index for packet loss detection.
    u16 packet_flags_extended;  // Bit 0: High capacity links only
                                // Bit 1: Low capacity links only
                                // Bit 2: Requires ACK
    u16 total_length;           // Total length including header and CRC
    u16 radio_link_packet_index;// Monotonic index per radio link (for link quality stats)
    u32 vehicle_id_src;         // Source Vehicle ID
    u32 vehicle_id_dest;        // Destination Vehicle ID (0 for broadcast)
} __attribute__((packed)) t_packet_header;
```

## 6. Directory Structure Mapping
*   **base/**: Hardware abstraction, Config, Shared Memory API. New hardware support starts here.
*   **common/**: Generic code shared across processes.
*   **radio/**: Low-level IEEE radio implementation (`radiotap`, packet injection).
*   **renderer/**: Graphics abstraction (Pi/Radxa/OpenIPC renderers).
*   **r_start**: Boot process (`ruby_start`).
*   **r_station**: Controller-specific code (`ruby_rt_station`).
*   **r_vehicle**: Vehicle-side code (`ruby_rt_vehicle`).
*   **r_central**: UI/OSD process (`ruby_central`).
*   **r_utils**: Standalone utilities (loggers, updaters, video processing).
*   **r_i2c**: I2C device communication process.
