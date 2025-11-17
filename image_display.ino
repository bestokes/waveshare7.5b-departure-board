/* E-Paper Train Times Display
 * Fetches and displays train departure times from JSON API on Waveshare 7.5" e-paper display
 */

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <WiFi.h>
#include <HTTPClient.h>

/* WiFi credentials */
const char* ssid = "xxxxxx";
const char* password = "xxxxxxxxx";

/* API URL */
const char* apiUrl = "http://192.168.1.131:5001/api/departures";

/* Display buffer */
UBYTE *imageBuffer;
UWORD imageSize;

/* Refresh interval */
const unsigned long refreshInterval = 30000; // 30 seconds
unsigned long lastRefresh = 0;

/* Partial refresh tracking */
bool firstDisplay = true;
char lastPlatform1Data[2000] = "";
char lastPlatform2Data[2000] = "";
char lastUpdatedTime[20] = "";

/* Function prototypes */
bool connectToWiFi();
bool fetchAndDisplayTrainTimes();
void displayTrainTable();
void displayError(const char* message);
void displayTrainRow(int platform, int row, const char* time, const char* destination, const char* operatorName, const char* status, bool isDelayed);
bool parseJSONResponse(String& payload, char* stationName, char* lastUpdated, char* platform1Data, char* platform2Data);
bool extractTrainData(const char* platformData, int trainIndex, char* time, char* destination, char* operatorName, char* status, char* statusClass, bool* isCancelled);
void partialRefreshTrainData(char* lastUpdated, char* platform1Data, char* platform2Data);

/* Entry point ----------------------------------------------------------------*/
void setup()
{
  Serial.begin(115200);
  printf("EPD Train Times Display\r\n");
  
  // Initialize display
  DEV_Module_Init();
  printf("e-Paper Init...\r\n");
  EPD_7IN5_V2_Init();
  EPD_7IN5_V2_Clear();
  DEV_Delay_ms(500);

  // Calculate image buffer size (800x480 monochrome)
  imageSize = ((EPD_7IN5_V2_WIDTH % 8 == 0) ? (EPD_7IN5_V2_WIDTH / 8) : (EPD_7IN5_V2_WIDTH / 8 + 1)) * EPD_7IN5_V2_HEIGHT;
  
  printf("Buffer size: %d bytes\n", imageSize);
  
  // Allocate image buffer
  imageBuffer = (UBYTE *)malloc(imageSize);
  if (imageBuffer == NULL) {
    printf("Failed to allocate image memory!\r\n");
    while(1);
  }
  
  // Initialize paint with image buffer (1-bit monochrome)
  Paint_NewImage(imageBuffer, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);
  Paint_SetScale(2); // 1-bit mode for memory efficiency
  Paint_SetRotate(ROTATE_0);
  
  // Connect to WiFi
  if (!connectToWiFi()) {
    printf("WiFi connection failed! Using offline mode.\r\n");
  }
  
  // Display initial status
  Paint_SelectImage(imageBuffer);
  Paint_Clear(WHITE);
  Paint_DrawString_EN(200, 200, "Connecting...", &Font24, WHITE, BLACK);
  EPD_7IN5_V2_Display(imageBuffer);
  
  printf("Setup complete, starting main loop...\r\n");
}

/* Main loop ------------------------------------------------------------------*/
void loop()
{
  unsigned long currentTime = millis();
  
  // Check if it's time to refresh
  if (currentTime - lastRefresh >= refreshInterval) {
    printf("Refreshing train times...\r\n");
    
    // Fetch and display train times
    if (!fetchAndDisplayTrainTimes()) {
      printf("Failed to fetch train times, will retry in 30 seconds...\r\n");
    }
    
    lastRefresh = currentTime;
    
    // Put display to sleep
    printf("Going to sleep for 30 seconds...\r\n");
    EPD_7IN5_V2_Sleep();
    delay(30000); // Sleep for 30 seconds
    
    // Wake up display for next refresh
    if (firstDisplay) {
      EPD_7IN5_V2_Init();
    } else {
      EPD_7IN5_V2_Init_Fast(); // Use fast refresh mode for subsequent updates
    }
  }
  
  delay(100); // Small delay to prevent watchdog timeout
}

