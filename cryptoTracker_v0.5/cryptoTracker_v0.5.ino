// cryptoTracker tracks price movement of selected cryptocurrency
// v0.1 uses hard-coded WiFi credentials & does not display time
// v0.2 displays time and implements splash screen
// v0.3 implements coin class, bitmaps class, and snprintf()
// v0.4 changes DIN_PIN & adds ability to connect to ESP32 to change ssid/password
// v0.5 adds SPIFFS ssid/password storage and corrects coin price change error
// Created by Nathan Birenbaum 02/13/2022
// Last modified by Nathan Birenbaum 07/25/2022

// Hardware notes:
// ESP32 board: https://a.co/d/41ZWf6y
// OLED screen: https://a.co/d/c0FZxVO
// Select NodeMCU-32S from ESP32 Arduino boards manager
// Hold reset button to right of USB at "Connecting..." to begin upload

// Resources:
// Coingecko API documentation: https://www.coingecko.com/en/api/documentation

// To do:
// Adjust for daylight savings
// Shade missing Wifi status pieces
// Add server ping if price is not correct
// Display available networks: https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
// Improve w/ events (tutorial): https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
// Improve w/ events (video): https://www.youtube.com/watch?v=qwmLCWhfn9s
// Improve security: remove "client.setInsecure();" and use long-lasting certificate
// Encrypt flash storage of ssid/password: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#nvs-encryption


// Libraries
#include <UnixTime.h>          // Getting current time from ntpServer
#include <WiFi.h>              // 
#include <WiFiClientSecure.h>  // 
#include <HTTPClient.h>        // 
#include <ArduinoJson.h>       // 
#include <SPI.h>               // 
#include <AsyncTCP.h>          // Client for username/password input
#include <ESPAsyncWebServer.h> // Client for username/password input
#include <SPIFFS.h>            // Save ssid/password in EEPROM
#include "Ucglib.h"            // OLED screen graphics
#include "time.h"              // 
#include "coin.h"              // Coin class contains coin price data & strings
#include "bitmaps.h"           // Bitmaps class contains WiFi symbol bitmap

// True required for initial upload & false can be used later to save time
#define SPIFFS_MODE true

// ESP32 pin definitions & global OLED screen
#define CLK_PIN 18
#define DIN_PIN 21
#define DC_PIN  16
#define CS_PIN  17
#define RST_PIN 05
Ucglib_SSD1351_18x128x128_SWSPI ucg(CLK_PIN, DIN_PIN, DC_PIN, CS_PIN, RST_PIN);

// Global WiFi variables
Bitmaps art();
String ssid = "";
String password = "";
String deviceName = "ESP32 Crypto Tracker";
WiFiClientSecure client;
HTTPClient https;
int httpsCode = 0;

// Global server variables for updating WiFi credentials
AsyncWebServer server(80);
bool submitButtonPressed = false;
const char* serverSsid = "ESP32-Access-Point";
const char* serverPassword = "123456789";
const char* PARAM_INPUT_1 = "submittedSsid";
const char* PARAM_INPUT_2 = "submittedPassword";

// Global Coin variables
char urlPrice[] = "https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=usd&include_market_cap=true&include_24hr_vol=true&include_24hr_change=true&include_last_updated_at=true";
char urlPing[] = "https://api.coingecko.com/api/v3/ping";
char coinName[] = "ethereum";
Coin coin(urlPrice, urlPing, coinName);

// Global ntpServer variables
const char* ntpServer = "pool.ntp.org";
const long  gmtOffsetSec = -21600; // Central time
int daylightOffsetSec = 3600; // DST active
UnixTime lastUpdatedDateTime(-5);

// HTML webpage to enter new ssid & password
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP32 WiFi Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head><body><h1>ESP32 WiFi Setup</h1>
  <form action="/get">
    SSID: <input type="text" name="submittedSsid"><br>
    Password: <input type="password" name="submittedPassword"><br><br>
    <input type="submit" value="Submit">
  </form><br>
</body></html>)rawliteral";

