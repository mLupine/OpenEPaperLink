# Seeedstudio XIAO ESP32 Setup Guide

This guide explains how to set up a "spaghetti AP" using Seeedstudio XIAO ESP32S3 and XIAO ESP32C6 boards with optional CC1101 sub-GHz radio and W5500 Ethernet modules.

## Hardware Requirements

### Required Components
- **Seeedstudio XIAO ESP32S3** - Main controller board
- **Seeedstudio XIAO ESP32C6** - 802.15.4 radio controller

### Optional Components
- **CC1101 Module** - Sub-GHz radio for extended range (433/868/915 MHz)
- **W5500 Ethernet Module** - Wired network connectivity with automatic WiFi fallback

## Pin Mapping

### XIAO ESP32C6 (Radio Controller) Pin Connections

#### CC1101 Module Connections
The CC1101 module should be connected to the ESP32C6 using the following pins:

| CC1101 Pin | ESP32C6 GPIO | XIAO Pin | Function | Notes |
|------------|--------------|----------|----------|-------|
| SCK        | GPIO19       | D8       | SPI Clock | Hardware SPI SCK |
| MISO       | GPIO20       | D9       | SPI MISO | Hardware SPI MISO |
| MOSI       | GPIO18       | D10      | SPI MOSI | Hardware SPI MOSI |
| CSN/CS     | GPIO21       | D3       | Chip Select | SPI Chip Select |
| GDO0       | GPIO22       | D4       | Data Ready | Interrupt pin - packet RX/TX |
| GDO2       | GPIO23       | D5       | TX FIFO | Interrupt capable |
| VCC        | 3.3V         | 3V3      | Power | 3.3V power supply |
| GND        | GND          | GND      | Ground | Common ground |

**Important Notes:**
- The pin mapping uses **hardware SPI** pins for optimal performance
- GDO0 and GDO2 are interrupt-capable pins
- Avoids GPIO3 (RF switch) and GPIO14 (antenna config) - these are reserved for WiFi control
- GPIO0, 1 are used for UART1 (S3↔C6 communication), matching ELECROW_C6 config
- GPIO16, 17 remain on UART0 for debug console
- Leaves GPIO2 free for future expansion or analog input

#### Connections to ESP32S3
| Signal | ESP32C6 GPIO | XIAO C6 Pin | Description | Connects To |
|--------|--------------|-------------|-------------|-------------|
| TX     | GPIO0        | D0          | UART1 transmit to S3 | S3 GPIO44 (RX) |
| RX     | GPIO1        | D1          | UART1 receive from S3 | S3 GPIO43 (TX) |
| BOOT   | GPIO9        | GPIO9 pad   | Boot mode control | S3 GPIO4 (PROG) |
| EN     | CHIP_EN      | EN pad/button | Reset control | S3 GPIO5 (RESET) |
| 3.3V   | 3V3          | 3V3         | Power input | S3 3.3V |
| GND    | GND          | GND         | Common ground | S3 GND |

**Critical Notes:**
- **UART Configuration:** C6 uses GPIO0/1 for UART1 (S3 communication). Console remains on GPIO16/17 (UART0).
- This matches the proven ELECROW_C6 configuration
- **GPIO9** may require soldering to a test pad or castellated hole on the XIAO C6
- **EN pin** access: If not exposed as a pad, you have two options:
  1. Solder a wire to the EN pin of the ESP32C6 chip directly (advanced)
  2. Use manual reset mode (see troubleshooting section below)

### XIAO ESP32S3 (Main Controller) Pin Connections

#### UART and Programming Connections to ESP32C6
| Signal | ESP32S3 GPIO | XIAO S3 Pin | Description | Connects To |
|--------|--------------|-------------|-------------|-------------|
| TX     | GPIO43       | D6          | Shared UART transmit | C6 GPIO1 (RX) |
| RX     | GPIO44       | D7          | Shared UART receive | C6 GPIO0 (TX) |
| PROG   | GPIO4        | D4          | C6 boot mode control | C6 GPIO9 |
| RESET  | GPIO5        | D5          | C6 reset control | C6 EN pin |
| 3.3V   | 3V3          | 3V3         | Power supply | C6 3.3V |
| GND    | GND          | GND         | Common ground | C6 GND |

