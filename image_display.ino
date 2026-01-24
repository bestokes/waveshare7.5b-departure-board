/* E-Paper Train Times Display
 * Fetches and displays train departure times from JSON API on Waveshare 7.5" e-paper display
 */

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <WiFi.h>
#include <HTTPClient.h>

#include <time.h>

/* WiFi credentials */
const char* ssid = "ssid";
const char* password = "password";

/* NTP Server */
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

/* API URL */
const char* apiUrl = "http://192.168.1.131:5001/api/departures";

/* Display buffer */
UBYTE *imageBuffer;
UWORD imageSize;

/* Refresh interval */
unsigned long refreshInterval = 60000; // Default 1 minute
unsigned long lastRefresh = 0;
unsigned long lastFullRefresh = 0;
const unsigned long fullRefreshInterval = 3600000; // 1 hour

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
void performPartialTimestampUpdate(char* lastUpdated);
void drawMainScreen(char* stationName, char* lastUpdated, char* platform1Data, char* platform2Data);
bool isQuietHours();

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
  
  // Initialize paint with image buffer
  Paint_NewImage(imageBuffer, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);
  Paint_SelectImage(imageBuffer);
  Paint_Clear(WHITE);
  Paint_SetScale(2);
  Paint_SetRotate(ROTATE_0);

  // Show "Initialising, please wait ..." screen
  Paint_DrawString_EN(180, 220, "Hassocks Station Departure Board", &Font20, WHITE, BLACK);
  EPD_7IN5_V2_Display(imageBuffer);
  DEV_Delay_ms(500);
  
  // Connect to WiFi
  if (!connectToWiFi()) {
    printf("WiFi connection failed! Using offline mode.\r\n");
  }
  
  // Configure time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
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
      refreshInterval = 30000;
    }
    
    lastRefresh = currentTime;
    
    // Check for quiet hours
    if (isQuietHours()) {
        printf("Quiet hours (01:00-05:00), sleeping longer...\n");
        EPD_7IN5_V2_Sleep();
        delay(300000); // 5 minutes 
        
        // After waking from quiet hours, we must re-init fully
        EPD_7IN5_V2_Init();
        firstDisplay = true; 
        lastRefresh = millis();
        return;
    }
  }
  
  delay(100);
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
  http.setTimeout(10000);
  
  printf("Fetching train times from: %s\n", apiUrl);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    http.end();
    
    printf("Received JSON data (%d bytes)\n", payload.length());
    
    char stationName[50] = "Hassocks";
    char lastUpdated[20] = "--:--:--";
    char platform1Data[2000] = "";
    char platform2Data[2000] = "";
    
    if (!parseJSONResponse(payload, stationName, lastUpdated, platform1Data, platform2Data)) {
      displayError("JSON Parse Error");
      return false;
    }
    
    bool dataChanged = (strcmp(platform1Data, lastPlatform1Data) != 0) || 
                       (strcmp(platform2Data, lastPlatform2Data) != 0);
    
    unsigned long currentTime = millis();
    bool hourlyRefreshNeeded = (currentTime - lastFullRefresh >= fullRefreshInterval);

    if (!dataChanged && !firstDisplay && !hourlyRefreshNeeded) {
        printf("Data unchanged, partial update only.\n");
        performPartialTimestampUpdate(lastUpdated);
        strcpy(lastUpdatedTime, lastUpdated); 
        refreshInterval = 60000; // 1 minute when data unchanged
        return true;
    }
    
    printf("Redrawing main screen\n");
    
    // Always use full Init for main screen redraw as requested (no Init_Fast)
    EPD_7IN5_V2_Init();
    lastFullRefresh = currentTime;
    
    drawMainScreen(stationName, lastUpdated, platform1Data, platform2Data);
    EPD_7IN5_V2_Display(imageBuffer);
    
    firstDisplay = false;
    strcpy(lastUpdatedTime, lastUpdated);
    strcpy(lastPlatform1Data, platform1Data);
    strcpy(lastPlatform2Data, platform2Data);
    
    // Set timing for next check
    if (dataChanged) {
        printf("Data changed, next refresh in 2 minutes.\n");
        refreshInterval = 120000; // 2 minutes
    } else {
        printf("Data unchanged (or first/hourly), next refresh in 1 minute.\n");
        refreshInterval = 60000; // 1 minute
    }
    
    return true;
    
  } else {
    printf("HTTP error: %d\n", httpCode);
    char errorMsg[50];
    snprintf(errorMsg, sizeof(errorMsg), "HTTP Error: %d", httpCode);
    displayError(errorMsg);
    http.end();
    return false;
  }
}

