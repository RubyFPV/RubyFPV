#pragma once

// A core plugin is built just as a simple linux C library that exports the methods below.
// It has full access to the hardware platform and can access the file system, create processes and whatever it needs in order to fulfil it's desired functionality.

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;


// Flags that define where the plugin instance is running.
// Relay flag should be ignored, it's transparent, Ruby handles the relaying if needed

#define CORE_PLUGIN_RUNTIME_LOCATION_VEHICLE 1
#define CORE_PLUGIN_RUNTIME_LOCATION_CONTROLLER 2
#define CORE_PLUGIN_RUNTIME_LOCATION_RELAY 4

// Flags that define the capabilities requested by a core plugin;
// Same flags denote the ones that are actually alocated by Ruby to the plugin;

#define CORE_PLUGIN_CAPABILITY_VIDEO_STREAM         (((u32)0x01)<<1)   // Request this capability if your plugin can generate or consume (play) a video stream. // Usually used if your core plugin will handle a custom camera type;
#define CORE_PLUGIN_CAPABILITY_RETRANSMISSIONS      (((u32)0x01)<<2)   // Request this capability if your plugin wants automatic retransmissions support for the video stream
#define CORE_PLUGIN_CAPABILITY_ADAPTIVE_VIDEO       (((u32)0x01)<<3)   // Request this capability if your plugin wants automatic adjustmens of the video stream throughput rate
#define CORE_PLUGIN_CAPABILITY_DATA_STREAM          (((u32)0x01)<<4)   // Request this capability if your plugin wants to send custom data between vehicle and controller (both ways or one way)
#define CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART (((u32)0x01)<<5)   // Request this capability if your plugin wants physical hardware access to a serial UART on the device. The end user will choose to allow access or not and which serial UART will allocate.
#define CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_SPI  (((u32)0x01)<<6)   // Request this capability if your plugin wants physical hardware access to a SPI interface on the device. The end user will choose to allow access or not and which SPI interface will allocate.

// Flags that denote the type of data exchanged over air between the two ends of the radio link
#define CORE_PLUGIN_TYPE_VIDEO_SEGMENT 1
#define CORE_PLUGIN_TYPE_DATA_SEGMENT 2

// Flags that denote what type of video stream the plugin is generating and handling and the hardware source for the video stream.
// Relevant only if the plugin requested video capabilities support.
#define CORE_PLUGIN_VIDEO_STREAM_SOURCE_NONE  0
#define CORE_PLUGIN_VIDEO_STREAM_SOURCE_CSI   1
#define CORE_PLUGIN_VIDEO_STREAM_SOURCE_USB   8
#define CORE_PLUGIN_VIDEO_STREAM_SOURCE_IP    9
#define CORE_PLUGIN_VIDEO_STREAM_SOURCE_CUSTOM 20