// Initialize serial monitor, WiFi connection, & initial price fetch
void setup() {
  
  // Setup serial monitor for error messages
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Serial monitor initialized.");

  // Setup SPIFFS
  if(!SPIFFS.begin(SPIFFS_MODE)){
    Serial.println("Error mounting SPIFFS.");
    return;
  }

  // Initialize OLED screen, background color, & display splash screen
  ucg.begin(UCG_FONT_MODE_SOLID);
  displaySplashScreen();
  
  // Set host name & connect to WiFi
  WiFi.setHostname(deviceName.c_str());
  connectWiFi();
  drawWifi(WiFi.RSSI());
  
  // Initialize ntpServer and update the time
  configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);
  updateTime();
  
  // Initial price fetch
  fetchPrice();
  updatePrice();
  
  // Draw border lines
  drawBorderLines(2);
  
}

// Loops through the entire program
void loop() {
  
  // Static timing variables
  static unsigned long currentMs = 0;
  static unsigned long priorCxnCheckMs = 0;
  static const unsigned long cxnCheckIntervalMs = 5000; // 5 sec
  static unsigned long priorPriceFetchMs = 0;
  static const unsigned long priceFetchIntervalMs = 300000; // 5 min
  static unsigned long priorUpdateTimeMs = 0;
  static const unsigned long priorUpdateTimeIntervalMs = 1000; // 1 sec
  
  // Check WiFi connection periodically
  currentMs = millis();
  if (currentMs - priorCxnCheckMs >= cxnCheckIntervalMs) {
    if ((WiFi.status() != WL_CONNECTED)) {
      reconnectWiFi();
    }
    drawWifi(WiFi.RSSI());
    priorCxnCheckMs = currentMs;
  }
  
  // Update time periodically
  currentMs = millis();
  if (currentMs - priorUpdateTimeMs >= priorUpdateTimeIntervalMs) {
    updateTime();
    priorUpdateTimeMs = currentMs;
  }
  
  // Fetch & update price periodically
  currentMs = millis();
  if (currentMs - priorPriceFetchMs >= priceFetchIntervalMs) {
    fetchPrice();
    updatePrice();
    priorPriceFetchMs = currentMs;
  }
  
}

// Displays splash screen
void displaySplashScreen() {

  // Prepare screen
  ucg.clearScreen();
  ucg.setColor(1, 0, 0, 0); // Remove and see if needed
  ucg.setColor(200, 200, 255);
  ucg.setFont(ucg_font_ncenR12_hr);
  
  // Line 1
  ucg.setPrintPos(6, 20);
  ucg.print("Cryptocurrency");
  
  // Line 2
  ucg.setPrintPos(18, 40);
  ucg.print("tracker v0.3");

  // Draw border lines
  drawBorderLines(1);
  
  // Line 3
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(0, 112);
  ucg.print("github.com/nkbirenbaum/");
  
  // Line 4
  ucg.setPrintPos(0, 125);
  ucg.print("CryptoTracker");
  
}

// Connects to WiFi (status messages displayed over splash screen)
void connectWiFi() {

  // Read ssid & password from SPIFFS
  ssid = readFile(SPIFFS, "/ssid.txt");
  password = readFile(SPIFFS, "/password.txt");

  // Display connection status on OLED screen
  WiFi.mode(WIFI_STA);
  char ssidStr[128];
  snprintf(ssidStr, 128, "WiFi: %s", ssid.c_str());
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(0, 63);
  ucg.print(ssidStr);
  ucg.setPrintPos(0, 74);
  ucg.print("Connecting to network...");
  ucg.setPrintPos(0, 89);
  String connectionStatusStr = ".";
  ucg.print(connectionStatusStr);

  // Attempt to begin WiFi connection or reconnectWiFi() to change credentials
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), password.c_str());
  int cycles = 0;
  while (WiFi.status() != WL_CONNECTED) {
    ucg.print(connectionStatusStr);
    delay(500);
    cycles++;
    if (cycles > 20) {
      connectionStatusStr += "failed.";
      ucg.print(connectionStatusStr);
      delay(2000);
      reconnectWiFi();
      return;
    }
  }
  client.setInsecure();

  // Display connected message then clear screen
  connectionStatusStr += "connected!";
  ucg.print(connectionStatusStr);
  delay(3000);
  ucg.clearScreen();

}