**Note:** S3 configuration remains unchanged - it always uses GPIO43/44 regardless of which C6 pins they connect to.

**Important Notes:**
- The UART is **shared** for both normal communication AND programming the C6
- **PROG pin** must connect to C6's **GPIO9** to enter bootloader mode
- **RESET pin** should connect to C6's **EN (CHIP_EN)** pin for automatic flashing
  - If C6's EN pin is not accessible, you can set `FLASHER_AP_RESET=-1` and manually press the C6's reset button when flashing

## Wiring Diagram

### Complete Connection Map (8 wires total)

```
┌──────────────────────────┐         ┌──────────────────────────┐
│   XIAO ESP32S3           │         │   XIAO ESP32C6           │
│   (Main Controller)      │         │   (Radio Controller)     │
│                          │         │                          │
│  GPIO43 (D6/TX) ─────────┼────────►│  GPIO1 (D1/RX) UART1    │  1. UART Data
│  GPIO44 (D7/RX) ◄────────┼─────────│  GPIO0 (D0/TX) UART1    │  2. UART Data
│  GPIO4  (D4/PROG) ───────┼────────►│  GPIO9 (BOOT)   *       │  3. Boot Control
│  GPIO5  (D5/RESET) ──────┼────────►│  EN (CHIP_EN)   **      │  4. Reset Control
│  3.3V ────────────────────┼─────────│  3.3V                   │  5. Power
│  GND ─────────────────────┼─────────│  GND                    │  6. Ground
│                          │         │                          │
│                          │         │  GPIO16/17: Console     │  UART0 debug via USB
└──────────────────────────┘         └──────────────────────────┘
                                                  │
                                                  │ CC1101 Module
                                                  │ SPI Connection
                                                  ▼
                                      ┌──────────────────────┐
                                      │   CC1101 Sub-GHz     │
                                      │   Radio Module       │
                                      │                      │
                                      │  SCK  ← GPIO19 (D8)  │  7. SPI Clock
                                      │  MISO → GPIO20 (D9)  │  8. SPI Data In
                                      │  MOSI ← GPIO18 (D10) │  9. SPI Data Out
                                      │  CS   ← GPIO21 (D3)  │  10. Chip Select
                                      │  GDO0 → GPIO22 (D4)  │  11. Interrupt 0
                                      │  GDO2 → GPIO23 (D5)  │  12. Interrupt 2
                                      │  VCC  ← 3.3V         │  13. Power
                                      │  GND  ← GND          │  14. Ground
                                      └──────────────────────┘

Legend:
  * GPIO9 may require soldering to a test pad or castellated hole
 ** EN pin may require soldering to the reset button pad or chip pin

Total Connections: 6 wires between S3↔C6, plus CC1101 module wiring
```

### Connection Summary

**Between ESP32S3 and ESP32C6 (6 critical wires):**
1. TX: S3 GPIO43 → C6 GPIO1 (UART1 RX, D1 pin)
2. RX: S3 GPIO44 ← C6 GPIO0 (UART1 TX, D0 pin)
3. PROG: S3 GPIO4 → C6 GPIO9
4. RESET: S3 GPIO5 → C6 EN pin
5. Power: S3 3.3V → C6 3.3V
6. Ground: S3 GND → C6 GND

**Note:** C6 debug console remains on UART0 (GPIO16/17), accessible via USB. S3 side uses GPIO43/44 unchanged.

**Between ESP32C6 and CC1101 (8 wires - OPTIONAL):**
7. SPI SCK: C6 GPIO19 → CC1101 SCK
8. SPI MISO: C6 GPIO20 ← CC1101 MISO
9. SPI MOSI: C6 GPIO18 → CC1101 MOSI
10. SPI CS: C6 GPIO21 → CC1101 CSN
11. Interrupt: C6 GPIO22 ← CC1101 GDO0
12. TX FIFO: C6 GPIO23 ← CC1101 GDO2
13. Power: C6 3.3V → CC1101 VCC
14. Ground: C6 GND → CC1101 GND