/* Connect to WiFi ------------------------------------------------------------*/
bool connectToWiFi() {
  printf("Connecting to WiFi: %s\r\n", ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    printf(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    printf("\nWiFi connected! IP: %s\r\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    printf("\nWiFi connection failed!\r\n");
    return false;
  }
}

/* Fetch and display train times from API ------------------------------------*/
bool fetchAndDisplayTrainTimes() {
  if (WiFi.status() != WL_CONNECTED) {
    printf("WiFi not connected, skipping update\r\n");
    displayError("WiFi Connection Failed");
    return false;
  }
  
  HTTPClient http;
  http.begin(apiUrl);
  http.setTimeout(10000); // 10 second timeout
  
  printf("Fetching train times from: %s\n", apiUrl);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    http.end();
    
    printf("Received JSON data (%d bytes)\n", payload.length());
    
    // Parse JSON manually (no external library needed)
    char stationName[50] = "Hassocks";
    char lastUpdated[20] = "--:--:--";
    char platform1Data[2000] = "";
    char platform2Data[2000] = "";
    
    if (!parseJSONResponse(payload, stationName, lastUpdated, platform1Data, platform2Data)) {
      displayError("JSON Parse Error");
      return false;
    }
    
    // Debug: Print extracted data
    printf("Last Updated: %s\n", lastUpdated);
    printf("Platform 1 Data Length: %d\n", strlen(platform1Data));
    printf("Platform 2 Data Length: %d\n", strlen(platform2Data));
    
    // Display the final image
    if (firstDisplay) {
      // First display - draw everything and do full refresh
      Paint_SelectImage(imageBuffer);
      Paint_Clear(WHITE);
      
      // Station header (left justified)
      char header[100];
      snprintf(header, sizeof(header), "%s Station", stationName);
      Paint_DrawString_EN(20, 10, header, &Font24, WHITE, BLACK);
      
      // Last updated (right justified on same line)
      char updateStr[50];
      snprintf(updateStr, sizeof(updateStr), "Updated: %s", lastUpdated);
      Paint_DrawString_EN(600, 10, updateStr, &Font16, WHITE, BLACK);
      
      // Platform headers
      Paint_DrawString_EN(40, 50, "Platform 1", &Font20, WHITE, BLACK);
      Paint_DrawString_EN(440, 50, "Platform 2", &Font20, WHITE, BLACK);
      
      // Column headers
      Paint_DrawString_EN(40, 80, "Time", &Font16, WHITE, BLACK);
      Paint_DrawString_EN(100, 80, "Destination", &Font16, WHITE, BLACK);
      Paint_DrawString_EN(280, 80, "Status", &Font16, WHITE, BLACK);
      
      Paint_DrawString_EN(440, 80, "Time", &Font16, WHITE, BLACK);
      Paint_DrawString_EN(500, 80, "Destination", &Font16, WHITE, BLACK);
      Paint_DrawString_EN(680, 80, "Status", &Font16, WHITE, BLACK);
      
      // Platform 1 trains
      for (int row = 0; row < 5; row++) {
        char time[10] = "";
        char destination[50] = "";
        char operatorName[30] = "";
        char status[20] = "";
        char statusClass[20] = "";
        bool isCancelled = false;
        
        if (extractTrainData(platform1Data, row, time, destination, operatorName, status, statusClass, &isCancelled)) {
          bool isDelayed = (strcmp(statusClass, "status-delayed") == 0);
          
          displayTrainRow(1, row, time, destination, operatorName, status, isDelayed);
        }
      }
      
      // Platform 2 trains
      for (int row = 0; row < 5; row++) {
        char time[10] = "";
        char destination[50] = "";
        char operatorName[30] = "";
        char status[20] = "";
        char statusClass[20] = "";
        bool isCancelled = false;
        
        if (extractTrainData(platform2Data, row, time, destination, operatorName, status, statusClass, &isCancelled)) {
          bool isDelayed = (strcmp(statusClass, "status-delayed") == 0);
          
          displayTrainRow(2, row, time, destination, operatorName, status, isDelayed);
        }
      }
      
      EPD_7IN5_V2_Display(imageBuffer);
      firstDisplay = false;
    } else {
      // Use partial refresh for train data area only
      partialRefreshTrainData(lastUpdated, platform1Data, platform2Data);
    }
    
    // Store current data for comparison
    strcpy(lastUpdatedTime, lastUpdated);
    strcpy(lastPlatform1Data, platform1Data);
    strcpy(lastPlatform2Data, platform2Data);
    
    printf("Train times displayed successfully\n");
    return true;
    
  } else {
    // HTTP error
    printf("HTTP error: %d\n", httpCode);
    
    char errorMsg[50];
    snprintf(errorMsg, sizeof(errorMsg), "HTTP Error: %d", httpCode);
    displayError(errorMsg);
    
    http.end();
    return false;
  }
}

/* Display a train row in the table ------------------------------------------*/
void displayTrainRow(int platform, int row, const char* time, const char* destination, const char* operatorName, const char* status, bool isDelayed) {
  int yPos = 110 + (row * 70);  // Increased row height for two-line layout
  int platformOffset = (platform == 1) ? 0 : 400;
  
  // Time column
  Paint_DrawString_EN(40 + platformOffset, yPos, time, &Font16, WHITE, BLACK);
  
  // Destination column (first line)
  char truncatedDest[40];
  strncpy(truncatedDest, destination, 39);
  truncatedDest[39] = '\0';
  Paint_DrawString_EN(100 + platformOffset, yPos, truncatedDest, &Font16, WHITE, BLACK);
  
  // Operator column (second line, smaller font)
  char truncatedOp[30];
  strncpy(truncatedOp, operatorName, 29);
  truncatedOp[29] = '\0';
  Paint_DrawString_EN(100 + platformOffset, yPos + 20, truncatedOp, &Font12, WHITE, BLACK);
  
  // Status column - use different color for delayed trains
  if (isDelayed) {
    Paint_DrawString_EN(280 + platformOffset, yPos, status, &Font16, WHITE, BLACK);
  } else {
    Paint_DrawString_EN(280 + platformOffset, yPos, status, &Font16, WHITE, BLACK);
  }
}

/* Display error message -----------------------------------------------------*/
void displayError(const char* message) {
  Paint_SelectImage(imageBuffer);
  Paint_Clear(WHITE);
  
  Paint_DrawString_EN(200, 180, "Error", &Font24, WHITE, BLACK);
  Paint_DrawString_EN(150, 220, message, &Font20, WHITE, BLACK);
  Paint_DrawString_EN(180, 260, "Retrying...", &Font20, WHITE, BLACK);
  
  EPD_7IN5_V2_Display(imageBuffer);
}

/* Display train table layout ------------------------------------------------*/
void displayTrainTable() {
  // This function is called before drawing individual rows to set up the layout
  // The actual drawing happens in displayTrainRow()
}

/* Parse JSON response manually ----------------------------------------------*/
bool parseJSONResponse(String& payload, char* stationName, char* lastUpdated, char* platform1Data, char* platform2Data) {
  // Extract last_updated
  int updateStart = payload.indexOf("\"last_updated\":\"");
  if (updateStart != -1) {
    updateStart += 16; // Skip "\"last_updated\":\""
    int updateEnd = payload.indexOf("\"", updateStart);
    if (updateEnd != -1) {
      payload.substring(updateStart, updateEnd).toCharArray(lastUpdated, 20);
    }
  }
  
  // Extract platform_1 data - find the entire array including brackets
  int platform1Start = payload.indexOf("\"platform_1\":[");
  if (platform1Start != -1) {
    platform1Start += 14; // Skip "\"platform_1\":["
    int bracketCount = 1;
    int platform1End = platform1Start;
    
    // Find the matching closing bracket for the array
    while (bracketCount > 0 && platform1End < payload.length()) {
      if (payload[platform1End] == '[') {
        bracketCount++;
      } else if (payload[platform1End] == ']') {
        bracketCount--;
      }
      platform1End++;
    }
    
    if (bracketCount == 0) {
      // platform1End now points to the character after the closing bracket
      // We need to go back one to get the actual closing bracket position
      platform1End--;
      payload.substring(platform1Start, platform1End).toCharArray(platform1Data, 2000);
    }
  }
  
  // Extract platform_2 data - find the entire array including brackets
  int platform2Start = payload.indexOf("\"platform_2\":[");
  if (platform2Start != -1) {
    platform2Start += 14; // Skip "\"platform_2\":["
    int bracketCount = 1;
    int platform2End = platform2Start;
    
    // Find the matching closing bracket for the array
    while (bracketCount > 0 && platform2End < payload.length()) {
      if (payload[platform2End] == '[') {
        bracketCount++;
      } else if (payload[platform2End] == ']') {
        bracketCount--;
      }
      platform2End++;
    }
    
    if (bracketCount == 0) {
      // platform2End now points to the character after the closing bracket
      // We need to go back one to get the actual closing bracket position
      platform2End--;
      payload.substring(platform2Start, platform2End).toCharArray(platform2Data, 2000);
    }
  }
  
  return true;
}

/* Fast refresh for train data area only ------------------------------------*/
void partialRefreshTrainData(char* lastUpdated, char* platform1Data, char* platform2Data) {
  // Always do a full refresh but use fast refresh mode
  // This avoids the noise issues with partial refresh
  
  Paint_SelectImage(imageBuffer);
  Paint_Clear(WHITE);
  
  // Station header (left justified)
  char header[100];
  snprintf(header, sizeof(header), "Hassocks Station"); // Hardcoded station name for now
  Paint_DrawString_EN(20, 10, header, &Font24, WHITE, BLACK);
  
  // Last updated (right justified on same line)
  char updateStr[50];
  snprintf(updateStr, sizeof(updateStr), "Updated: %s", lastUpdated);
  Paint_DrawString_EN(600, 10, updateStr, &Font16, WHITE, BLACK);
  
  // Platform headers
  Paint_DrawString_EN(40, 50, "Platform 1", &Font20, WHITE, BLACK);
  Paint_DrawString_EN(440, 50, "Platform 2", &Font20, WHITE, BLACK);
  
  // Column headers
  Paint_DrawString_EN(40, 80, "Time", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(100, 80, "Destination", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(280, 80, "Status", &Font16, WHITE, BLACK);
  
  Paint_DrawString_EN(440, 80, "Time", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(500, 80, "Destination", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(680, 80, "Status", &Font16, WHITE, BLACK);
  
  // Platform 1 trains
  for (int row = 0; row < 5; row++) {
    char time[10] = "";
    char destination[50] = "";
    char operatorName[30] = "";
    char status[20] = "";
    char statusClass[20] = "";
    bool isCancelled = false;
    
    if (extractTrainData(platform1Data, row, time, destination, operatorName, status, statusClass, &isCancelled)) {
      bool isDelayed = (strcmp(statusClass, "status-delayed") == 0);
      
      displayTrainRow(1, row, time, destination, operatorName, status, isDelayed);
    }
  }
  
  // Platform 2 trains
  for (int row = 0; row < 5; row++) {
    char time[10] = "";
    char destination[50] = "";
    char operatorName[30] = "";
    char status[20] = "";
    char statusClass[20] = "";
    bool isCancelled = false;
    
    if (extractTrainData(platform2Data, row, time, destination, operatorName, status, statusClass, &isCancelled)) {
      bool isDelayed = (strcmp(statusClass, "status-delayed") == 0);
      
      displayTrainRow(2, row, time, destination, operatorName, status, isDelayed);
    }
  }
  
  // Use fast display for subsequent updates
  EPD_7IN5_V2_Display(imageBuffer);
  printf("Fast refresh completed\n");
}

/* Extract train data from platform array ------------------------------------*/
bool extractTrainData(const char* platformData, int trainIndex, char* time, char* destination, char* operatorName, char* status, char* statusClass, bool* isCancelled) {
  if (strlen(platformData) == 0) {
    return false;
  }
  
  // Find the nth train object in the platform array
  const char* currentPos = platformData;
  for (int i = 0; i <= trainIndex; i++) {
    currentPos = strstr(currentPos, "{");
    if (currentPos == NULL) {
      return false;
    }
    
    if (i < trainIndex) {
      currentPos = strstr(currentPos, "},");
      if (currentPos == NULL) {
        return false;
      }
      currentPos += 2;
    }
  }
  
  const char* trainEnd = strstr(currentPos, "}");
  if (trainEnd == NULL) {
    return false;
  }
  
  // Extract fields from the train object
  String trainStr = String(currentPos).substring(0, trainEnd - currentPos);
  
  // Debug: Print the train object being parsed
  printf("Train %d object: %s\n", trainIndex, trainStr.c_str());
  
  // Extract std (time) - note: no space after colon in actual JSON
  int timeStart = trainStr.indexOf("\"std\":\"");
  if (timeStart != -1) {
    timeStart += 7; // Skip "\"std\":\""
    int timeEnd = trainStr.indexOf("\"", timeStart);
    if (timeEnd != -1) {
      trainStr.substring(timeStart, timeEnd).toCharArray(time, 10);
      printf("  Time: %s\n", time);
    }
  }
  
  // Extract destination - note: no space after colon in actual JSON
  int destStart = trainStr.indexOf("\"destination\":\"");
  if (destStart != -1) {
    destStart += 15; // Skip "\"destination\":\""
    int destEnd = trainStr.indexOf("\"", destStart);
    if (destEnd != -1) {
      trainStr.substring(destStart, destEnd).toCharArray(destination, 50);
      printf("  Destination: %s\n", destination);
    }
  }
  
  // Extract operator - note: no space after colon in actual JSON
  int opStart = trainStr.indexOf("\"operator\":\"");
  if (opStart != -1) {
    opStart += 12; // Skip "\"operator\":\""
    int opEnd = trainStr.indexOf("\"", opStart);
    if (opEnd != -1) {
      trainStr.substring(opStart, opEnd).toCharArray(operatorName, 30);
      printf("  Operator: %s\n", operatorName);
    }
  }
  
  // Extract etd (status) - note: no space after colon in actual JSON
  int statusStart = trainStr.indexOf("\"etd\":\"");
  if (statusStart != -1) {
    statusStart += 7; // Skip "\"etd\":\""
    int statusEnd = trainStr.indexOf("\"", statusStart);
    if (statusEnd != -1) {
      trainStr.substring(statusStart, statusEnd).toCharArray(status, 20);
      printf("  Status: %s\n", status);
    }
  }
  
  // Extract status_class - note: no space after colon in actual JSON
  int classStart = trainStr.indexOf("\"status_class\":\"");
  if (classStart != -1) {
    classStart += 16; // Skip "\"status_class\":\""
    int classEnd = trainStr.indexOf("\"", classStart);
    if (classEnd != -1) {
      trainStr.substring(classStart, classEnd).toCharArray(statusClass, 20);
      printf("  Status Class: %s\n", statusClass);
    }
  }
  
  // Extract is_cancelled - note: no space after colon in actual JSON
  int cancelStart = trainStr.indexOf("\"is_cancelled\":");
  if (cancelStart != -1) {
    cancelStart += 15; // Skip "\"is_cancelled\":"
    int cancelEnd = trainStr.indexOf(",", cancelStart);
    if (cancelEnd == -1) cancelEnd = trainStr.indexOf("}", cancelStart);
    if (cancelEnd != -1) {
      String cancelStr = trainStr.substring(cancelStart, cancelEnd);
      *isCancelled = (cancelStr == "true");
      printf("  Is Cancelled: %s\n", *isCancelled ? "true" : "false");
    }
  }
  
  return true;
}