#ifdef __cplusplus
extern "C" {
#endif

// This is the first method called at runtime, on any runtime location where the plugin is running (vehicle/controller).
// The plugin should just return the desired capabilities.
u32 core_plugin_on_requested_capabilities();

// The plugin should return a user friendly name for the plugin
const char* core_plugin_get_name();

// The plugin should return a unique alphanumeric string GUID (i.e. "342-ASDFSA-232-JASKD")
const char* core_plugin_get_guid();

// The plugin should return it's current version. It's used by Ruby to manage updates of the plugins and versioning.
int core_plugin_get_version();

// This method should be used to actually initialize any data the plugin needs.
// The actual capabilities allocated by Ruby to the plugin are in the uAllocatedCapabilities parameter.
// Plugin should return 0 if the initialization succeeded.
int core_plugin_init(u32 uRuntimeLocation, u32 uAllocatedCapabilities);

// This is the last method called at runtime, before a plugin is unloaded (due to a reboot or plugin uninstall).
void core_plugin_uninit();


// This method is called when data that is transmitted by the other end of the radio link is received by this end of the radio link.
// Each data exchange between the two radio ends, have a unique monotonically increasing segment number.
// This segment number is used in order to be able to manage rx diversity, multiple redundant  radio links and retransmissions (if enabled).
void core_plugin_on_rx_data(u8* pData, int iDataLength, int iDataType, u32 uSegmentIndex);

// This method is called to check if a plugin has data it wants to send over the radio to the other end.
// Each data exchange between the two radio ends, have a unique monotonically increasing segment number.
// Your plugin should increase this segment number each time it wants to transmit a segment of data.
// This segment number is used in order to be able to manage rx diversity, multiple redundant  radio links and retransmissions (if enabled).
// If the plugin returns 0, it means it has nothing new to send to the other end.
// It's recommended to return a new segment index only when a reasonable amount of data has accumulated (that is, do not generate a new segment for each byte you want to send over air)
u32 core_plugin_has_pending_tx_data();

// This 3 methods are called when a data segment needs to be sent over the air to the other end or it was not received by the other end.
// Ruby will call this methods to get the segment from the plugin so that it can be (re)send over air.
// Your plugin should just use the tx function to retransmit the segment in question.
// Each data exchange between the two radio ends, have a unique monotonically increasing segment number.
// If you have not requested retransmissions support, you can ignore this call.
u8* core_plugin_on_get_segment_data(u32 uSegmentIndex);
int core_plugin_on_get_segment_length(u32 uSegmentIndex);
int core_plugin_on_get_segment_type(u32 uSegmentIndex); // Should return data or video segment type

// This method is called if the plugin requested video streams capabilities.
// The plugin should return the type of video streams it generates/handles so that Ruby can disable it's own handling of such streams. (For example, you can't have both Ruby and your plugin accessing and handling the CSI video port on the Pi.)
// This method is mandatory to be implemented if your plugin requested video capabilities.
// Failing to return the right type of video stream the plugin handles (and the video hardware it accesses) can result in undefined behavior in Ruby.
int core_plugin_on_get_video_stream_source_type();

// This method is called to find out if the plugin managed to setup (if a setup is needed) the video stream source (for example, for an IP camera, the plugin need to see if an IP camera is connected).
// If the plugin returns 0, Ruby will mark that the video stream from the plugin is not available.
int core_plugin_on_setup_video_stream_source();

// The plugin should start capturing the video stream with the provided parameters.
// This method is called only for the plugin instance that is running on the vehicle side.
// It's up to the plugin to store the captured video stream and provide it to Ruby, in a segmented way, when requested by Ruby with the call: core_plugin_has_pending_tx_data
void core_plugin_on_start_video_stream_capture(int iWidth, int iHeight, int iFPS);

// The plugin should stop the video capture
// This method is called only for the plugin instance that is running on the vehicle side.
void core_plugin_on_stop_video_stream_capture();

// The plugin should start the video playback on the screen (controller) at the allocated rectangle on the screen.
// This method is called only for the plugin instance that is running on the controller side.
// It's up to the plugin to cache all the received data over the air and create the appropriate stream for the video playback program it chooses to use.
void core_plugin_on_start_video_stream_playback(int iXPos, int iYPos, int iWidth, int iHeight);

// The plugin should stop the video playback
// This method is called only for the plugin instance that is running on the controller side.
void core_plugin_on_stop_video_stream_playback();

// The plugin should generate the capture video stream at this maximum data rate
// This method is called only for the plugin instance that is running on the vehicle side and only if the plugin requested the adaptive video capability.
void core_plugin_on_lower_video_capture_rate(int iMaxkbps);

// The plugin should generate the capture video stream at this maximum data rate
// This method is called only for the plugin instance that is running on the vehicle side and only if the plugin requested the adaptive video capability.
void core_plugin_on_higher_video_capture_rate(int iMaxkbps);

// The plugin was allocated a UART by the end user
// This method is called only for the plugin instance that requested UART access.
void core_plugin_on_allocated_uart(char* szUARTName);

// The plugin should stop using the UART (dealocated by end user)
// This method is called only for the plugin instance that requested UART access.
void core_plugin_on_stop_using_uart();


#ifdef __cplusplus
}
#endif