**Between ESP32S3 and W5500 Ethernet (7 wires - OPTIONAL):**
See dedicated W5500 section below for complete wiring details.

## W5500 Ethernet Module Setup (Optional)

The W5500 Ethernet module provides wired network connectivity with automatic WiFi fallback. This is ideal for reliable, high-performance network connections.

### W5500 Module Features
- **100Mbps** Ethernet connection via RJ45
- **Automatic failover**: Seamlessly switches between Ethernet and WiFi
- **Lower latency**: Wired connection for faster tag updates
- **Configurable modes**: Auto (Ethernet priority), Ethernet-only, or WiFi-only
- **Power over Ethernet (PoE)**: Some W5500 modules support PoE

### W5500 Pin Connections to ESP32S3

The W5500 module connects to the ESP32S3 via SPI. These pins are defined in the `ESP32_S3_XIAO_SEEEDSTUDIO_C6_W5500_AP` environment:

| W5500 Pin | ESP32S3 GPIO | XIAO S3 Pin | Function | Notes |
|-----------|--------------|-------------|----------|-------|
| SCK       | GPIO7        | D2          | SPI Clock | Hardware SPI |
| MISO      | GPIO9        | D8          | SPI MISO | Hardware SPI |
| MOSI      | GPIO8        | D3          | SPI MOSI | Hardware SPI |
| CS        | GPIO10       | -           | Chip Select | May require soldering to pad |
| INT       | GPIO3        | D1          | Interrupt | Packet ready signal |
| RST       | GPIO2        | D0          | Reset | Hardware reset |
| VCC       | 3.3V or 5V   | 3V3/5V      | Power | Check your module specs |
| GND       | GND          | GND         | Ground | Common ground |

**Important Notes:**
- GPIO10 (CS) may require soldering to a castellated pad on the XIAO S3
- Some W5500 modules require 5V power, others work with 3.3V - check your module
- The W5500 uses different SPI pins than the C6's CC1101 module
- INT and RST pins are optional but recommended for better performance

### Network Mode Configuration

The firmware supports three network modes (configurable via web UI):

1. **Auto Mode (Default - Recommended)**
   - Ethernet has priority when connected
   - Automatically falls back to WiFi if Ethernet cable is disconnected
   - Automatically switches back to Ethernet when cable is reconnected
   - WiFi is disabled when Ethernet is active (reduces interference)

2. **Ethernet Only Mode**
   - Only uses Ethernet, WiFi is completely disabled
   - No automatic failover
   - Use for environments where WiFi is not available or desired

3. **WiFi Only Mode**
   - Only uses WiFi, Ethernet initialization is skipped
   - Use when you have W5500 hardware but want to use WiFi

**To configure network mode:**
1. Open the AP's web interface
2. Go to Settings tab
3. Find "Network mode" dropdown
4. Select desired mode: Auto / Ethernet only / WiFi only
5. Click Save
6. Mode persists across reboots

### W5500 Wiring Example

```
┌──────────────────────────┐         ┌──────────────────────────┐
│   XIAO ESP32S3           │         │   W5500 Ethernet Module  │
│   (Main Controller)      │         │                          │
│                          │         │                          │
│  GPIO7  (D2/SCK) ────────┼────────►│  SCK                     │
│  GPIO9  (D8/MISO) ◄──────┼─────────│  MISO                    │
│  GPIO8  (D3/MOSI) ───────┼────────►│  MOSI                    │
│  GPIO10 (CS pad) ────────┼────────►│  CS                      │
│  GPIO3  (D1/INT) ◄───────┼─────────│  INT                     │
│  GPIO2  (D0/RST) ────────┼────────►│  RST                     │
│  3.3V or 5V ─────────────┼────────►│  VCC                     │
│  GND ────────────────────┼────────►│  GND                     │
│                          │         │                          │
│                          │         │  [RJ45 Port] ← Ethernet  │
└──────────────────────────┘         └──────────────────────────┘
```

