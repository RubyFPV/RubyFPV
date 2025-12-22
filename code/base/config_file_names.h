#pragma once
#include "config_hw.h"

#define MAX_FILE_PATH_SIZE 128

#define LOG_FILE_LOGGER "log_logger.log"
#define LOG_FILE_START  "log_start.txt"
#define LOG_FILE_SYSTEM "log_system.txt"
#define LOG_FILE_ERRORS "log_errors.txt"
#define LOG_FILE_ERRORS_SOFT "log_errors_soft.txt"
#define LOG_FILE_COMMANDS "log_commands.txt"
#define LOG_FILE_WATCHDOG "log_watchdog.txt"
#define LOG_FILE_VIDEO "log_video.txt"
#define LOG_FILE_CAPTURE_VEYE "log_capture_veye.txt"
#define LOG_FILE_VEHICLE "log_vehicle_%s.txt"
#define LOG_FILE_LIVE_VEHICLE_LOG "log_live_vehicle.txt"

#define FILE_FORMAT_SCREENSHOT "picture-%s-%d-%d-%d.png"
#define FILE_FORMAT_VIDEO_INFO "video-%s-%d-%d-%d.info"

#define LOG_USE_PROCESS "use_log_process"
#define CONFIG_FILENAME_DEBUG "debug"
#define FILE_INFO_VERSION "version_ruby_base.txt"
#define FILE_INFO_SHORT_LAST_UPDATE "ruby_update.log"
#define FILE_UPDATE_CMD_LIST "ruby_update_op.cmd"
#define FILE_INFO_LAST_UPDATE "ruby_update.log"

#define FILE_CONFIG_CURRENT_VERSION "current_version.inf"
#define FILE_CONFIG_VEHICLE_SETTINGS "vehicle_settings.cfg"
#define FILE_CONFIG_FIRST_BOOT "firstboot.txt"
#define FILE_CONFIG_FIRST_PAIRING_DONE "firstpairingdone"
#define FILE_CONFIG_SYSTEM_TYPE "system_type.txt"
#define FILE_CONFIG_CAMERA_TYPE "camera_type.txt"
#define FILE_CONFIG_BOARD_TYPE "board.txt"
#define FILE_CONFIG_GPIO "gpio.txt"
#define FILE_VEHICLE_SPECTATOR "spect-%d.mdl"
#define FILE_VEHICLE_CONTROLL "ctrl-%d.mdl"
#define FILE_CONFIG_ACTIVE_CONTROLLER_MODEL "controller_active_model.cfg"
#define FILE_CONFIG_CURRENT_VEHICLE_MODEL "current_vehicle.mdl"
#define FILE_CONFIG_CURRENT_VEHICLE_MODEL_BACKUP "current_vehicle.bak"
#define FILE_CONFIG_CURRENT_VEHICLE_COUNT "current_vehicle_count.cfg"
#define FILE_CONFIG_CURRENT_SEARCH_BAND "current_search_band.cfg"
#define FILE_CONFIG_CURRENT_RADIO_HW_CONFIG "current_radios.cfg"
#define FILE_CONFIG_HARDWARE_I2C_DEVICES "i2c_devices_settings.cfg"
#define FILE_CONFIG_ENCRYPTION_PASS "current_pph.cfg"
#define FILE_CONFIG_HW_SERIAL_PORTS "hw_serial.cfg"
#define FILE_CONFIG_MODELS_CONNECT_FREQUENCIES "models_connect_freq.cfg"
#define FILE_CONFIG_LAST_SIK_RADIOS_DETECTED "last_sik_radios_detected.cfg"
#define FILE_CONFIG_BOOT_TIMESTAMP "boot_timestamp.cfg"
#define FILE_CONFIG_BOOT_COUNT "boot_count.cfg"
#define FILE_CONFIG_VEHICLE_REBOOT_CACHE "reboot_cache.tmp"
#define FILE_CONFIG_UI_PREFERENCES "ui_preferences.cfg"
#define FILE_CONFIG_OSD_PLUGINS_SETTINGS "osd_plugins_settings.cfg"
#define FILE_CONFIG_CORE_PLUGINS_SETTINGS "core_plugins_settings.cfg"
#define FILE_CONFIG_CONTROLLER_SETTINGS "controller_settings.cfg"
#define FILE_CONFIG_CONTROLLER_INTERFACES "controller_interfaces.cfg"
#define FILE_CONFIG_CONTROLLER_ID "controller_id.cfg"
#define FILE_CONFIG_CONTROLLER_OSD_WIDGETS "osd_widgets.cfg"
#define FILE_CONFIG_CONTROLLER_FAVORITES_VEHICLES "favorites.cfg"
#define FILE_CONFIG_FAST_BOOT_COUNTER "fast_boot_counter.txt"