// Reconnects to WiFi if connection is lost
void reconnectWiFi() {

  // Start ESP32 server and display IP address
  WiFi.mode(WIFI_AP);
  WiFi.softAP(serverSsid, serverPassword);
  IPAddress IP = WiFi.softAPIP();
  submitButtonPressed = false;
  Serial.print("ESP32 server IP address: ");
  Serial.println(IP);

  // Display info for reconnecting to WiFi on OLED display
  ucg.clearScreen();
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(0, 10);
  ucg.print("Error connecting to WiFi:");
  ucg.setPrintPos(0, 30);
  ucg.print(ssid);
  ucg.setPrintPos(0, 60);
  ucg.print("Please connect to network: ");
  ucg.setPrintPos(0, 80);
  ucg.print(serverSsid);
  ucg.setPrintPos(0, 100);
  ucg.print("And go to web address: ");
  ucg.setPrintPos(0, 120);
  ucg.print(IP);

  // Send webpage with input fields when client connects
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", webpage);
    Serial.println("Client connected to server.");
  });
  
  // Send GET request to IP to return PARAM_INPUT_1 & PARAM_INPUT_2 values
  // This block executes only when submit button is pressed
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasParam(PARAM_INPUT_1)) {
      ssid = request->getParam(PARAM_INPUT_1)->value();
      writeFile(SPIFFS, "/ssid.txt", ssid.c_str());
      Serial.println(ssid);
    }
    if (request->hasParam(PARAM_INPUT_2)) {
      password = request->getParam(PARAM_INPUT_2)->value();
      writeFile(SPIFFS, "/password.txt", password.c_str());
      Serial.println(password);
    }
    request->send(200, "text/html", "HTTP GET request sent to ESP32.<br><br><a href=\"/\">Back</a>");
    submitButtonPressed = true;
  });

  // 
  server.onNotFound(notFound);
  
  // Start server and wait until a response is submitted
  server.begin();
  while (!submitButtonPressed) {
    delay(10);
  }

  // Terminate server and try to connect with new credentials
  server.end();
  WiFi.softAPdisconnect();
  Serial.println("read tests:");
  String a = readFile(SPIFFS, "/ssid.txt");
  String b = readFile(SPIFFS, "/password.txt");
  Serial.println(a);
  Serial.println(b);
  displaySplashScreen();
  connectWiFi();
}

// ESP32 WiFi Setup webpage not found
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

// Fetches price from coingecko.com API & updates OLED screen with new price data
void fetchPrice() {
  
  // Get json data using https client
  https.begin(client, urlPrice);
  httpsCode = https.GET();
  const char* json = https.getString().c_str();
  https.end();
  
  // Parse json into doc
  DynamicJsonDocument doc(1024);
  DeserializationError deserializeError = deserializeJson(doc, json);
  if (deserializeError) {
    Serial.print("Deserialize error in fetchPrice(): ");
    Serial.println(deserializeError.f_str());
    return;
  }
  
  // Get cryptocurrency price data from json doc
  coin.price = doc["ethereum"]["usd"];
  coin.percentChange = doc["ethereum"]["usd_24h_change"];
  coin.oldPrice = coin.price / ( (coin.percentChange/100) + 1);
  coin.priceChange = coin.price - coin.oldPrice;
  coin.flagPositiveChange = (coin.percentChange > 0.0) ? true : false;
  String plusMinus = (coin.percentChange > 0.0) ? "+" : "-";

  // Create price-related strings
  snprintf(coin.priceBuffer1, Coin::bufferSize, "ETH: $%0.2f          ", coin.price);
  snprintf(coin.priceBuffer2, Coin::bufferSize, "24h: ");
  snprintf(coin.priceBuffer3, Coin::bufferSize, "%s$%0.2f             ", plusMinus, abs(coin.priceChange));
  snprintf(coin.priceBuffer4, Coin::bufferSize, "%s%0.2f%%            ", plusMinus, abs(coin.percentChange));
  
  // Get update time data from json doc & create update time-related strings
  long utcTimestamp = doc["ethereum"]["last_updated_at"];
  lastUpdatedDateTime.getDateTime(utcTimestamp);
  int monthInt = (int)lastUpdatedDateTime.month;
  int dayInt = (int)lastUpdatedDateTime.day;
  int yearInt = (int)lastUpdatedDateTime.year;
  int hourInt = hourMilitaryToRegular((int)lastUpdatedDateTime.hour);
  int minuteInt = (int)lastUpdatedDateTime.minute;
  String ampm = amOrPm((int)lastUpdatedDateTime.hour);
  snprintf(coin.timeBuffer1, Coin::bufferSize, "Price updated:");
  snprintf(coin.timeBuffer2, Coin::bufferSize, "%d/%d/%d at %02d:%02d %s          ", monthInt, dayInt, yearInt, hourInt, minuteInt, ampm);
  
}