### Building Firmware with W5500 Support

Use the dedicated W5500 build environment:

```bash
cd ESP32_AP-Flasher

# Build with W5500 support
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_W5500_AP

# Flash to board
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_W5500_AP -t upload

# Upload web interface
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_W5500_AP -t uploadfs
```

### Testing W5500 Connection

After flashing and connecting the W5500:

1. **Connect Ethernet cable** to the W5500 module
2. **Monitor serial output**:
   ```bash
   pio device monitor -b 115200
   ```
3. **Look for Ethernet messages**:
   ```
   [ETH] Initializing W5500 SPI Ethernet (mode=0)
   [ETH] W5500 initialized
   [WiFi-event X] ETH Started
   [WiFi-event X] ETH Connected (WiFi disabled)
   [WiFi-event X] ETH MAC: XX:XX:XX:XX:XX:XX, IPv4: 192.168.1.100, FULL_DUPLEX, 100Mbps
   [UDP] Initializing UDP discovery on ETHERNET
   ```

4. **Test failover** (if in Auto mode):
   - Unplug Ethernet cable
   - Watch AP automatically switch to WiFi:
     ```
     [ETH] Ethernet cable disconnected
     [ETH] Ethernet disconnected, switching to WiFi mode
     Connected!
     [UDP] Initializing UDP discovery on WIFI
     ```
   - Plug cable back in
   - Watch AP switch back to Ethernet

### W5500 Troubleshooting

**W5500 Not Detected:**
- Verify SPI connections (SCK, MISO, MOSI, CS)
- Check power supply voltage (3.3V or 5V depending on module)
- Ensure CS (GPIO10) is properly connected (may need soldering)
- Try measuring continuity with multimeter
- Check if W5500 module has built-in pull-up resistors on SPI lines

**Ethernet Cable Not Detected:**
- Verify RJ45 cable is CAT5e or better
- Try a different Ethernet cable
- Check link LEDs on W5500 module (should light when cable connected)
- Verify router/switch port is active
- Try connecting to a different router/switch port

**No IP Address on Ethernet:**
- Ensure DHCP server is running on your network
- Check serial logs for DHCP timeout messages
- Try configuring static IP via web UI (WiFi → Settings)
- Verify network cable pinout (T568A or T568B standard)

**Cannot Switch from Ethernet to WiFi:**
- Check "Network mode" setting in web UI (should be "Auto")
- Ensure WiFi credentials are saved in AP configuration
- Monitor serial output when unplugging Ethernet cable
- Try manually setting mode to "WiFi only" to test WiFi connection

## Building the Firmware

### For ESP32C6 (Radio Controller)

1. Navigate to the C6 firmware directory:
   ```bash
   cd ARM_Tag_FW/OpenEPaperLink_esp32_C6_AP
   ```

2. Configure the project using the Seeedstudio XIAO profile:
   ```bash
   idf.py menuconfig
   ```
   - Navigate to "OEPL config" → "Hardware profile"
   - Select "SEEEDSTUDIO-XIAO"
   - Ensure "Enable SubGhz Support" is checked
   - Save and exit

3. Or use the pre-configured sdkconfig:
   ```bash
   cp sdkconfig.seeedstudio_xiao sdkconfig
   ```

4. Build and flash:
   ```bash
   idf.py build
   idf.py -p /dev/ttyACM0 flash monitor
   ```

### For ESP32S3 (Main Controller)

Choose the appropriate build environment based on your hardware:

#### Standard Build (WiFi Only)

Use this if you don't have a W5500 Ethernet module:

```bash
cd ESP32_AP-Flasher

# Build
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP

# Flash
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t upload

# Upload web interface
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t uploadfs
```

#### W5500 Ethernet Build

Use this if you have a W5500 Ethernet module connected:

```bash
cd ESP32_AP-Flasher

# Build with W5500 support
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_W5500_AP

# Flash
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_W5500_AP -t upload

# Upload web interface
pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_W5500_AP -t uploadfs
```