#define FILE_TEMP_USB_TETHERING_DEVICE "usb_tethering"
#define FILE_TEMP_VIDEO_MEM_FILE "tmpVideo.h26x"
#define FILE_TEMP_VIDEO_FILE "tmpVideo.h26x"
#define FILE_TEMP_VIDEO_FILE_INFO "tmpVideo.info"
#define FILE_TEMP_VIDEO_FILE_OSD "tmpVideo.osd"
#define FILE_TEMP_VIDEO_FILE_SRT "tmpVideo.srt"
#define FILE_TEMP_VIDEO_FILE_PROCESS_ERROR "tmpErrorVideo.stat"
#define FILE_TEMP_UPDATE_IN_PROGRESS "updateinprogress"
#define FILE_TEMP_UPDATE_IN_PROGRESS_APPLY "updateinprogressapply"
#define FILE_TEMP_UPDATE_CONTROLLER_PROGRESS "tmp_ctrl_update_result.txt"
#define FILE_TEMP_REINIT_RADIO_IN_PROGRESS "radioreinitinprogress"
#define FILE_TEMP_REINIT_RADIO_REQUEST "radioreinitrequest"
#define FILE_TEMP_ARMED "armed"
#define FILE_TEMP_RC_DETECTED "rclink"
#define FILE_TEMP_ALARM_ON "alarmon"
#define FILE_TEMP_I2C_UPDATED "i2c_updated"
#define FILE_TEMP_CONTROLLER_LOCAL_STATS "tmp_local_stats.inf"
#define FILE_TEMP_CONTROLLER_LOAD_LOCAL_STATS "tmp_load_local_stats"
#define FILE_TEMP_CONTROLLER_CENTRAL_CRASHED "tmp_central_crashed"
#define FILE_TEMP_HDMI_CHANGED "tmp_hdmi_changed"
#define FILE_TEMP_CAMERA_NAME "cam_name.txt"
#define FILE_TEMP_CURRENT_VIDEO_PARAMS "current_video_config.txt"
#define FILE_TEMP_SIK_CONFIG_FINISHED "sik_config_complete"
#define FILE_TEMP_AUDIO_RECORDING "audio.wav"
#define FILE_TEMP_RADIOS_CONFIGURED "radio_configured"
#define SUBFOLDER_UPDATES_PI    "bin/pi/"
#define SUBFOLDER_UPDATES_RADXA "bin/radxaz3/"
#define SUBFOLDER_UPDATES_OIPC  "bin/ssc338q/"
#define SUBFOLDER_UPDATES_DRIVERS "bin/drivers/"


#define CONFIG_FILE_FULLPATH_MAJESTIC_LOG "/tmp/maj.log"
#define CONFIG_FILE_FULLPATH_PAUSE_VIDEO_PLAYER "/tmp/pausedvr"

#define FOLDER_RUBY_FIFO_TEMP "/tmp/ruby/"

//-------------------------------------------

#ifdef HW_PLATFORM_RASPBERRY

#define FOLDER_BINARIES "/home/pi/ruby/"
#define FOLDER_DRIVERS "/home/pi/ruby/drivers/"
#define FOLDER_CONFIG "/home/pi/ruby/config/"
#define FOLDER_CONFIG_MODELS "/home/pi/ruby/config/models/"
#define FOLDER_VEHICLE_HISTORY "/home/pi/ruby/config/models/history-%d/"
#define FOLDER_LOGS "/home/pi/ruby/logs/"
#define FOLDER_MEDIA "/home/pi/ruby/media/"
#define FOLDER_MEDIA_VEHICLE_DATA "/home/pi/ruby/media/vehicle-%u/"
#define FOLDER_RESOURCES "/home/pi/ruby/res/"
#define FOLDER_OSD_PLUGINS "/home/pi/ruby/plugins/osd/"
#define FOLDER_CORE_PLUGINS "/home/pi/ruby/plugins/core/"
#define FOLDER_UPDATES "/home/pi/ruby/updates/"
#define FOLDER_RUBY_TEMP "/home/pi/ruby/tmp/"
#define FOLDER_USB_MOUNT "/home/pi/ruby/tmp/tmpusbfiles/"
#define FOLDER_TEMP_VIDEO_MEM "/home/pi/ruby/tmp/memdisk/"
#define FOLDER_WINDOWS_PARTITION "/boot/"
#define FOLDER_CALIBRATION_FILES "/home/pi/ruby/cal/"

#define FILE_FORCE_VEHICLE "/boot/forcevehicle"
#define FILE_FORCE_VEHICLE_NO_CAMERA "/boot/force_no_camera"
#define FILE_FORCE_CONTROLLER "/boot/forcecontroller"
#define FILE_FORCE_ROUTER "/boot/forcerouter"
#define FILE_FORCE_RESET "/boot/forcereset"
#define FILE_BOOT_LOG_STEPS "/boot/bootstepslog.txt"

//#define VIDEO_RECORDER_COMMAND "raspivid"
//#define VIDEO_RECORDER_COMMAND_VEYE "/usr/local/bin/veye_raspivid"
//#define VIDEO_RECORDER_COMMAND_VEYE307 "/usr/local/bin/307/veye_raspivid"
//#define VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME "veye_raspivid"
#define VIDEO_RECORDER_COMMAND "ruby_capture_raspi"
#define VIDEO_RECORDER_COMMAND_VEYE "./ruby_capture_veye"
#define VIDEO_RECORDER_COMMAND_VEYE307 "./ruby_capture_veye"
#define VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME "ruby_capture_veye"

