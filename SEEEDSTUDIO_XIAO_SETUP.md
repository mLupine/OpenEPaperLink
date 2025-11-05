# Seeedstudio XIAO ESP32 Setup Guide

This guide explains how to set up a "spaghetti AP" using Seeedstudio XIAO ESP32S3 and XIAO ESP32C6 boards with a CC1101 sub-GHz radio module.

## Hardware Requirements

- **Seeedstudio XIAO ESP32S3** - Main controller board
- **Seeedstudio XIAO ESP32C6** - 802.15.4 radio controller
- **CC1101 Module** - Sub-GHz radio for extended range

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
- Leaves GPIO0, 1, 2 free for future expansion or analog inputs

#### UART Connection to ESP32S3
| Signal | ESP32C6 GPIO | XIAO C6 Pin | Description |
|--------|--------------|-------------|-------------|
| TX     | GPIO16       | D6          | Transmit to S3 RX |
| RX     | GPIO17       | D7          | Receive from S3 TX |

### XIAO ESP32S3 (Main Controller) Pin Connections

#### UART Connection to ESP32C6
| Signal | ESP32S3 GPIO | XIAO S3 Pin | Description |
|--------|--------------|-------------|-------------|
| TX     | GPIO43       | D6          | Transmit to C6 RX |
| RX     | GPIO44       | D7          | Receive from C6 TX |

**Note:** GPIO43 and GPIO44 are the hardware UART pins on the XIAO ESP32S3.

## Wiring Diagram

```
┌─────────────────────┐         ┌─────────────────────┐
│  XIAO ESP32S3       │         │  XIAO ESP32C6       │
│                     │         │                     │
│  GPIO43 (D6/TX) ────┼────────►│  GPIO17 (D7/RX)     │
│  GPIO44 (D7/RX) ◄───┼─────────│  GPIO16 (D6/TX)     │
│  GND ───────────────┼─────────│  GND                │
│  3.3V ──────────────┼─────────│  3.3V               │
└─────────────────────┘         └─────────────────────┘
                                         │
                                         │ SPI Connection
                                         ▼
                                ┌─────────────────┐
                                │   CC1101 Module │
                                │                 │
                                │  SCK  ← GPIO19  │
                                │  MISO → GPIO20  │
                                │  MOSI ← GPIO18  │
                                │  CS   ← GPIO21  │
                                │  GDO0 → GPIO22  │
                                │  GDO2 → GPIO23  │
                                │  VCC  ← 3.3V    │
                                │  GND  ← GND     │
                                └─────────────────┘
```

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

1. Navigate to the S3 firmware directory:
   ```bash
   cd ESP32_AP-Flasher
   ```

2. Build using PlatformIO:
   ```bash
   pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP
   ```

3. Flash to the board:
   ```bash
   pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t upload
   ```

4. Upload filesystem (web interface):
   ```bash
   pio run -e ESP32_S3_XIAO_SEEEDSTUDIO_C6_AP -t uploadfs
   ```

## Pin Configuration Details

### Why These Pins?

**ESP32C6 Pin Selection Rationale:**
- **GPIO19, 20, 18**: These are the hardware SPI pins, providing better performance and reliability
- **GPIO21, 22, 23**: Free GPIO pins that don't conflict with:
  - USB pins (GPIO12, GPIO13)
  - Boot/strapping pins (GPIO8, GPIO9)
  - RF control (GPIO3, GPIO14)
  - Internal flash (GPIO4-7)
- **GPIO16, 17**: Hardware UART pins for reliable communication

**ESP32S3 Pin Selection Rationale:**
- **GPIO43, 44**: Hardware UART pins (U0TXD, U0RXD)
- These are the only reliable UART pins available on the XIAO ESP32S3 form factor

### Pins to Avoid

**On ESP32C6:**
- GPIO0, 1: Used for ADC/general purpose - kept free
- GPIO3: RF switch power control - DO NOT USE
- GPIO4-7: Flash/JTAG - DO NOT USE
- GPIO8, 9: Strapping pins - DO NOT USE
- GPIO12, 13: USB - DO NOT USE
- GPIO14: Antenna selection - DO NOT USE
- GPIO15: Onboard LED

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

## Troubleshooting

### CC1101 Not Detected
- Verify all SPI connections (SCK, MISO, MOSI, CS)
- Check 3.3V power supply to CC1101
- Ensure GND is properly connected
- Verify CC1101 module is not damaged (try with multimeter continuity test)

### UART Communication Failure
- Verify TX/RX are cross-connected (S3 TX → C6 RX, S3 RX → C6 TX)
- Check baud rate settings match on both sides
- Ensure common ground between S3 and C6

### Build Errors
- For C6: Ensure ESP-IDF version 5.0 or later is installed
- For S3: Ensure PlatformIO and ESP32 platform are up to date
- Clean build: `idf.py fullclean` (C6) or `pio run -t clean` (S3)

## Comparison with Default Configuration

| Component | Default Config | XIAO Config | Reason for Change |
|-----------|---------------|-------------|-------------------|
| C6 CC1101 SCK | GPIO0 | GPIO19 | Hardware SPI, avoid ADC |
| C6 CC1101 MISO | GPIO7 | GPIO20 | GPIO7 doesn't exist on XIAO |
| C6 CC1101 MOSI | GPIO1 | GPIO18 | Hardware SPI alignment |
| C6 CC1101 CSN | GPIO4 | GPIO21 | GPIO4 used for flash |
| C6 CC1101 GDO0 | GPIO5 | GPIO22 | GPIO5 used for flash |
| C6 CC1101 GDO2 | GPIO6 | GPIO23 | GPIO6 used for flash |
| C6 UART TX | GPIO3 | GPIO16 | GPIO3 controls RF switch |
| C6 UART RX | GPIO2 | GPIO17 | Match hardware UART |
| S3 UART TX | GPIO17 | GPIO43 | GPIO17 not on XIAO |
| S3 UART RX | GPIO18 | GPIO44 | GPIO18 not on XIAO |

## Additional Resources

- [Seeedstudio XIAO ESP32S3 Wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [Seeedstudio XIAO ESP32C6 Wiki](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/)
- [CC1101 Datasheet](https://www.ti.com/product/CC1101)
- [OpenEPaperLink Wiki](https://github.com/jjwbruijn/OpenEPaperLink/wiki)

## License

This configuration follows the main OpenEPaperLink project license:
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