**Note:** The W5500 build includes automatic Ethernet/WiFi failover. If you have W5500 hardware but want WiFi-only operation, flash the W5500 build and set "Network mode" to "WiFi only" in the web UI.

## Pin Configuration Details

### Why These Pins?

**ESP32C6 Pin Selection Rationale:**
- **GPIO19, 20, 18**: These are the hardware SPI pins, providing better performance and reliability
- **GPIO21, 22, 23**: Free GPIO pins that don't conflict with:
  - USB pins (GPIO12, GPIO13)
  - Boot/strapping pins (GPIO8, GPIO9)
  - RF control (GPIO3, GPIO14)
  - Internal flash (GPIO4-7)
- **GPIO0, 1**: UART1 pins for S3↔C6 communication (matches ELECROW_C6 proven config)
- **GPIO16, 17**: UART0 console (default, accessible via USB)
- **GPIO2**: Kept free for ADC or other purposes

**ESP32S3 Pin Selection Rationale:**
- **GPIO43, 44**: Hardware UART pins (U0TXD, U0RXD)
- These are the only reliable UART pins available on the XIAO ESP32S3 form factor

### Pins to Avoid

**On ESP32C6:**
- GPIO3: RF switch power control - DO NOT USE
- GPIO4-7: Flash/JTAG - DO NOT USE
- GPIO8, 9: Strapping pins (GPIO9 is used for BOOT control only)
- GPIO12, 13: USB hardware pins - DO NOT USE
- GPIO14: Antenna selection - DO NOT USE
- GPIO15: Onboard LED
- GPIO16, 17: UART0 console - reserved for debug output

**On ESP32S3:**
- GPIO19, 20: USB - DO NOT USE
- Most other GPIO pins are not available on the XIAO form factor

## Testing

After flashing both boards:

1. Connect them according to the wiring diagram above
2. Power on the ESP32S3 (it will provide power to the C6 via 3.3V connection)
3. The S3 will attempt to communicate with the C6 and flash the C6 firmware if needed
4. Monitor the serial output on the S3:
   ```bash
   pio device monitor -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP
   ```
5. Look for successful initialization messages for:
   - ESP32C6 communication
   - CC1101 module initialization
   - Sub-GHz radio functionality

## Alternative: Manual Reset Mode

If you **cannot access the C6's EN pin** (no exposed pad), you can use manual reset mode:

1. Edit `platformio.ini` and change:
   ```
   -D FLASHER_AP_RESET=-1
   ```

2. When flashing the C6 firmware:
   - The S3 will still control GPIO9 (PROG pin) automatically
   - You'll need to **manually press the C6's reset button** when prompted
   - Look for "waiting for download mode" message, then press reset

3. This mode requires user intervention but avoids difficult soldering to the EN pin.

## Troubleshooting

### C6 Won't Enter Programming Mode
**Symptoms:** Flashing fails with "Failed to connect" or timeout errors

**Solutions:**
1. **Check PROG connection:** Verify S3 GPIO4 is connected to C6 GPIO9
   - GPIO9 may be on a test pad or castellated hole on the XIAO C6
   - Use a multimeter to verify continuity
2. **Check RESET connection:** Verify S3 GPIO5 is connected to C6 EN pin
   - If EN pin is not accessible, use manual reset mode (see above)
3. **Verify power:** Ensure C6 is receiving 3.3V power from S3
4. **Try manual mode:**
   - Hold down C6's boot button
   - Press and release C6's reset button
   - Release boot button
   - Try flashing again

### CC1101 Not Detected
**Symptoms:** SubGhz radio not initializing, "CC1101 not found" errors

**Solutions:**
- Verify all SPI connections (SCK, MISO, MOSI, CS)
- Check 3.3V power supply to CC1101 (measure with multimeter)
- Ensure GND is properly connected
- Verify CC1101 module is not damaged (try with multimeter continuity test)
- Check that GPIO pins match the configuration (GPIO18-23)

### UART Communication Failure
**Symptoms:** S3 can't communicate with C6, timeout errors, no response

