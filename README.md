
# ESPCast

ESPCast is an ESP32-based smart display device that combines Chromecast discovery and control capabilities with a rich touchscreen interface. Built on the ESP-IDF framework, it provides a comprehensive solution for wireless media control and device interaction.

**Hardware Platform:** This project is currently based on the [Waveshare ESP32-S3-Touch-LCD-1.46B](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.46B) development board, which provides the ESP32-S3 microcontroller, 1.46" round LCD display, and integrated touch functionality.

**Development:** This project was developed with AI assistance using Augment Agent, demonstrating collaborative human-AI software development for embedded systems.

## Overview

ESPCast transforms an ESP32 microcontroller into a sophisticated smart display that can:

- **Discover and control Chromecast devices** on your local network using mDNS
- **Provide a touchscreen GUI** for WiFi management and Chromecast interaction
- **Play audio** through high-quality DAC output
- **Monitor environmental sensors** including accelerometer/gyroscope and real-time clock
- **Manage power** with battery monitoring and power key controls
- **Store data** on SD card storage

## Key Features

### 🎵 **Audio & Media**
- **PCM5101 DAC** for high-quality audio output
- **MP3 playback** from SD card storage
- **Microphone support** for voice input and speech recognition
- **Audio player** with volume control and playback management

### 📱 **Display & Touch Interface**
- **412x412 pixel color display** (SPD2010 controller)
- **Multi-point capacitive touch** support (up to 5 touch points)
- **LVGL-based GUI** with tabbed interface
- **Backlight control** with PWM brightness adjustment

### 🌐 **Networking & Chromecast**
- **WiFi station mode** with automatic connection
- **mDNS-based Chromecast discovery**
- **Chromecast device control** via Google Cast protocol
- **TLS-secured communication** with Chromecast devices
- **Default WiFi credentials** support for easy deployment

### 🔧 **Hardware Integration**
- **ESP32-S3** microcontroller support
- **I2C sensor bus** for multiple peripherals
- **SD card storage** via MMC interface
- **Battery monitoring** with voltage measurement
- **Power management** with sleep/wake functionality
- **GPIO expansion** via TCA9554 I/O expander

### 📊 **Sensors & Monitoring**
- **QMI8658 6-axis IMU** (accelerometer + gyroscope)
- **PCF85063 RTC** for real-time clock functionality
- **Battery voltage monitoring**
- **Power key detection** with interrupt handling

## Hardware Components

**Base Platform:** [Waveshare ESP32-S3-Touch-LCD-1.46B](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.46B)

### Core Processing
- **ESP32-S3** microcontroller with WiFi/Bluetooth
- **TCA9554PWR** I2C GPIO expander for additional I/O

### Display & Input
- **SPD2010** 412x412 LCD controller with QSPI interface
- **Capacitive touch controller** integrated with display
- **PWM backlight control**

### Audio System
- **PCM5101** high-quality stereo DAC
- **I2S audio interface** (44.1kHz, 16-bit stereo)
- **Microphone input** for voice commands

### Storage & Connectivity
- **SD/MMC card slot** for media storage
- **WiFi 802.11 b/g/n** for network connectivity
- **Bluetooth** support (ESP32-S3 integrated)

### Sensors & Power
- **QMI8658** 6-axis inertial measurement unit
- **PCF85063** real-time clock with battery backup
- **Battery monitoring** circuit
- **Power management** with sleep modes

## Software Architecture

### Main Components

#### **ESP Cast Core** (`main/Cast/`)
- `esp_cast.c` - Main application logic and GUI initialization
- `wifi_manager.c` - WiFi connection management with auto-connect
- `wifi_gui_manager.c` - WiFi configuration GUI
- `chromecast_discovery_wrapper.cpp` - mDNS-based device discovery
- `chromecast_controller_wrapper.cpp` - Chromecast device control
- `chromecast_gui_manager.c` - Chromecast control interface

#### **Hardware Drivers** (`main/*/`)
- **Display**: SPD2010 LCD driver with LVGL integration
- **Touch**: Multi-point capacitive touch handling
- **Audio**: PCM5101 DAC with I2S interface
- **Sensors**: QMI8658 IMU and PCF85063 RTC drivers
- **Storage**: SD card interface for media files
- **Power**: Battery monitoring and power management

