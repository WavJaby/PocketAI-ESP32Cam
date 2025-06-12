# PocketAI-ESP32Cam

A compact AI-powered camera system built on ESP32 with image capture, Wi-Fi connectivity, and button control functionality.

## Features

- **Image Capture**: High-quality image capture using ESP32-CAM module
- **Wi-Fi Connectivity**: 2.4G Wi-Fi connection
- **Button Controls**: 
  - Capture Mode
    - Single click: Capture image and send image to server, after process done, will go to `Result Mode`
  - Result Mode
    - Single click: Switch scroll mode (`Right`, `Left`, `Jump To Begin`)
    - Long press: Start scroll
    - Double click: Zoom in or zoom out. 
      - When in `Jump To Begin` mode, back to `Capture Mode`.
- **OTA Updates**: Over-the-air firmware update capability
- **SPI RAM Support**: Extended memory support for image processing

## Hardware Requirements

- ESP32-CAM module
- Push button (Connect to gpio)
- OLED Screen (SSD1306)

### Core Components

- **Main Application** (`main/main.c`): Core application logic with state management
- **Wi-Fi Control** (`main/module/wifi_control.h`): Wi-Fi connection and network management
- **Image Library** (`main/image_lib.h`): Image capture and processing utilities
- **OTA Updates** (`main/ota_update.h`): Firmware update functionality

### Application States

The system operates in different states:
- **Preview Mode**: Continuous image preview
- **Result Processing**: Image capture and processing
- **System Control**: Configuration and maintenance

## Configuration

### Wi-Fi Settings

Configure your Wi-Fi credentials in the project configuration:

```bash
idf.py menuconfig
```

Navigate to "Example Configuration" and set:
- Wi-Fi SSID
- Wi-Fi Password
- Maximum retry attempts
- Authentication mode (WPA2/WPA3)

## Building and Flashing

### Prerequisites

- ESP-IDF development framework
- USB UART cable for programming

### Build Process

```bash
# Configure the project
idf.py set-target esp32

# Configure project settings
idf.py menuconfig

# Build the project
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

**Note**: This project requires ESP-IDF framework and is designed specifically for ESP32-CAM modules.