void displayTrainRow(int platform, int row, const char* time, const char* destination, const char* operatorName, const char* status, bool isDelayed) {
  int yPos = 110 + (row * 70);
  int platformOffset = (platform == 1) ? 0 : 400;
  
  Paint_DrawString_EN(40 + platformOffset, yPos, time, &Font16, WHITE, BLACK);
  
  char truncatedDest[40];
  strncpy(truncatedDest, destination, 39);
  truncatedDest[39] = '\0';
  if (strlen(truncatedDest) > 13) {
      truncatedDest[13] = '.';
      truncatedDest[14] = '\0';
  }
  Paint_DrawString_EN(100 + platformOffset, yPos, truncatedDest, &Font16, WHITE, BLACK);
  
  char truncatedOp[30];
  strncpy(truncatedOp, operatorName, 29);
  truncatedOp[29] = '\0';
  Paint_DrawString_EN(100 + platformOffset, yPos + 20, truncatedOp, &Font12, WHITE, BLACK);
  
  Paint_DrawString_EN(280 + platformOffset, yPos, status, &Font16, WHITE, BLACK);
}

void displayError(const char* message) {
  // Reset Paint to full screen in case of error
  Paint_NewImage(imageBuffer, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);
  Paint_SelectImage(imageBuffer);
  Paint_Clear(WHITE);
  Paint_DrawString_EN(200, 180, "Error", &Font24, WHITE, BLACK);
  Paint_DrawString_EN(150, 220, message, &Font20, WHITE, BLACK);
  Paint_DrawString_EN(180, 260, "Retrying...", &Font20, WHITE, BLACK);
  EPD_7IN5_V2_Display(imageBuffer);
}