void updatePrice() {
  
  drawWifi(WiFi.RSSI());
  ucg.setFont(ucg_font_ncenR12_hr);
  
  // Line 1
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(0, 50);
  ucg.print(coin.priceBuffer1);
  
  // Line 2
  ucg.setPrintPos(0, 70);
  ucg.print(coin.priceBuffer2);
  
  // Colored part of line 2
  if (coin.flagPositiveChange) {
    ucg.setColor(40, 255, 40);
  } else {
    ucg.setColor(255, 40, 40);
  }
  ucg.setPrintPos(40, 70);
  ucg.print(coin.priceBuffer3);
  
  // Line 3
  ucg.setPrintPos(40, 90);
  ucg.print(coin.priceBuffer4);
  
  // Line 4
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setPrintPos(0, 112);
  ucg.setColor(255, 255, 255);
  ucg.print(coin.timeBuffer1);
  
  // Line 5
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setPrintPos(0, 125);
  ucg.print(coin.timeBuffer2);
  
}

// Updates OLED screen WiFi icon
void drawWifi(int rssi) {
  
  // Determine signal strengthbased on rssi value
  int signalStrength;
  if (rssi >= -50) {
    signalStrength = 4;
  } else if (rssi >= -60) {
    signalStrength = 3;
  } else if (rssi >= -70) {
    signalStrength = 2;
  } else if (rssi >= -99) {
    signalStrength = 1;
  } else {
    signalStrength = 0;
  }
  
  // Do not upate if signal strength is the same as before
  static int signalStrengthOld = -1;
  if (signalStrength == signalStrengthOld) {
    return;
  }
  signalStrengthOld = signalStrength;
  
  // Clear WiFi graphics box
  const int xOffset = 100;
  const int yOffset = 0;
  ucg.setColor(0, 0, 0);
  ucg.drawBox(xOffset, yOffset, Bitmaps::wifiCols, Bitmaps::wifiRows);
  
  // Change color based on signal strength
  switch (signalStrength) {
    case 4:
      ucg.setColor(0, 255, 0);
      break;
    case 3:
      ucg.setColor(0, 255, 0);
      break;
    case 2:
      ucg.setColor(255, 255, 0);
      break;
    case 1:
      ucg.setColor(255, 0, 0);
      break;
    case 0:
      ucg.setColor(255, 0, 0);
      break;
  }
  
  // Draw Wifi signal strength
  drawWiFiSymbol(signalStrength);
  
}

