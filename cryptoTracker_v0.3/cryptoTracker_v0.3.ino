// cryptoTracker tracks price movement of selected cryptocurrency
// v0.1 uses hard-coded WiFi credentials & does not display time
// v0.2 displays time and implements splash screen
// v0.3 implements coin class, bitmaps class, and snprintf
// Created by Nathan Birenbaum 02/13/2022
// Last modified by Nathan Birenbaum 07/20/2022

// Notes:
// For ESP32 use NodeMCU-32S from ESP32 Arduino boards manager
// https://www.amazon.com/gp/product/B0718T232Z/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1
// Must press reset button to right to right of USB once upload starts
// Coingecko API documentation: https://www.coingecko.com/en/api/documentation

// To do:
// Improve w/ events (tutorial): https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
// Improve w/ events (video): https://www.youtube.com/watch?v=qwmLCWhfn9s
// Improve security: remove "client.setInsecure();" and use long-lasting certificate
// Allow for changing credentials
// Adjust for daylight savings
// Shade missing Wifi status pieces
// Reformat Wifi connection "...s"
// Update reconnectWiFi() function (change to disconnected state)

// Libraries
#include <UnixTime.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include "Ucglib.h"
#include "time.h"
#include "coin.h"
#include "bitmaps.h"

// ESP32 pin definitions & global OLED screen
#define CLK_PIN 18
#define DIN_PIN 23
#define DC_PIN  16
#define CS_PIN  17
#define RST_PIN 05
Ucglib_SSD1351_18x128x128_SWSPI ucg(CLK_PIN, DIN_PIN, DC_PIN, CS_PIN, RST_PIN);

// Global WiFi variables
Bitmaps art();
String ssid = "**********";
String password = "**********";
String deviceName = "ESP32 Crypto Tracker";
WiFiClientSecure client;
HTTPClient https;
int httpsCode = 0;

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


// Initialize serial monitor, WiFi connection, & initial price fetch
void setup() {
  // Setup serial monitor for error messages
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Serial monitor initialized.");
  // Initialize OLED screen, background color, & display splash screen
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
  drawBorderLines();
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
  ucg.begin(UCG_FONT_MODE_SOLID);
  ucg.clearScreen();
  ucg.setColor(1, 0, 0, 0); // Remove and see if needed
  ucg.setColor(200, 200, 255);
  ucg.setFont(ucg_font_ncenR12_hr);
  // Line 1
  ucg.setPrintPos(8, 20);
  ucg.print("Cryptocurrency");
  // Line 2
  ucg.setPrintPos(22, 40);
  ucg.print("tracker v0.3");
  // Line 3
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setPrintPos(0, 112);
  ucg.print("github.com/nkbirenbaum/");
  // Line 4
  ucg.setPrintPos(0, 125);
  ucg.print("CryptoTracker");
}

// Connects to WiFi
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  char connectingStr[128];
  snprintf(connectingStr, 128, "WiFi: %s", ssid.c_str());
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setPrintPos(0, 70);
  ucg.print(connectingStr);
  ucg.setPrintPos(0, 83);
  String connectingDots = ".";
  ucg.print(connectingDots);
  connectingDots += ".";
  ucg.print(connectingDots);
  connectingDots += ".";
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    ucg.print(connectingDots);
    delay(250);
  }
  client.setInsecure();
  connectingDots += "connected!";
  ucg.print(connectingDots);
  delay(3000);
  ucg.clearScreen();
}

// Reconnects to WiFi if connection is lost
void reconnectWiFi() {
  Serial.println("WiFi connection lost. Attempting to reestablish.");
  WiFi.disconnect();
  WiFi.reconnect();
  connectWiFi();
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
  coin.priceChange = doc["ethereum"]["usd_24h_change"];
  coin.oldPrice = coin.price - coin.priceChange;
  coin.percentChange = 100 * coin.priceChange / coin.oldPrice;
  coin.flagPositiveChange = (coin.percentChange > 0.0) ? true : false;
  String plusMinus = (coin.percentChange > 0.0) ? "+" : "-";
  //
  snprintf(coin.priceBuffer1, Coin::bufferSize, "ETH: $%0.2f          ", coin.price);
  snprintf(coin.priceBuffer2, Coin::bufferSize, "24h: ");
  snprintf(coin.priceBuffer3, Coin::bufferSize, "%s$%0.2f             ", plusMinus, abs(coin.priceChange));
  snprintf(coin.priceBuffer4, Coin::bufferSize, "%s%0.2f%%            ", plusMinus, abs(coin.percentChange));
  // Get update time data from json doc
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
  // Clear WiFi box
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
  for (int x = 0; x < Bitmaps::wifiCols; x++) {
    for (int y = 0; y < Bitmaps::wifiRows; y++) {
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
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time using getLocalTime().");
    return;
  }
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

void drawBorderLines() {
  ucg.setColor(25, 30, 35);
  ucg.drawHLine(1, 29, 126);
  ucg.drawHLine(1, 98, 126);
}