void drawMainScreen(char* stationName, char* lastUpdated, char* platform1Data, char* platform2Data) {
  // IMPORTANT: Reset Paint attributes to full screen size
  Paint_NewImage(imageBuffer, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);
  Paint_SelectImage(imageBuffer);
  Paint_Clear(WHITE);
  
  char header[100];
  snprintf(header, sizeof(header), "%s Station", stationName);
  Paint_DrawString_EN(20, 10, header, &Font24, WHITE, BLACK);
  
  char updateStr[50];
  snprintf(updateStr, sizeof(updateStr), "Updated: %s", lastUpdated);
  Paint_DrawString_EN(600, 10, updateStr, &Font16, WHITE, BLACK);
  
  Paint_DrawString_EN(40, 50, "Platform 1", &Font20, WHITE, BLACK);
  Paint_DrawString_EN(440, 50, "Platform 2", &Font20, WHITE, BLACK);
  
  Paint_DrawString_EN(40, 80, "Time", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(100, 80, "Destination", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(280, 80, "Status", &Font16, WHITE, BLACK);
  
  Paint_DrawString_EN(440, 80, "Time", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(500, 80, "Destination", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(680, 80, "Status", &Font16, WHITE, BLACK);
  
  if (strlen(platform1Data) == 0 || strcmp(platform1Data, "[]") == 0) {
      Paint_DrawString_EN(40, 110, "No services", &Font20, WHITE, BLACK);
  } else {
      for (int row = 0; row < 5; row++) {
        char time[10] = "", destination[50] = "", operatorName[30] = "", status[20] = "", statusClass[20] = "";
        bool isCancelled = false;
        if (extractTrainData(platform1Data, row, time, destination, operatorName, status, statusClass, &isCancelled)) {
          displayTrainRow(1, row, time, destination, operatorName, status, strcmp(statusClass, "status-delayed") == 0);
        }
      }
  }
  
  if (strlen(platform2Data) == 0 || strcmp(platform2Data, "[]") == 0) {
      Paint_DrawString_EN(440, 110, "No services", &Font20, WHITE, BLACK);
  } else {
      for (int row = 0; row < 5; row++) {
        char time[10] = "", destination[50] = "", operatorName[30] = "", status[20] = "", statusClass[20] = "";
        bool isCancelled = false;
        if (extractTrainData(platform2Data, row, time, destination, operatorName, status, statusClass, &isCancelled)) {
          displayTrainRow(2, row, time, destination, operatorName, status, strcmp(statusClass, "status-delayed") == 0);
        }
      }
  }
}

void performPartialTimestampUpdate(char* lastUpdated) {
    EPD_7IN5_V2_Init_Part_NoReset();
    
    UWORD updateW = 200, updateH = 30;
    UWORD bufferSize = ((updateW % 8 == 0) ? (updateW / 8) : (updateW / 8 + 1)) * updateH;
    UBYTE *timeBuffer = (UBYTE *)malloc(bufferSize);
    
    if (timeBuffer == NULL) return;
    
    // Create tiny image for partial refresh
    Paint_NewImage(timeBuffer, updateW, updateH, 0, WHITE);
    Paint_SelectImage(timeBuffer);
    Paint_Clear(WHITE);
    
    char updateStr[50];
    snprintf(updateStr, sizeof(updateStr), "Updated: %s", lastUpdated);
    Paint_DrawString_EN(0, 0, updateStr, &Font16, WHITE, BLACK);
    
    EPD_7IN5_V2_Display_Part(timeBuffer, 600, 10, 600 + updateW, 10 + updateH);
    free(timeBuffer);
}

bool isQuietHours() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) {
        return false;
    }
    if (timeinfo.tm_hour >= 1 && timeinfo.tm_hour < 5) {
        return true;
    }
    return false;
}

bool extractTrainData(const char* platformData, int trainIndex, char* time, char* destination, char* operatorName, char* status, char* statusClass, bool* isCancelled) {
  if (strlen(platformData) == 0) return false;
  const char* currentPos = platformData;
  for (int i = 0; i <= trainIndex; i++) {
    currentPos = strstr(currentPos, "{");
    if (currentPos == NULL) return false;
    if (i < trainIndex) {
      currentPos = strstr(currentPos, "},");
      if (currentPos == NULL) return false;
      currentPos += 2;
    }
  }
  const char* trainEnd = strstr(currentPos, "}");
  if (trainEnd == NULL) return false;
  String trainStr = String(currentPos).substring(0, trainEnd - currentPos);
  
  int timeStart = trainStr.indexOf("\"std\":\"");
  if (timeStart != -1) {
    timeStart += 7;
    int timeEnd = trainStr.indexOf("\"", timeStart);
    if (timeEnd != -1) trainStr.substring(timeStart, timeEnd).toCharArray(time, 10);
  }
  int destStart = trainStr.indexOf("\"destination\":\"");
  if (destStart != -1) {
    destStart += 15;
    int destEnd = trainStr.indexOf("\"", destStart);
    if (destEnd != -1) trainStr.substring(destStart, destEnd).toCharArray(destination, 50);
  }
  int opStart = trainStr.indexOf("\"operator\":\"");
  if (opStart != -1) {
    opStart += 12;
    int opEnd = trainStr.indexOf("\"", opStart);
    if (opEnd != -1) trainStr.substring(opStart, opEnd).toCharArray(operatorName, 30);
  }
  int statusStart = trainStr.indexOf("\"etd\":\"");
  if (statusStart != -1) {
    statusStart += 7;
    int statusEnd = trainStr.indexOf("\"", statusStart);
    if (statusEnd != -1) trainStr.substring(statusStart, statusEnd).toCharArray(status, 20);
  }
  int classStart = trainStr.indexOf("\"status_class\":\"");
  if (classStart != -1) {
    classStart += 16;
    int classEnd = trainStr.indexOf("\"", classStart);
    if (classEnd != -1) trainStr.substring(classStart, classEnd).toCharArray(statusClass, 20);
  }
  int cancelStart = trainStr.indexOf("\"is_cancelled\":");
  if (cancelStart != -1) {
    cancelStart += 15;
    int cancelEnd = trainStr.indexOf(",", cancelStart);
    if (cancelEnd == -1) cancelEnd = trainStr.indexOf("}", cancelStart);
    if (cancelEnd != -1) {
      String cancelStr = trainStr.substring(cancelStart, cancelEnd);
      *isCancelled = (cancelStr == "true");
    }
  }
  return true;
}

bool parseJSONResponse(String& payload, char* stationName, char* lastUpdated, char* platform1Data, char* platform2Data) {
  int updateStart = payload.indexOf("\"last_updated\":\"");
  if (updateStart != -1) {
    updateStart += 16; 
    int updateEnd = payload.indexOf("\"", updateStart);
    if (updateEnd != -1) payload.substring(updateStart, updateEnd).toCharArray(lastUpdated, 20);
  }
  int p1Start = payload.indexOf("\"platform_1\":[");
  if (p1Start != -1) {
    p1Start += 14; 
    int bracketCount = 1, p1End = p1Start;
    while (bracketCount > 0 && p1End < payload.length()) {
      if (payload[p1End] == '[') bracketCount++;
      else if (payload[p1End] == ']') bracketCount--;
      p1End++;
    }
    if (bracketCount == 0) payload.substring(p1Start, p1End-1).toCharArray(platform1Data, 2000);
  }
  int p2Start = payload.indexOf("\"platform_2\":[");
  if (p2Start != -1) {
    p2Start += 14; 
    int bracketCount = 1, p2End = p2Start;
    while (bracketCount > 0 && p2End < payload.length()) {
      if (payload[p2End] == '[') bracketCount++;
      else if (payload[p2End] == ']') bracketCount--;
      p2End++;
    }
    if (bracketCount == 0) payload.substring(p2Start, p2End-1).toCharArray(platform2Data, 2000);
  }
  return true;
}
