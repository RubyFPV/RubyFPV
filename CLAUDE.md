# CLAUDE.md - RubyFPV Project Guide

## Project Overview

RubyFPV (v11.8) is an open-source digital FPV (First Person View) radio system for UAVs, drones, planes, RC cars, and other remote vehicles. It provides live video, audio, telemetry, remote control, and custom data streams over redundant radio links.

**Website:** https://rubyfpv.com/
**Author:** Petru Soroaga (petrusoroaga@yahoo.com)

## Architecture

Modular, process-based architecture where independent components communicate via standardized packet-based IPC. The router processes (`ruby_rt_*`) are the backbone — they route packets between local components and over air, and exclusively manage radio configuration (frequencies, modulation). Video processing logic is partially integrated into the router to reduce latency by avoiding extra IPC hops.

```
Vehicle Side                    Ground Station Side
─────────────                   ───────────────────
ruby_rt_vehicle  ◄──radio──►   ruby_rt_station       (routers - packet routing + radio mgmt)
ruby_tx_telemetry               ruby_rx_telemetry     (transparent serial bridge)
ruby_i2c                        ruby_tx_rc            (RC input → packets)
ruby_video_proc                 ruby_controller
                                ruby_central (UI)     (OSD, menus, Model object per vehicle)
                                ruby_player_radxa     (Radxa only)

Shared: ruby_start, ruby_utils (logger, dhcp, sik_config, alive, update, dbg)
```

**Data flow:** Vehicle captures video + telemetry → transmits via raw WiFi monitor mode → ground station receives and displays in UI. RC commands flow in reverse. The system operates as a decentralized network — nodes maintain neighbor info and route packets toward destinations.

**Key design principles:**
- All inter-process and air-to-ground communication uses standardized packet format (`radiopackets2.h`) with common headers, vehicle_id_src/dest routing, packet_flags, and CRC validation
- Router handles duplicate elimination when identical packets arrive via multiple radio interfaces
- High-priority packets (e.g. video retransmission requests) get prioritized processing
- Missing files should be recreated automatically; degraded functionality preferred over failure
- Functions must validate input parameters; graceful error recovery over exceptions

## Radio Streams

Data is organized into independent **streams**, split into stream packets (similar to RTP). Multiple streams of the same type can coexist independently.

**Stream types:** Video (multiple cameras/relayed vehicles), Data (telemetry + plugin data), Control (RC)

**Priority hierarchy:**
1. Radio link quality measurement (highest)
2. Retransmission requests for missing packets
3. All other streams (equal priority)

**Router responsibilities:** Routing based on radio link capabilities and consumers, duplicate elimination, packet concatenation to reduce overhead, broadcast routing (dest ID 0 for video spectators).

## Build System

**Dev tools install:** `sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential`

```bash
# Raspberry Pi (default)
make all

# Radxa platform
make all RUBY_BUILD_ENV=radxa
# or: ./make_radxa.sh

# OpenIPC platform
make all RUBY_BUILD_ENV=openipc

# Individual targets
make vehicle          # ruby_start, ruby_utils, ruby_tx_telemetry, ruby_rt_vehicle
make station          # ruby_controller, ruby_rt_station, ruby_tx_rc, ruby_rx_telemetry
make ruby_central     # UI application
make ruby_i2c         # I2C daemon
make ruby_utils       # Utility programs
make tests            # Test programs
make clean
```

**Compiler:** C/C++ with `-O2 -Wall`, platform-specific flags via `RUBY_BUILD_HW_PLATFORM_*` defines.

**Key dependencies:** libpcap, libpthread, librt, plus platform-specific: WiringPi (Pi), libgpiod/SDL2/Cairo/DRM (Radxa).

## Code Structure

