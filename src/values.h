#pragma once

// Operation Mode
enum OpMode
{
    OPMODE_NORMAL = 0,      // Normal mode
    OPMODE_BLUETOOTH_SINK,  // Bluetooth sink mode. Player acts as as bluetooth speaker. WiFi is deactivated. Music from SD and webstreams can't be played.
    OPMODE_BLUETOOTH_SOURCE // Bluetooth sourcemode. Player sennds audio to bluetooth speaker/headset. WiFi is deactivated. Music from SD and webstreams can't be played.
};

// Track-Control
enum TrackControl
{
    NO_ACTION = 0, // Dummy to unset track-control-command
    STOP,          // Stop play
    PLAY,          // Start playback
    PAUSE,         // Pause playback
    PAUSEPLAY,     // Pause/play
    NEXTTRACK,     // Next track of playlist
    PREVIOUSTRACK, // Previous track of playlist
    FIRSTTRACK,    // First track of playlist
    LASTTRACK,     // Last track of playlist
};

// Playmodes
enum PlayModes
{
    NO_PLAYLIST = 0,                       // If no playlist is active
    SINGLE_TRACK = 1,                      // Play a single track
    SINGLE_TRACK_LOOP = 2,                 // Play a single track in infinite-loop
    AUDIOBOOK = 3,                         // Single track, can save last play-position
    AUDIOBOOK_LOOP = 4,                    // Single track as infinite-loop, can save last play-position
    ALL_TRACKS_OF_DIR_SORTED = 5,          // Play all files of a directory (alph. sorted)
    ALL_TRACKS_OF_DIR_RANDOM = 6,          // Play all files of a directory (randomized)
    ALL_TRACKS_OF_DIR_SORTED_LOOP = 7,     // Play all files of a directory (alph. sorted) in infinite-loop
    WEBSTREAM = 8,                         // Play webradio-stream
    ALL_TRACKS_OF_DIR_RANDOM_LOOP = 9,     // Play all files of a directory (randomized) in infinite-loop
    PLAYLIST_BUSY = 10,                    // Used if playlist is created
    LOCAL_M3U = 11,                        // Plays items (webstream or files) with addresses/paths from a local m3u-file
    SINGLE_TRACK_OF_DIR_RANDOM = 12,       // Play a single track of a directory and fall asleep subsequently
    RANDOM_SUBDIRECTORY_OF_DIRECTORY = 13, // Picks a random subdirectory from a given directory and do ALL_TRACKS_OF_DIR_SORTED
};

// RFID-modifcation-types
enum Commands
{
    CMD_NOTHING = 0,                        // Do Nothing
    CMD_LOCK_BUTTONS_MOD = 100,             // Locks all buttons and rotary encoder
    CMD_SLEEP_TIMER_MOD_15 = 101,           // Puts uC into deepsleep after 15 minutes + LED-DIMM
    CMD_SLEEP_TIMER_MOD_30 = 102,           // Puts uC into deepsleep after 30 minutes + LED-DIMM
    CMD_SLEEP_TIMER_MOD_60 = 103,           // Puts uC into deepsleep after 60 minutes + LED-DIMM
    CMD_SLEEP_TIMER_MOD_120 = 104,          // Puts uC into deepsleep after 120 minutes + LED-DIMM
    CMD_SLEEP_AFTER_END_OF_TRACK = 105,     // Puts uC into deepsleep after track is finished + LED-DIMM
    CMD_SLEEP_AFTER_END_OF_PLAYLIST = 106,  // Puts uC into deepsleep after playlist is finished + LED-DIMM
    CMD_SLEEP_AFTER_5_TRACKS = 107,         // Puts uC into deepsleep after five tracks + LED-DIMM
    CMD_REPEAT_PLAYLIST = 110,              // Changes active playmode to endless-loop (for a playlist)
    CMD_REPEAT_TRACK = 111,                 // Changes active playmode to endless-loop (for a single track)
    CMD_DIMM_LEDS_NIGHTMODE = 120,          // Changes LED-brightness
    CMD_TOGGLE_WIFI_STATUS = 130,           // Toggles WiFi-status
    CMD_TOGGLE_BLUETOOTH_SINK_MODE = 140,   // Toggles Normal/Bluetooth sink Mode
    CMD_TOGGLE_BLUETOOTH_SOURCE_MODE = 141, // Toggles Normal/Bluetooth source Mode
    CMD_ENABLE_FTP_SERVER = 150,            // Enables FTP-server
    CMD_TELL_IP_ADDRESS = 151,              // Command: ESPuino announces its IP-address via speech
    CMD_PLAYPAUSE = 170,                    // Command: play/pause
    CMD_PREVTRACK = 171,                    // Command: previous track
    CMD_NEXTTRACK = 172,                    // Command: next track
    CMD_FIRSTTRACK = 173,                   // Command: first track
    CMD_LASTTRACK = 174,                    // Command: last track
    CMD_VOLUMEINIT = 175,                   // Command: set volume to initial value
    CMD_VOLUMEUP = 176,                     // Command: increase volume by 1
    CMD_VOLUMEDOWN = 177,                   // Command: lower volume by 1
    CMD_MEASUREBATTERY = 178,               // Command: Measure battery-voltage
    CMD_SLEEPMODE = 179,                    // Command: Go to deepsleep
    CMD_SEEK_FORWARDS = 180,                // Command: jump forwards (time period to jump (in seconds) is configured via settings.h: jumpOffset)
    CMD_SEEK_BACKWARDS = 181,               // Command: jump backwards (time period to jump (in seconds) is configured via settings.h: jumpOffset)
    CMD_STOP = 182,                         // Command: stops playback
};

// Repeat-Modes
enum RepeatMode
{
    NO_REPEAT = 0,    // No repeat
    TRACK,            // Repeat current track (infinite loop)
    PLAYLIST,         // Repeat whole playlist (infinite loop)
    TRACK_N_PLAYLIST, // Repeat both (infinite loop)
};

// Seek-modes
enum AudioSeekMode
{
    SEEK_NORMAL = 0, // Normal play
    SEEK_FORWARDS,   // Seek forwards
    SEEK_BACKWARDS,  // Seek backwards
};

// supported languages
#define DE 1
#define EN 2

// Debug
#define PRINT_TASK_STATS 900 // Prints task stats (only debugging; needs modification of platformio.ini (https://forum.espuino.de/t/rfid-mit-oder-ohne-task/353/21))