#define VIDEO_PLAYER_PIPE "ruby_player_p"
#define VIDEO_PLAYER_SM "ruby_player_s"
#define VIDEO_PLAYER_OFFLINE "ruby_player_f"

#define VEYE_COMMANDS_FOLDER "/usr/local/share/veye-raspberrypi"
#define VEYE_COMMANDS_FOLDER307 "/usr/local/share/veye-raspberrypi/307"

#endif


// --------------------------------------------------------------------------------

#if defined(HW_PLATFORM_RADXA)

#define FOLDER_BINARIES "/home/radxa/ruby/"
#define FOLDER_DRIVERS "/home/radxa/ruby/drivers/"
#define FOLDER_CONFIG "/home/radxa/ruby/config/"
#define FOLDER_CONFIG_MODELS "/home/radxa/ruby/config/models/"
#define FOLDER_VEHICLE_HISTORY "/home/radxa/ruby/config/models/history-%d/"
#define FOLDER_LOGS "/home/radxa/ruby/logs/"
#define FOLDER_MEDIA "/home/radxa/ruby/media/"
#define FOLDER_MEDIA_VEHICLE_DATA "/home/radxa/ruby/media/vehicle-%u/"
#define FOLDER_RESOURCES "/home/radxa/ruby/res/"
#define FOLDER_OSD_PLUGINS "/home/radxa/ruby/plugins/osd/"
#define FOLDER_CORE_PLUGINS "/home/radxa/ruby/plugins/core/"
#define FOLDER_UPDATES "/home/radxa/ruby/updates/"
#define FOLDER_RUBY_TEMP "/home/radxa/ruby/tmp/"
#define FOLDER_USB_MOUNT "/home/radxa/ruby/tmp/tmpusbfiles/"
#define FOLDER_TEMP_VIDEO_MEM "/home/radxa/ruby/tmp/memdisk/"
#define FOLDER_WINDOWS_PARTITION "/config/"
#define FOLDER_CALIBRATION_FILES "/home/radxa/ruby/cal/"

#define FILE_FORCE_VEHICLE "/config/forcevehicle"
#define FILE_FORCE_VEHICLE_NO_CAMERA "/config/force_no_camera"
#define FILE_FORCE_CONTROLLER "/config/forcecontroller"
#define FILE_FORCE_ROUTER "/config/forcerouter"
#define FILE_FORCE_RESET "/config/forcereset"
#define FILE_BOOT_LOG_STEPS "/config/bootstepslog.txt"

#define VIDEO_PLAYER_PIPE "ruby_player_radxa"
#define VIDEO_PLAYER_SM "ruby_player_radxa"
#define VIDEO_PLAYER_UDP "ruby_player_radxa"
#define VIDEO_PLAYER_OFFLINE "ruby_player_radxa"

#endif

// -------------------------------------------------------------------------------


#ifdef HW_PLATFORM_OPENIPC_CAMERA

#define FOLDER_BINARIES "/usr/sbin/"
#define FOLDER_DRIVERS "/usr/sbin/drivers/"
#define FOLDER_CONFIG "/root/ruby/config/"
#define FOLDER_CONFIG_MODELS "/root/ruby/config/models/"
#define FOLDER_VEHICLE_HISTORY "/root/ruby/config/models/history-%d/"
#define FOLDER_LOGS "/tmp/logs/"
#define FOLDER_MEDIA "/tmp/media/"
#define FOLDER_MEDIA_VEHICLE_DATA "/tmp/media/vehicle-%u/"
#define FOLDER_RESOURCES "/tmp/res/"
#define FOLDER_OSD_PLUGINS "/root/ruby/plugins/osd/"
#define FOLDER_CORE_PLUGINS "/root/ruby/plugins/core/"
#define FOLDER_UPDATES "/tmp/updates/"
#define FOLDER_RUBY_TEMP "/tmp/ruby/tmp/"
#define FOLDER_USB_MOUNT "/tmp/tmpusbfiles/"
#define FOLDER_TEMP_VIDEO_MEM "/tmp/ruby/memdisk/"
#define FOLDER_WINDOWS_PARTITION ""
#define FOLDER_CALIBRATION_FILES "/tmp/"

#define FILE_FORCE_VEHICLE "/root/forcevehicle"
#define FILE_FORCE_VEHICLE_NO_CAMERA "/root/force_no_camera"
#define FILE_FORCE_CONTROLLER "/root/forcecontroller"
#define FILE_FORCE_ROUTER "/root/forcerouter"
#define FILE_FORCE_RESET "/root/forcereset"
#define FILE_BOOT_LOG_STEPS "/root/ruby/bootstepslog.txt"

#define VIDEO_RECORDER_COMMAND "majestic"

#endif

// --------------------------------------------------------------------------



#define FILE_ID_VEHICLE_LOG 0
#define FILE_ID_VEHICLE_LOGS_ARCHIVE 1
#define FILE_ID_CORE_PLUGINS_ARCHIVE 5