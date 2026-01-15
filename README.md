# E-Paper Train Times Display

This program fetches train departure times from a JSON API and displays them on a Waveshare 7.5-inch e-paper display (800x480px) connected to an ESP32.

## Features

- **WiFi Connectivity**: Connects to local WiFi network
- **JSON API Integration**: Fetches real-time train data from `http://192.168.1.131:5001/api/departures`
- **Two-Column Display**: Shows Platform 1 and Platform 2 departures side by side
- **Real-time Updates**: Collect data every 30 seconds, only refresh when necessary
- **Memory Efficient**: Uses minimal memory with 1-bit monochrome display
- **Error Handling**: Displays clear error messages when issues occur
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

- The display uses 1-bit monochrome mode for maximum memory efficiency
- Train destinations are truncated with a dot if too long to fit in the column
- Delayed trains are indicated in the status column
- The ESP32 has sufficient memory for all operations
- When the json response is empty, it will show 'no services'
- Project based on the demo files provided by Waveshare.