```
code/
├── base/           # (100 files) Core infrastructure: config, GPIO, hardware abstraction,
│                   #   shared memory, settings, I2C, radio utilities
├── radio/          # (23 files) Radio layer: FEC, radiotap/802.11, packet formats,
│                   #   TX/RX engines, duplicate detection, packet queues
├── common/         # (14 files) Shared: radio stats, string utils, relay utils, frequencies
├── renderer/       # (18 files) Graphics: Cairo (Radxa), OpenVG (Pi), DRM, framebuffer,
│                   #   PNG/JPEG decoding
├── r_vehicle/      # (67 files) Vehicle runtime: video capture/TX, telemetry (MAVLink/LTM/MSP),
│                   #   RC reception, adaptive video, radio link management
├── r_station/      # (49 files) Ground station runtime: video RX/recording, telemetry RX,
│                   #   RC transmission, adaptive video, SiK radio support
├── r_central/      # (343 files) UI: menus, OSD, popups, notifications, input handling,
│                   #   pairing, video playback — LARGEST module
├── r_start/        # (9 files) System startup, first boot, radio initialization
├── r_utils/        # (13 files) Utilities: logger, DHCP, SiK config, watchdog, updater, debugger
├── r_i2c/          # (2 files) I2C device daemon
├── r_player/       # (3 files) Video player (Radxa MPP integration)
├── r_plugins_osd/  # (8 files) OSD gauge plugins: AHI, speed, altitude, heading
├── r_plugins_core/ # (1 file) Core plugin framework
├── r_tests/        # (41 files) Tests: GPIO, sockets, UDP, radio, link, joystick, Cairo
├── public/         # (10 files) Public SDK: plugin APIs, telemetry/settings info, render API
├── test_plugin/    # (11 files) Example plugins
└── utils/          # (4 files) Additional utilities
```

**Total:** ~716 source files, ~167K lines of C/C++.

## Key Source Files

| File | Purpose |
|------|---------|
| `code/base/config.h` | Global constants, version defines (v11.8, build 11801) |
| `code/base/config_hw.h` | Hardware platform detection and defines |
| `code/base/config_radio.h` | Radio configuration: frequencies, power, MCS rates |
| `code/base/config_file_names.h` | All config file path constants |
| `code/base/hardware.cpp` | Board detection, system capabilities |
| `code/base/hardware_radio.c` | WiFi driver loading, adapter detection, monitor mode setup |
| `code/base/hardware_radio_txpower.c` | TX power control per chipset |
| `code/base/gpio.h` | GPIO pin mappings (Pi and Radxa) |
| `code/radio/radiopackets2.h` | Packet format definitions and constants |
| `code/radio/radiolink.c` | Radio link open/close, raw socket TX/RX |
| `code/radio/radio_tx.h` | Radiotap header construction, frame injection |
| `code/radio/fec.c` | Forward Error Correction |
| `code/r_start/ruby_start.cpp` | Main entry point and process supervisor |
| `code/r_start/first_boot.cpp` | First-boot platform setup |
| `code/r_start/r_initradio.cpp` | Radio interface initialization |
| `code/r_vehicle/ruby_rt_vehicle.cpp` | Vehicle main loop |
| `code/r_station/ruby_rt_station.cpp` | Station main loop |
| `code/r_central/ruby_central.cpp` | UI main entry point |

## Target Platforms

| Platform | Boards | Rendering | GPIO |
|----------|--------|-----------|------|
| **Raspberry Pi** | Zero, Zero W, Zero 2, 2B, 3A+, 3B, 3B+, 4B | OpenVG/framebuffer | WiringPi |
| **Radxa** | Zero3, 3C, RunCam VRx | DRM + Cairo + SDL2 | libgpiod |
| **OpenIPC** | Goke 200/210/300, SigmaStar 338Q | Minimal | N/A |

## Radio System

**Mode:** All WiFi adapters run in **monitor mode** (raw packet injection/capture via radiotap headers). No managed/station/AP mode is used for the FPV link.

