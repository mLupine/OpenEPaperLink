# OpenEPaperLink - AI Assistant Reference Guide

> **Last Updated**: 2025-11-13
> **Purpose**: Comprehensive reference for AI assistants working with OpenEPaperLink codebase

## Table of Contents
- [Project Overview](#project-overview)
- [Architecture](#architecture)
- [Directory Structure](#directory-structure)
- [Key Concepts](#key-concepts)
- [Build System](#build-system)
- [Important Files](#important-files)
- [Configuration](#configuration)
- [Protocol & Communication](#protocol--communication)
- [Common Operations](#common-operations)
- [Development Workflow](#development-workflow)
- [Known Issues & Gotchas](#known-issues--gotchas)

---

## Project Overview

**OpenEPaperLink** is a system for repurposing electronic shelf label (ESL) tags as general-purpose e-paper displays controlled via WiFi.

### Core Components
1. **Access Point (AP)**: ESP32-based hub that manages tags
2. **Tags**: Battery-powered e-paper devices (various manufacturers)
3. **Web Interface**: Browser-based management UI
4. **Radio Stack**: 2.4GHz (802.15.4) and/or Sub-GHz (433/868/915 MHz)

### Supported Hardware
- **ESP32-S3**: Main AP controller
- **ESP32-C6**: 802.15.4 radio co-processor (2.4GHz tags)
- **ESP32-H2**: Alternative 802.15.4 radio
- **CC1101**: Optional sub-GHz radio module (433/868/915 MHz)

---

## Architecture

### Dual-Radio System (ESP32-S3 + ESP32-C6)

```
┌─────────────────────────────────────────────────┐
│  ESP32-S3 (Main Controller)                     │
│  - WiFi management                              │
│  - Web server (AsyncWebServer)                  │
│  - Content management                           │
│  - Tag database                                 │
│  - SPIFFS filesystem                            │
│  - Optional: CC1101 sub-GHz radio (SPI)         │
└──────────┬──────────────────────────────────────┘
           │ UART (GPIO 4/5 + PROG/RESET)
           ▼
┌─────────────────────────────────────────────────┐
│  ESP32-C6 (Radio Co-processor)                  │
│  - 802.15.4 MAC/PHY (2.4GHz)                    │
│  - Tag protocol implementation                  │
│  - Optional: CC1101 sub-GHz radio (SPI)         │
└─────────────────────────────────────────────────┘
           │ Wireless (802.15.4)
           ▼
     [E-Paper Tags]
```

### Communication Flow

1. **User → Web Interface**: HTTP/WebSocket to ESP32-S3
2. **S3 → C6**: UART commands (binary protocol)
3. **C6 → Tags**: 802.15.4 or Sub-GHz radio packets
4. **Tags → C6**: Responses, checkins, data requests
5. **C6 → S3**: Tag data, status updates via UART
6. **S3 → User**: WebSocket updates, tag status

### Key Subsystems

- **Content Manager** (`contentmanager.cpp`): Orchestrates tag updates
- **Tag Database** (`tag_db.cpp`): Stores tag info, tracks state
- **Protocol Handler** (`newproto.cpp`): Tag communication protocol
- **Web Server** (`web.cpp`): HTTP endpoints, WebSocket
- **Flasher** (`flasher.cpp`): Tag firmware flashing
- **C6/H2 Firmware** (`ARM_Tag_FW/OpenEPaperLink_esp32_C6_AP/`): Radio stack

---

## Directory Structure

```
OpenEPaperLink/
├── ESP32_AP-Flasher/           # Main ESP32-S3 AP firmware
│   ├── src/                    # C++ source files
│   │   ├── main.cpp           # Entry point, setup/loop
│   │   ├── contentmanager.cpp # Tag content orchestration
│   │   ├── tag_db.cpp         # Tag database management
│   │   ├── web.cpp            # HTTP endpoints
│   │   ├── newproto.cpp       # Tag protocol implementation
│   │   ├── flasher.cpp        # Tag firmware flashing
│   │   ├── wifimanager.cpp    # WiFi configuration
│   │   └── ...
│   ├── include/               # Header files
│   │   ├── tag_db.h           # Config struct, tag record
│   │   └── ...
│   ├── wwwroot/               # Web interface files
│   │   ├── index.html         # Main UI
│   │   ├── main.js            # Tag list, WebSocket handling
│   │   ├── ota.js             # Firmware update UI
│   │   └── ...
│   ├── data/                  # SPIFFS filesystem data
│   ├── platformio.ini         # Build configuration
│   ├── xiao_8MB_ota.csv       # Partition table (XIAO S3)
│   └── ...
│
├── ARM_Tag_FW/                 # Tag & Radio firmware
│   ├── OpenEPaperLink_esp32_C6_AP/  # ESP32-C6 radio firmware
│   │   ├── main/
│   │   │   ├── main.c         # C6 main loop
│   │   │   ├── second_uart.c  # UART to S3
│   │   │   ├── radio.c        # 802.15.4 radio
│   │   │   ├── proto.h        # Protocol structures
│   │   │   └── ...
│   │   └── sdkconfig.*        # ESP-IDF configurations
│   │
│   ├── OpenEPaperLink_TLSR/   # TLSR tag firmware (older)
│   └── ...
│
├── binaries/                   # Pre-built firmware binaries
│   ├── ESP32-AP/              # S3 AP firmware
│   ├── ESP32-C6/              # C6 radio firmware
│   └── Tag/                   # Tag firmware
│
├── oepl-proto.h               # Shared protocol definitions
└── ...
```

---

## Key Concepts

### 1. Tag Lifecycle

```
[Unassociated] → [Scanning] → [Associated] → [Active]
                                    ↓
                              [Check-in] ← [Sleeping]
                                    ↓
                              [Data Request] → [Receiving Data]
                                    ↓
                              [Display Update] → [Sleep]
```

### 2. Sleep Management

Tags sleep to conserve battery. The AP controls sleep duration:

- **config.maxsleep**: Maximum sleep time in **seconds** (0-65535)
- **Default**: 40 seconds
- **Range**: 5s (fast updates) to 3600s (1 hour)
- **Protocol**: `nextCheckIn` field in `AvailDataInfo` struct

**Important**: As of recent refactor, ALL sleep times are in **seconds** (not minutes or encoded values).

### 3. Content Modes

Tags support various content modes (field: `tagRecord.contentMode`):

- `0`: No content / uninitialized
- `1`: Fixed image
- `5`: Calendar display
- `10`: Slideshow
- `12`: HA (Home Assistant) integration
- `17`: NFC tag data
- `18`: Custom display
- `19`: JSON-based template
- `21`: AP status display
- etc.

### 4. Image Processing Pipeline

```
User uploads → Stored in SPIFFS → Processed to tag format →
Compressed (optional) → Sent to tag → Tag decompresses → Display
```

Supported formats:
- Raw binary (`.raw`)
- Compressed (LZ4, custom)
- 1bpp, 2bpp, 4bpp (B/W, B/W/R, B/W/Y)

---

## Build System

### PlatformIO

All ESP32 builds use PlatformIO. Each hardware variant has its own environment.

**Example environments** (`platformio.ini`):
```ini
[env:ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP]    # XIAO S3 with C6
[env:ESP32_AP]                            # Standard ESP32
[env:ESP32_S3_ZeroPlus_C6_AP]            # M5Stack variant
```

### Build Commands

```bash
# From ESP32_AP-Flasher directory
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP    # Build
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t upload  # Flash
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t uploadfs  # Flash filesystem
```

### ESP32-C6 Build (ESP-IDF)

```bash
cd ARM_Tag_FW/OpenEPaperLink_esp32_C6_AP
cp sdkconfig.seeedstudio_xiao sdkconfig  # Use config
idf.py build
idf.py flash
```

### Key Build Flags

**Performance** (`platformio.ini`):
```ini
board_build.f_cpu = 240000000L         # CPU: 240MHz
board_build.f_flash = 80000000L        # Flash: 80MHz
build_flags = -O2                      # Optimize (LTO removed - causes linker errors)
board_build.psram_speed = 80           # PSRAM: 80MHz
```

**WiFi**:
```ini
-D WIFI_NO_SLEEP                        # Disable WiFi power save
-D DEFAULT_WIFI_TX_POWER=WIFI_POWER_19dBm  # Max TX power
```

**AsyncTCP**:
```ini
-D CONFIG_ASYNC_TCP_MAX_ACK_TIME=5000
-D CONFIG_ASYNC_TCP_PRIORITY=10
-D CONFIG_ASYNC_TCP_RUNNING_CORE=1
```

---

## Important Files

### ESP32-S3 AP Firmware

#### `src/main.cpp`
- Entry point: `setup()` and `loop()`
- Initializes: WiFi, SPIFFS, WebServer, Tag DB, UART to C6
- Main loop: pumps tag database, handles queues

#### `src/contentmanager.cpp`
- **Purpose**: Orchestrates tag content updates
- **Key Functions**:
  - `processContentQueue()`: Main loop, iterates tag DB
  - `drawNew()`: Initiates tag update
  - `drawImage()`, `drawCalendar()`, `drawCustom()`: Content-specific rendering
  - Sleep management logic (lines 74-103)

**Sleep Logic** (simplified):
```cpp
if (tag needs update) {
    secondsUntilNextUpdate = nextupdate - now;
    if (secondsUntilNextUpdate > config.maxsleep) {
        secondsUntilNextUpdate = config.maxsleep;  // Cap to config
    }
    prepareIdleReq(mac, secondsUntilNextUpdate);  // Tell tag to sleep
}
```

#### `src/tag_db.cpp`
- **Purpose**: Tag database (vector of `tagRecord`)
- **Key Structures**:
  - `Config`: Global AP settings (WiFi, maxsleep, repo, etc.)
  - `tagRecord`: Per-tag state (MAC, battery, content, pending data)
- **Functions**:
  - `loadAPConfig()` / `saveAPConfig()`: Persist settings in JSON
  - `getTagCount()`: Count tags, timeouts, low battery
  - `tagRecord::findByMAC()`: Lookup tag by MAC address

#### `include/tag_db.h`
**Critical struct** - AP configuration:
```cpp
struct Config {
    uint8_t channel;           // 802.15.4 channel (11-26)
    uint8_t subghzchannel;     // Sub-GHz channel
    char alias[32];            // AP name
    uint16_t maxsleep;         // Max sleep in SECONDS (0-65535)
    String repo;               // Update repo (mLupine/OpenEPaperLink)
    String env;                // Build environment
    // ... many more fields
};
```

**Tag record**:
```cpp
struct tagRecord {
    uint8_t mac[8];            // Tag MAC address
    uint16_t RSSI;             // Signal strength
    uint8_t LQI;               // Link quality
    uint16_t batteryMv;        // Battery voltage (mV)
    uint8_t contentMode;       // Content type (0-21+)
    uint32_t nextupdate;       // Next update timestamp
    uint32_t expectedNextCheckin;  // When tag should wake
    uint16_t pendingIdle;      // Pending sleep duration (seconds)
    // ... many more fields
};
```

#### `src/newproto.cpp`
- **Purpose**: Tag protocol implementation
- **Key Functions**:
  - `prepareIdleReq()`: Send "go to sleep" command (line 76)
  - `prepareDataAvail()`: Notify tag of pending data
  - `sendDataAvail()`: Actually transmit packet to C6

**Protocol packet** (via UART to C6):
```cpp
void prepareIdleReq(const uint8_t* dst, uint16_t nextCheckin) {
    pending.availdatainfo.dataType = DATATYPE_NOUPDATE;
    pending.availdatainfo.nextCheckIn = nextCheckin;  // SECONDS
    pending.attemptsLeft = 10 + (nextCheckin / 60);  // More retries for longer sleeps
    sendDataAvail(&pending);  // → UART → C6 → Radio → Tag
}
```

#### `src/web.cpp`
- **Purpose**: HTTP endpoints
- **Key Endpoints**:
  - `GET /`: Main UI (`index.html`)
  - `GET /json_tag_db`: Tag list (JSON)
  - `POST /save_apcfg`: Save AP config
  - `POST /imgupload`: Upload image for tag
  - WebSocket `/ws`: Real-time updates

#### `wwwroot/index.html`
- **Purpose**: Main web UI
- **Structure**:
  - Tab-based interface (taglist, settings, update, etc.)
  - Number input for `maxsleep` (line 363):
    ```html
    <input type="number" id="apclatency" min="5" max="65535" step="1" placeholder="40">
    ```
  - Human-readable time display shows interpretation (e.g., "300 seconds (5 minutes)")
  - Input validation: accepts 5-65535 seconds, shows red border if invalid

#### `wwwroot/main.js`
- **Purpose**: Tag list UI, WebSocket handling
- **Key Functions**:
  - `updateTaglist()`: Renders tag table
  - `syncDB()`: Fetches tag database via WebSocket
  - `formatSleepTime(seconds)`: Converts seconds to human-readable format
  - `updateSleepTimeDisplay()`: Updates display next to sleep input field
  - Timeout detection (line 541): `maxsleep + 300 seconds`

#### `wwwroot/ota.js`
- **Purpose**: Firmware update UI
- **Repo**: Uses `apConfig.repo` or defaults to `mLupine/OpenEPaperLink`
- **Flow**: Fetch GitHub releases → Download binaries → Flash

---

### ESP32-C6 Radio Firmware

#### `ARM_Tag_FW/OpenEPaperLink_esp32_C6_AP/main/main.c`
- **Purpose**: C6 main loop
- **Key Functions**:
  - `app_main()`: Initialize radio, UART, tasks
  - UART RX task: Receives commands from S3
  - Radio task: Sends/receives 802.15.4 packets

#### `ARM_Tag_FW/OpenEPaperLink_esp32_C6_AP/main/second_uart.h`
- **Purpose**: UART pin configuration for S3 ↔ C6
- **Example** (XIAO config):
  ```c
  #define CONFIG_OEPL_HARDWARE_UART_TX 0   // GPIO0 = UART1 TX
  #define CONFIG_OEPL_HARDWARE_UART_RX 1   // GPIO1 = UART1 RX
  ```
- **Critical**: Must use UART1, NOT UART0 (console)

#### `ARM_Tag_FW/OpenEPaperLink_esp32_C6_AP/main/proto.h`
- **Purpose**: Protocol structures shared between S3 and C6
- **Key Struct**:
  ```c
  struct AvailDataInfo {
      uint8_t checksum;
      uint64_t dataVer;
      uint32_t dataSize;
      uint8_t dataType;
      uint8_t dataTypeArgument;
      uint16_t nextCheckIn;  // SECONDS (0-65535)
  } __packed;
  ```

#### `ARM_Tag_FW/OpenEPaperLink_esp32_C6_AP/sdkconfig.*`
- **Purpose**: ESP-IDF build configurations
- **Examples**:
  - `sdkconfig.seeedstudio_xiao`: XIAO C6 pin config
  - `sdkconfig.m5stampC6`: M5Stack C6
- **Key Settings**:
  ```
  CONFIG_OEPL_HARDWARE_PROFILE_SEEEDSTUDIO_XIAO=y
  CONFIG_OEPL_SUBGIG_SUPPORT=y
  CONFIG_MISO_GPIO=20
  CONFIG_SCK_GPIO=19
  CONFIG_MOSI_GPIO=18
  CONFIG_CSN_GPIO=21
  CONFIG_GDO0_GPIO=22
  CONFIG_GDO2_GPIO=23
  ```

---

## Configuration

### AP Configuration (`apconfig.json`)

Stored in SPIFFS, loaded by `loadAPConfig()`:

```json
{
  "channel": 15,              // 802.15.4 channel (11-26)
  "subghzchannel": 0,         // Sub-GHz channel
  "alias": "OEPL-AP",         // AP name
  "maxsleep": 40,             // Max sleep (SECONDS)
  "stopsleep": 1,             // Shorten latency when web UI open
  "repo": "mLupine/OpenEPaperLink",  // Update repository
  "env": "ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP",  // Build env
  "timezone": "CET-1CEST,M3.5.0,M10.5.0/3",
  "sleeptime1": 0,            // Sleep schedule start hour
  "sleeptime2": 0,            // Sleep schedule end hour
  "wifipower": 34,            // WiFi TX power (17 dBm)
  "language": 0,              // UI language
  // ... more settings
}
```

**Access in code**:
```cpp
extern Config config;  // Global, defined in tag_db.cpp
uint16_t sleepTime = config.maxsleep;  // Always in seconds
String updateRepo = config.repo;
```

### Partition Tables

**XIAO S3** (8MB flash): `xiao_8MB_ota.csv`
```csv
nvs,      data, nvs,     0x9000,   0x4000,    # Settings
otadata,  data, ota,     0xD000,   0x2000,    # OTA metadata
phy_init, data, phy,     0xF000,   0x1000,    # WiFi calibration
app0,     app,  ota_0,   0x10000,  0x200000,  # Firmware slot 1 (2MB)
app1,     app,  ota_1,   0x210000, 0x200000,  # Firmware slot 2 (2MB)
spiffs,   data, spiffs,  0x410000, 0x3D0000,  # Filesystem (3.8MB)
coredump, data, coredump,0x7E0000, 0x10000,   # Crash dumps
```

---

## Protocol & Communication

### 802.15.4 Packet Structure

Tags use IEEE 802.15.4 for radio communication. The protocol is defined in `oepl-proto.h` and implemented in C6 firmware.

### UART Protocol (S3 ↔ C6)

Binary packets, structure varies by command. Examples:

**Send Data Available**:
```
[CMD][MAC:8][AvailDataInfo struct][checksum]
```

**Tag Checkin**:
```
[CMD][MAC:8][TagInfo struct][checksum]
```

### Sleep Control

**Flow**:
1. S3 calculates sleep duration (in seconds)
2. S3 sends `prepareIdleReq()` → C6 via UART
3. C6 broadcasts "Data Available" packet with `nextCheckIn` field
4. Tag receives packet, sleeps for `nextCheckIn` seconds
5. Tag wakes, sends checkin packet
6. Repeat

**Key Point**: `nextCheckIn` is **always in seconds** (uint16_t, max 65535 = 18.2 hours)

---

## Common Operations

### 1. Change Maximum Sleep Time

**Via Web UI**:
- Settings tab → "Maximum sleep (seconds)" input field
- Enter any value from 5 to 65535 seconds (5s to 18.2 hours)
- Human-readable display shows interpretation (e.g., "5 minutes 30 seconds")
- Click "Save"

**Via Code**:
```cpp
config.maxsleep = 300;  // 5 minutes
saveAPConfig();
```

### 2. Add New Tag

Tags self-associate when they scan for APs. No manual intervention needed.

**Flow**:
```
Tag → Sends Association Request
C6  → Receives, forwards to S3
S3  → Creates tagRecord in tagDB
S3  → Responds with Association Info
```

### 3. Update Tag Content

**Via Web UI**:
- Click tag in list
- Upload image or configure content mode
- Tag updates on next checkin

**Via Code**:
```cpp
tagRecord* tag = tagRecord::findByMAC(mac);
tag->contentMode = 1;  // Fixed image
tag->nextupdate = 0;   // Force immediate update
drawNew(mac, tag);     // Trigger update
```

### 4. Flash New Firmware

**AP Firmware** (ESP32-S3):
```bash
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t upload
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t uploadfs
```

**C6 Firmware**:
```bash
cd ARM_Tag_FW/OpenEPaperLink_esp32_C6_AP
idf.py build flash
```

**Tag Firmware**:
- Via Web UI: Update tab → select tag → flash firmware
- Via code: Use `flasher.cpp` functions

---

## Development Workflow

### 1. Setting Up Build Environment

**Prerequisites**:
- PlatformIO (for ESP32-S3)
- ESP-IDF 5.x (for ESP32-C6)
- Python 3.x

**Install PlatformIO**:
```bash
pip install platformio
```

**Install ESP-IDF**:
```bash
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c6
. ./export.sh
```

### 2. Making Changes

**Typical workflow**:
1. Edit source files (`src/*.cpp`, `include/*.h`)
2. Build: `pio run -e <environment>`
3. Flash: `pio run -e <environment> -t upload`
4. Monitor: `pio device monitor` (115200 baud)
5. Debug: Check serial output, use `Serial.printf()`

**Web UI changes**:
1. Edit `wwwroot/*.html`, `wwwroot/*.js`
2. Flash filesystem: `pio run -e <environment> -t uploadfs`
3. Reload browser (Ctrl+Shift+R for hard refresh)

### 3. Testing Changes

**Test checklist**:
- [ ] AP boots, WiFi connects
- [ ] Web UI loads, displays tag list
- [ ] Tags can associate
- [ ] Tags can receive updates
- [ ] Sleep intervals work correctly
- [ ] OTA updates work
- [ ] Configuration saves/loads

### 4. Debugging

**Serial output**:
```cpp
Serial.printf("Tag %02X%02X sleeping %d seconds\n", mac[7], mac[6], seconds);
```

**WebSocket debugging**:
```javascript
console.log("Tag update:", data);
```

**C6 debugging**:
```c
ESP_LOGI(TAG, "UART RX: %d bytes", event.size);
```

---

## Known Issues & Gotchas

### 1. UART Pin Conflicts

❌ **Don't use UART0 for S3↔C6 communication**
- UART0 (GPIO16/17) is the USB console
- Using UART0 causes debug output to corrupt data

✅ **Use UART1 (GPIO0/1 or other pins)**
```c
#define CONFIG_OEPL_HARDWARE_UART_TX 0  // UART1
#define CONFIG_OEPL_HARDWARE_UART_RX 1
```

### 2. Sleep Time Units

❌ **Old code used minutes OR negative-value encoding**
- Very confusing: `maxsleep=10` meant 10 minutes OR `-6` meant 5 seconds

✅ **New code uses seconds everywhere**
```cpp
config.maxsleep = 40;  // 40 SECONDS (not minutes!)
```

**Migration note**: If upgrading from old firmware, config may have wrong values. Default is 40 seconds.

### 3. Partition Size

❌ **Firmware too large for partition**
- Standard partition: 1.5MB
- XIAO S3 firmware: ~1.95MB

✅ **Use custom partition table** (`xiao_8MB_ota.csv`)
```csv
app0, app, ota_0, 0x10000, 0x200000,  # 2MB (enough!)
```

### 4. PSRAM Speed

❌ **Default PSRAM: 40MHz** (slow)

✅ **Configure PSRAM to 80MHz**
```ini
board_build.psram_speed = 80
-D CONFIG_SPIRAM_SPEED_80M=1
```

### 5. WiFi Stability

❌ **WiFi power save can cause issues**

✅ **Disable WiFi power save for AP**
```cpp
WiFi.setSleep(WIFI_PS_NONE);  // No power save
```

Or via build flag:
```ini
-D WIFI_NO_SLEEP
```

### 6. Web UI Caching

❌ **Browser caches old JS/HTML aggressively**

✅ **Hard refresh after filesystem update**
- Chrome/Firefox: Ctrl+Shift+R
- Safari: Cmd+Shift+R

### 7. SDCC Version (Tag Firmware)

❌ **SDCC 4.3.0 or 4.4.0 may produce broken code**

✅ **Use SDCC 4.2.0 exactly**
```bash
sdcc -v
# SDCC : mcs51 4.2.0 #13081
```

### 8. Git Submodules

❌ **Missing shared definitions**
```
fatal: '../shared/oepl-proto.h' not found
```

✅ **Initialize submodules**
```bash
git submodule update --init --recursive
```

### 9. Update Repository

❌ **Default repo: OpenEPaperLink/OpenEPaperLink** (upstream)

✅ **For mLupine's fork**
```cpp
config.repo = "mLupine/OpenEPaperLink";
```

Or via web UI: Update tab → Repository field

### 10. Tag Association

❌ **Tags won't associate if channel mismatch**

✅ **Ensure AP and tag use same channel**
- Default: Channel 15 (802.15.4)
- Change via web UI: Settings tab → Channel dropdown

---

## Recent Major Changes

### 2025-11-13: Seconds-Based Sleep Refactor

**Changed**:
- `config.maxsleep`: `int8_t` → `uint16_t` (now 0-65535 seconds)
- Protocol: `nextCheckIn` always in seconds (removed bit-flag encoding)
- Web UI: Dropdown values changed from encoded/minutes to clean seconds
- Removed all negative value hacks (`-6=5s`, `-5=10s`, etc.)

**Impact**:
- ⚠️ **BREAKING CHANGE**: Old tag firmware won't understand new sleep values
- ⚠️ Config migration: Old `maxsleep=10` (10 minutes) → New default `40` (40 seconds)
- ✅ Much cleaner code, easier to understand
- ✅ Supports up to 18.2 hours (65535 seconds) vs old 127 minutes max

**Migration**:
1. Flash new AP firmware
2. Flash new tag firmware (Chroma 29, TLSR, etc.)
3. Reset AP config OR manually set `maxsleep` via web UI

---

## Useful Commands Reference

### Build & Flash
```bash
# ESP32-S3 AP
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t upload
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t uploadfs

# ESP32-C6 Radio
cd ARM_Tag_FW/OpenEPaperLink_esp32_C6_AP
idf.py build
idf.py flash
idf.py monitor

# Clean
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t clean
```

### Serial Monitor
```bash
pio device monitor -b 115200
# Or
screen /dev/ttyUSB0 115200
```

### Erase Flash
```bash
esptool.py --port /dev/ttyUSB0 erase_flash
```

### OTA Update
```bash
curl -F "update=@.pio/build/ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP/firmware.bin" http://oepl-ap.local/update
```

---

## Additional Resources

- **Main Wiki**: https://github.com/OpenEPaperLink/OpenEPaperLink/wiki
- **Discord**: Community support and development
- **Issues**: https://github.com/OpenEPaperLink/OpenEPaperLink/issues
- **mLupine's Fork**: https://github.com/mLupine/OpenEPaperLink

---

**End of OpenEPaperLink AI Reference**