**Solutions:**
- **Critical:** Verify TX/RX are cross-connected to UART1 pins: S3 GPIO43 → C6 GPIO1 (D1), S3 GPIO44 ← C6 GPIO0 (D0)
- **Common mistake:** Do NOT connect to C6 GPIO16/17 (D6/D7) - these are UART0 console, not UART1!
- Check baud rate settings match on both sides (default 115200)
- Ensure common ground between S3 and C6
- Verify C6 firmware is running (connect via USB to see debug output on UART0)
- Try reflashing C6 firmware with correct sdkconfig
- **S3 side requires NO changes** - it always uses GPIO43/44

### Build Errors
**For C6:**
- Ensure ESP-IDF version 5.0 or later is installed
- Clean build: `idf.py fullclean && idf.py build`
- Verify sdkconfig.seeedstudio_xiao was copied correctly

**For S3:**
- Ensure PlatformIO and ESP32 platform are up to date
- Clean build: `pio run -t clean -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP`
- Check that partition table exists: `esp32_squeeze_ota.csv`

## Comparison with Default Configuration

### Pin Mapping Differences

| Component | Default/Yellow AP | XIAO Config | Reason for Change |
|-----------|-------------------|-------------|-------------------|
| **ESP32C6 - CC1101 Module** |
| CC1101 SCK | GPIO0 | GPIO19 | Hardware SPI pin, GPIO0 kept free for ADC |
| CC1101 MISO | GPIO7 | GPIO20 | GPIO7 conflicts with flash on XIAO |
| CC1101 MOSI | GPIO1 | GPIO18 | Hardware SPI alignment |
| CC1101 CSN | GPIO4 | GPIO21 | GPIO4-7 reserved for flash/JTAG |
| CC1101 GDO0 | GPIO5 | GPIO22 | GPIO5 reserved for flash |
| CC1101 GDO2 | GPIO6 | GPIO23 | GPIO6 reserved for flash |
| **ESP32C6 - S3 Communication** |
| UART TX to S3 | GPIO3 | GPIO0 | GPIO3 controls RF switch! Uses UART1, console stays on UART0 |
| UART RX from S3 | GPIO2 | GPIO1 | UART1 pins - matches ELECROW_C6 config |
| BOOT (GPIO9) | GPIO9 | GPIO9 | Same (required for programming) |
| **ESP32S3 - C6 Communication** |
| UART TX to C6 | GPIO17 | GPIO43 | GPIO17 not available on XIAO S3 |
| UART RX from C6 | GPIO18 | GPIO44 | GPIO18 not available on XIAO S3 |
| Debug/Prog TX | GPIO19 | Shared (43) | Shared UART mode saves GPIO |
| Debug/Prog RX | GPIO20 | Shared (44) | Shared UART mode saves GPIO |
| PROG (C6 GPIO9) | GPIO21 | GPIO4 | GPIO21 not available on XIAO S3 |
| RESET (C6 EN) | GPIO47 | GPIO5 | GPIO47 not available on XIAO S3 |
| LED Control | GPIO16 | Not used | GPIO16 not available on XIAO S3 |

### Key Architectural Differences

1. **Shared UART Mode**: XIAO config uses `FLASHER_DEBUG_SHARED` to use one UART for both communication and programming
2. **Hardware SPI**: XIAO C6 uses actual hardware SPI pins (GPIO18-20) for better CC1101 performance
3. **Avoids Reserved Pins**: Carefully avoids GPIO3 (RF control), GPIO14 (antenna), and GPIO4-7 (flash/JTAG)
4. **Compact Design**: Uses minimal GPIO pins to work within XIAO form factor constraints

## Additional Resources

- [Seeedstudio XIAO ESP32S3 Wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [Seeedstudio XIAO ESP32C6 Wiki](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/)
- [CC1101 Datasheet](https://www.ti.com/product/CC1101)
- [OpenEPaperLink Wiki](https://github.com/jjwbruijn/OpenEPaperLink/wiki)

## License

This configuration follows the main OpenEPaperLink project license:
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