**Supported WiFi chipsets:**
- Realtek: RTL8812AU, RTL8812EU, RTL8733BU (2.4 + 5.8 GHz)
- Atheros: ath9k_htc (default 18 Mbps)
- Ralink: RT2800USB (2.4 GHz only)
- Serial radios: SiK, ELRS

**Frequency bands:** 433/868/915 MHz, 2.3/2.4/2.5/5.8 GHz

**Transmission:** Raw 802.11 frames with radiotap headers. Supports legacy rates (6-54 Mbps) and MCS 0-9. FEC for video. TX via raw sockets or pcap_inject.

**Reception:** pcap_create + promiscuous mode + pcap_dispatch.

## Networking & SSH

- **SSH credentials:** Pi: `pi/raspberry`, Radxa: `radxa/radxa`, OpenIPC: `root/12345`
- **SSH toggle:** UI menu in `code/r_central/menu/menu_controller_network.cpp`
- **Ethernet:** `eth0` with DHCP (via `pump` on Pi) or fixed IP
- **DHCP control:** `/boot/nodhcp` file disables DHCP
- **Regulatory domain:** Set to `00` (unrestricted) via `iw reg set`

## Protocols

- **MAVLink** - Autopilot communication (full message definitions in `mavlink/`)
- **LTM** - Lightweight Telemetry
- **MSP** - MultiWii Serial Protocol (Betaflight/iNav)
- **Custom Ruby protocol** - Packet-based with radiotap/802.11 framing

## Configuration Files (runtime, on device)

Stored in `FOLDER_CONFIG` (e.g., `/home/pi/ruby/config/`):
- `system_type.txt` - Vehicle vs controller mode + board type
- `current_radios.cfg` - Detected radio hardware
- `vehicle_settings.cfg` / `controller_settings.cfg` - Device settings
- `ui_preferences.cfg` - UI preferences
- `osd_widgets.cfg` - OSD layout
- `camera_type.txt` - Detected camera

## Plugin System

Three plugin categories:

1. **OSD Plugins** — Custom gauges, instruments, telemetry displays on the OSD
2. **Core Plugins** — Custom radio streams and functionalities (USB cameras, AI/vision, gimbal control, vehicle-to-controller data)
3. **Hardware Plugins** — Custom peripherals via microcontrollers (input devices, RC control, camera control, flight management)

Public SDK in `code/public/`:
- `ruby_core_plugin.h` - Core plugin API (custom cameras, data streams, hardware)
- `plugin_osd_functions.h` - OSD plugin API (gauges, indicators)
- Plugins can run on vehicle, controller, or relay
- Built-in OSD plugins: AHI, speed, altitude, heading gauges
- **Deployment:** Compile on Pi, deploy via USB stick to Ruby controller

## Conventions

- C for low-level (base, radio, GPIO, drivers); C++ for high-level (UI, processors, managers)
- **Hungarian notation** for variable names (project convention)
- Shared memory IPC between processes (`shared_mem.h`)
- Platform-specific code via `#ifdef RUBY_BUILD_HW_PLATFORM_*`
- Config constants in `code/base/config*.h` files
- File naming: `r_*` directories = runtime components, `ruby_*` = executable names
- Radio packet max size: 1500 bytes, payload max: 1250 bytes
- **Model object:** C++ object persisting vehicle-specific settings (video, FPS, telemetry ports, OSD). Vehicles have one Model; controllers have one per paired vehicle.
- Radio links are independent, each on a different frequency. Hardware is auto-detected at boot — plug and play.

## Resources

- **Website:** https://rubyfpv.com/
- **Development:** https://rubyfpv.com/development.php
- **Architecture:** https://rubyfpv.com/development_arch.php
- **Dev Guide:** https://rubyfpv.com/development_guide.php
- **GitHub (upstream):** https://github.com/RubyFPV/RubyFPV
- **Forum:** https://www.rcgroups.com/forums/showthread.php?3880253