#### **User Interface** (`main/LVGL_*/`)
- **LVGL graphics library** for smooth GUI rendering
- **Tabbed interface** with WiFi and Chromecast controls
- **Touch event handling** with gesture support
- **Real-time status updates** and device feedback

### External Components
- **chromecast_controller** - C++ library for Google Cast protocol
- **chromecast_discovery** - mDNS-based device discovery
- **esp-audio-player** - MP3 playback and audio management
- **esp-dsp** - Digital signal processing capabilities
- **lvgl** - Graphics and UI framework

## Getting Started

### Prerequisites
- **ESP-IDF v4.4+** development framework
- **ESP32-S3** development board with compatible hardware
- **CMake** and **Ninja** build tools

### Building the Project

1. **Clone the repository:**
   ```bash
   git clone <repository-url>
   cd ESPCast
   ```

2. **Set up ESP-IDF environment:**
   ```bash
   . $IDF_PATH/export.sh
   ```

3. **Configure the project:**
   ```bash
   idf.py menuconfig
   ```

4. **Build and flash:**
   ```bash
   idf.py build
   idf.py flash monitor
   ```

### Configuration

#### WiFi Setup
Configure default WiFi credentials for automatic connection:

```bash
idf.py menuconfig
# Navigate to: Example Configuration → Default WiFi Configuration
```

Or edit `sdkconfig.defaults`:
```
CONFIG_DEFAULT_WIFI_ENABLED=y
CONFIG_DEFAULT_WIFI_SSID="YourWiFiNetwork"
CONFIG_DEFAULT_WIFI_PASSWORD="YourWiFiPassword"
```

See [DEFAULT_WIFI_CONFIG.md](DEFAULT_WIFI_CONFIG.md) for detailed WiFi configuration options.

#### Hardware Configuration
Adjust pin assignments and hardware settings in:
- `main/*/` driver header files
- `sdkconfig.defaults` for component-specific settings

## Usage

### Basic Operation

1. **Power on** the device
2. **WiFi connection** - Device automatically connects using default or saved credentials
3. **Touch interface** - Use the touchscreen to navigate between WiFi and Chromecast tabs
4. **Chromecast discovery** - Scan for available Chromecast devices on the network
5. **Device control** - Select and control Chromecast devices (volume, status)

### GUI Interface

#### WiFi Tab
- **Network scanning** and connection management
- **Saved credentials** management
- **Connection status** and signal strength display
- **Manual network configuration**

#### Chromecast Tab
- **Device discovery** with automatic scanning
- **Device list** with connection status
- **Volume control** for connected devices
- **Real-time status** updates

### Audio Playback
- **SD card support** for MP3 file playback
- **Volume control** via touch interface
- **High-quality audio** through PCM5101 DAC

## Development

### Adding New Features
The modular architecture makes it easy to extend functionality:

- **New sensors** - Add drivers in `main/` with I2C integration
- **GUI elements** - Extend LVGL interface in `main/LVGL_UI/`
- **Network protocols** - Add new discovery or control methods
- **Audio formats** - Extend audio player capabilities

### Testing
- **Hardware testing** - Individual component test functions
- **WiFi testing** - `esp_cast_test_default_wifi()` and related functions
- **Chromecast testing** - Device discovery and control validation

### Debugging
- **Serial monitor** - `idf.py monitor` for real-time logging
- **Component logs** - Detailed logging for each subsystem
- **Memory monitoring** - Built-in heap and stack monitoring

## Protocol Buffer Setup

For Chromecast communication, compile the protocol buffers:

```bash
protoc-c --c_out=. cast_channel.proto
protoc --cpp_out=../. ./components/chromecast_controller/chromecast_protobuf/*.proto
```

## License

This project uses various open-source components. See individual component licenses for details.

## Contributing

Contributions are welcome! Please ensure:
- **Code follows** ESP-IDF coding standards
- **Hardware changes** are documented
- **New features** include appropriate testing
- **Documentation** is updated for user-facing changes