// Pings coingecko.com API
void pingApi() {
  
  https.begin(client, urlPing);
  httpsCode = https.GET();
  const char* json = https.getString().c_str();
  https.end();
  DynamicJsonDocument doc(256);
  DeserializationError deserializeError = deserializeJson(doc, json);
  if (deserializeError) {
    Serial.print("Deserialize error in pingApi(): ");
    Serial.println(deserializeError.f_str());
    return;
  }
  String pingResult = doc["gecko_says"]; // Should be "(V3) To the Moon!"
  if (!pingResult.equalsIgnoreCase("(V3) To the Moon!")) {
    Serial.println("Ping to coingecko.com failed.");
  }
  
}

// Returns AM or PM depending on hour
String amOrPm(int currentHour) {
  
  if (currentHour < 12) {
    return "AM";
  } else {
    return "PM";
  }
  
}

// Returns hour adjusted for AM/PM
int hourMilitaryToRegular(int currentHour) {
  
  if (currentHour > 12) {
    return currentHour - 12;
  } else {
    return currentHour;
  }
  
}

// Updates OLED screen clock
void updateTime() {

  // Get timeinfo structure from ntpServer
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time using getLocalTime().");
    return;
  }

  // Display new time
  ucg.setFont(ucg_font_ncenR14_hr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(0, 20);
  char hourStr[4];
  strftime(hourStr, 4, "%H", &timeinfo);
  String ampm = amOrPm(atoi(hourStr));
  char hour12Str[4];
  strftime(hour12Str, 4, "%I", &timeinfo);
  char minStr[4];
  strftime(minStr, 4, "%M", &timeinfo);
  char currentTime[16];
  snprintf(currentTime, 16, "%s:%s %s", hour12Str, minStr, ampm);
  ucg.print(currentTime);
  
}

// Draws horizontal borders above & below price seciton of OLED screen
void drawBorderLines(int selection) {

  ucg.setColor(100, 100, 100);
  switch (selection) {
    case 1:   // Splash screen
      ucg.drawHLine(1, 46, 126);
      ucg.drawHLine(1, 98, 126);
      break;
    case 2:   // Main price screen
      ucg.drawHLine(1, 29, 126);
      ucg.drawHLine(1, 98, 126);
      break;
  }

}

// Draw Wifi signal strength
void drawWiFiSymbol(int signalStrength) {

  // Offset of corner of WiFi graphics box
  const int xOffset = 100;
  const int yOffset = 0;
  for (int x = 0; x < Bitmaps::wifiCols; x++) {
    for (int y = 0; y < Bitmaps::wifiRows; y++) {
      
      // Draw different pattern depending on signal strength (0-4 "bars")
      switch (signalStrength) {
        case 4:
          if (Bitmaps::wifi4[y][x]) {
            ucg.drawPixel(x + xOffset, y + yOffset);
          }
          break;
        case 3:
          if (Bitmaps::wifi3[y][x]) {
            ucg.drawPixel(x + xOffset, y + yOffset);
          }
          break;
        case 2:
          if (Bitmaps::wifi2[y][x]) {
            ucg.drawPixel(x + xOffset, y + yOffset);
          }
          break;
        case 1:
          if (Bitmaps::wifi1[y][x]) {
            ucg.drawPixel(x + xOffset, y + yOffset);
          }
          break;
        case 0:
          if (Bitmaps::wifi0[y][x]) {
            ucg.drawPixel(x + xOffset, y + yOffset);
          }
          break;
      }
    }
  }
  
}

// Reads file contents from SPIFFS
String readFile(fs::FS &fs, const char *path){

  // Open file for reading
  Serial.print("Reading file: ");
  Serial.println(path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("Error reading file.\nFile is empty or failed to open.");
    return "ERROR_READING_SPIFFS";
  }

  // Get contents
  String contents = "";
  while(file.available()){
    contents += String((char)file.read());
  }
  file.close();
  Serial.print("Contents: ");
  Serial.println(contents);
  return contents;
  
}

// Writes file to SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message){

  // Open file for writing
  Serial.print("Writing file: ");
  Serial.println(path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("Error opening file for writing.");
    return;
  }

  // Write contents
  if(file.print(message)){
    Serial.println("File written.");
  } else {
    Serial.println("File write failed.");
  }
  file.close();
  
}
