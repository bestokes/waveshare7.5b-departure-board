# E-Paper Train Times Display

This program fetches train departure times from a JSON API and displays them on a Waveshare 7.5-inch e-paper display (800x480px) connected to an ESP32.

## Features

- **WiFi Connectivity**: Connects to local WiFi network
- **JSON API Integration**: Fetches real-time train data from local api
- **Two-Column Display**: Shows Platform 1 and Platform 2 departures side by side
- **Memory Efficient**: Uses minimal memory with 1-bit monochrome display
- **Partial refresh**: Only update the updated time if everything else is unchanged

## Hardware Requirements

- ESP32 development board
- Waveshare 7.5-inch e-paper display (V2)
- SPI connections as defined in DEV_Config.h

## Setup

1. **WiFi Configuration**: Update the WiFi credentials in `image_display.ino`:
   ```cpp
   const char* ssid = "xxxxx";
   const char* password = "xxxxxxx";
   ```

2. **API URL**: Update the API URL if needed:
   ```cpp
   const char* apiUrl = "http://192.168.1.131:5001/api/departures";
   ```

3. **Install Required Libraries**:
   - WiFi (included with ESP32 Arduino core)
   - HTTPClient (included with ESP32 Arduino core)

## Notes

- Train destinations are truncated with a dot if too long to fit in the column
- Delayed trains are indicated in the status column
- When the json response is empty, it will show 'no services'
- This project is based on the e-paper demo files provided by Waveshare.
