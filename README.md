# E-Paper Train Times Display

This program fetches train departure times from a JSON API and displays them on a Waveshare 7.5-inch e-paper display (800x480px) connected to an ESP32.

## Features

- **WiFi Connectivity**: Connects to local WiFi network
- **JSON API Integration**: Fetches real-time train data from `http://192.168.1.131:5001/api/departures`
- **Two-Column Display**: Shows Platform 1 and Platform 2 departures side by side
- **Real-time Updates**: Automatically refreshes every 30 seconds
- **Memory Efficient**: Uses minimal memory with 1-bit monochrome display
- **Error Handling**: Displays clear error messages when issues occur

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
   - ArduinoJson (install via Arduino Library Manager)

## Display Layout

The display shows train departures in a clean two-column format:

### Header Section
- Station name (e.g., "Hassocks Station")
- Last updated timestamp

### Platform Columns
- **Platform 1** (left column) and **Platform 2** (right column)
- Each platform shows up to 5 upcoming departures

### Train Information
- **Time**: Scheduled departure time (std)
- **Destination**: Destination station with operator in parentheses
- **Status**: Estimated departure time (etd) or "On time"

## JSON API Response Format

The program expects JSON data in this format:
```json
{
  "station_name": "Hassocks",
  "last_updated": "17:42:42",
  "platform_1": [
    {
      "destination": "London Victoria",
      "operator": "Southern",
      "std": "17:49",
      "etd": "17:51",
      "status_class": "status-delayed",
      "is_cancelled": false
    }
  ],
  "platform_2": [
    // Similar structure for platform 2
  ]
}
```

## Memory Usage

- **Image Buffer**: ~48KB (800x480 monochrome)
- **JSON Parsing**: ~8KB buffer for JSON data
- **WiFi/HTTP**: Additional memory for network operations
- **Total**: Well within ESP32's 520KB limit

## Operation

1. **Startup**: Initializes display and connects to WiFi
2. **Main Loop**: Every 30 seconds:
   - Fetches train data from JSON API
   - Parses and displays the information
   - Shows error messages if fetch fails
   - Puts display to sleep for 30 seconds

## Error Handling

The program includes comprehensive error handling:
- WiFi connection failures
- HTTP request failures
- JSON parsing errors
- Clear error messages displayed on screen

## Customization

- **Refresh Interval**: Modify `refreshInterval` variable (currently 30000ms)
- **Display Layout**: Adjust column positions and font sizes
- **Error Messages**: Customize error display in `displayError()` function

## Notes

- The display uses 1-bit monochrome mode for maximum memory efficiency
- Train destinations are truncated if too long to fit in the column
- Delayed trains are indicated in the status column
- The ESP32 has sufficient memory for all operations
