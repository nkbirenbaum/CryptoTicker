// cryptoTracker tracks price movement of selected cryptocurrency
// v0.1 uses hard-coded WiFi credentials & no time displayed
// Created by Nathan Birenbaum 02/13/2022
// Last modified by Nathan Birenbaum 07/05/2022
// For ESP32 use NodeMCU-32S from ESP32 Arduino boards manager
// https://www.amazon.com/gp/product/B0718T232Z/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1
// Must press reset button to right to right of USB once upload starts
// Coingecko API documentation: https://www.coingecko.com/en/api/documentation
// Improve w/ events (tutorial): https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
// Improve w/ events (video): https://www.youtube.com/watch?v=qwmLCWhfn9s
// Improve security: remove "client.setInsecure();" and use long-lasting certificate
// Adjust for daylight savings

// Libraries
#include <UnixTime.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include "Ucglib.h"
#include "time.h"

// ESP32 pin definitions
#define CLK_PIN 18
#define DIN_PIN 23
#define DC_PIN 16
#define CS_PIN 17
#define RST_PIN 05

// Global variables
const char* ssid = "**********";
const char* password = "**********";
String deviceName = "ESP32 Crypto Tracker";
WiFiClientSecure client;
HTTPClient https;
int httpsCode = 0;
int signalStrengthOld = -1;
const char urlPrice[] = "https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=usd&include_market_cap=true&include_24hr_vol=true&include_24hr_change=true&include_last_updated_at=true";
const char urlPing[] = "https://api.coingecko.com/api/v3/ping";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffsetSec = -18000;
const int   daylightOffsetSec = 3600;
UnixTime lastUpdatedDateTime(-5);
unsigned long currentMs = 0;
unsigned long priorCxnCheckMs = 0;
const unsigned long cxnCheckIntervalMs = 5000; // 5 sec
unsigned long priorPriceFetchMs = 0;
const unsigned long priceFetchIntervalMs = 300000; // 5 min
Ucglib_SSD1351_18x128x128_SWSPI ucg(CLK_PIN, DIN_PIN, DC_PIN, CS_PIN, RST_PIN); // OLED screen
char priceBuffer[128];
char priceBuffer1[64];
char priceBuffer2[64];
char priceBuffer3[64];
char priceBuffer4[64];
char timeBuffer[128];
char timeBuffer1[64];
char timeBuffer2[64];
bool flagPositiveChange;
String plusMinus;

// Coordinates for WiFi symbols
const int rows = 20;
const int cols = 28;
const bool wifi4[rows][cols] = { {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
                                 {0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
                                 {0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0},
                                 {0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0},
                                 {0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0},
                                 {1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1},
                                 {1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1},
                                 {0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0},
                                 {0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0},
                                 {0,0,0,0,0,0,1,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0}
                               };
const bool wifi3[rows][cols] = { {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0},
                                 {0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0},
                                 {0,0,0,0,0,0,1,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0}
                                };
const bool wifi2[rows][cols] = { {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0}
                               };
const bool wifi1[rows][cols] = { {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0}
                               };
const bool wifi0[rows][cols] = { {0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0},
                                 {0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0},
                                 {0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0},
                                 {0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0},
                                 {0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0},
                                 {0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
                                 {0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0},
                                 {0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0},
                                 {0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0},
                                 {0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0},
                               };

// Initialize serial monitor, WiFi connection, & initial price fetch
void setup() {
  // Setup serial monitor
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Serial monitor initialized.");
  // Initialize OLED screen, font, & background color
  ucg.begin(UCG_FONT_MODE_SOLID);
  ucg.clearScreen();
  ucg.setColor(1, 0, 0, 0);
  // Set host name & connect to WiFi
  WiFi.setHostname(deviceName.c_str());
  WiFi.mode(WIFI_STA);
  connectWiFi();
  client.setInsecure();
  Serial.println("-----------------");
  // Init and get the time
  configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);
  printLocalTime();
  Serial.println("-----------------");
  // Initial price fetch
  fetchPrice();
  updateScreen();
}

// Loops through the entire program
void loop() {
  // Check WiFi connection periodically
  currentMs = millis();
  if (currentMs - priorCxnCheckMs >= cxnCheckIntervalMs) {
    if ((WiFi.status() != WL_CONNECTED)) {
      reconnectWiFi();
    }
    drawWifi(WiFi.RSSI());
    priorCxnCheckMs = currentMs;
  }
  // Fetch price & update OLED screen periodically
  currentMs = millis();
  if (currentMs - priorPriceFetchMs >= priceFetchIntervalMs) {
    fetchPrice();
    updateScreen();
    priorPriceFetchMs = currentMs;
  }
}

// Connects to WiFi
void connectWiFi() {
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print("..");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(250);
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.print(ssid);
  Serial.println(".");
  Serial.print("Local IP address: ");
  Serial.println(WiFi.localIP());
  drawWifi(WiFi.RSSI());
}

// Reconnects to WiFi if connection is lost
void reconnectWiFi() {
  Serial.println("WiFi connection lost. Attempting to reestablish");
  WiFi.disconnect();
  WiFi.reconnect();
  connectWiFi();
}

// Fetches price from coingecko.com API
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
  // Get Ethereum price data from json doc
  double ethPrice = doc["ethereum"]["usd"];
  double ethPriceChange = doc["ethereum"]["usd_24h_change"];
  double ethOldPrice = ethPrice-ethPriceChange;
  double ethPercentChange = 100*ethPriceChange/ethOldPrice;
  flagPositiveChange = (ethPercentChange > 0.0) ? true : false;
  plusMinus = (ethPercentChange > 0.0) ? "+" : "-";
  sprintf(priceBuffer, "Ethereum: $%0.2f\n24 hr change: %s$%0.2f (%0.2f%%)", ethPrice, plusMinus, abs(ethPriceChange), abs(ethPercentChange));
  sprintf(priceBuffer1, "ETH: $%0.2f          ", ethPrice);
  sprintf(priceBuffer2, "24h: ");
  sprintf(priceBuffer3, "%s$%0.2f             ", plusMinus, abs(ethPriceChange));
  sprintf(priceBuffer4, "%s%0.2f%%            ", plusMinus, abs(ethPercentChange));
  Serial.println(priceBuffer);
  // Get update time data from json doc
  long utcTimestamp = doc["ethereum"]["last_updated_at"];
  lastUpdatedDateTime.getDateTime(utcTimestamp);
  String dayOfWeekStr = dayNumToStr((int)lastUpdatedDateTime.dayOfWeek);
  int monthInt = (int)lastUpdatedDateTime.month;
  int dayInt = (int)lastUpdatedDateTime.day;
  int yearInt = (int)lastUpdatedDateTime.year;
  int hourInt = hourMilitaryToRegular((int)lastUpdatedDateTime.hour);
  int minuteInt = (int)lastUpdatedDateTime.minute;
  String ampm = amOrPm((int)lastUpdatedDateTime.hour);
  sprintf(timeBuffer, "Price updated %s, %d/%d/%d at %02d:%02d %s.", dayOfWeekStr, monthInt, dayInt, yearInt, hourInt, minuteInt, ampm);
  sprintf(timeBuffer1, "Price updated:");
  sprintf(timeBuffer2, "%d/%d/%d at %02d:%02d %s          ", monthInt, dayInt, yearInt, hourInt, minuteInt, ampm);
  Serial.println(timeBuffer);
}

// Updates OLED screen with new data
void updateScreen() {
  drawWifi(WiFi.RSSI());
  ucg.setFont(ucg_font_ncenR12_tr);
  // Line 1
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(0,45);
  ucg.print(priceBuffer1);
  // Line 2
  ucg.setPrintPos(0,65);
  ucg.print(priceBuffer2);
  // Colored part of line 2
  if (flagPositiveChange) {
    ucg.setColor(40, 255, 40);  
  } else {
    ucg.setColor(255, 40, 40);
  }
  ucg.setPrintPos(36,65);
  ucg.print(priceBuffer3);
  // Line 3
  ucg.setPrintPos(36,85);
  ucg.print(priceBuffer4);
  // Line 4
  ucg.setFont(ucg_font_ncenR08_tr);
  ucg.setPrintPos(0,112);
  ucg.setColor(255, 255, 255);
  ucg.print(timeBuffer1);
  // Line 5
  ucg.setFont(ucg_font_ncenR08_tr);
  ucg.setPrintPos(0,125);
  ucg.print(timeBuffer2);
}

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
  if (signalStrength == signalStrengthOld) {
    return;
  }
  signalStrengthOld = signalStrength;
  // Clear WiFi box
  const int xOffset = 100;
  const int yOffset = 0;
  ucg.setColor(0, 0, 0);
  ucg.drawBox(xOffset, yOffset, cols, rows);
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
  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      switch (signalStrength) {
        case 4:
          if (wifi4[y][x]) {
            ucg.drawPixel(x+xOffset, y+yOffset);
          }
          break;
        case 3:
          if (wifi3[y][x]) {
            ucg.drawPixel(x+xOffset, y+yOffset);
          }
          break;
        case 2:
          if (wifi2[y][x]) {
            ucg.drawPixel(x+xOffset, y+yOffset);
          }
          break;
        case 1:
          if (wifi1[y][x]) {
            ucg.drawPixel(x+xOffset, y+yOffset);
          }
          break;
        case 0:
          if (wifi0[y][x]) {
            ucg.drawPixel(x+xOffset, y+yOffset);
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

// Converts day of week as number to string
String dayNumToStr(int dayOfWeekNum) {
  String dayOfWeekStr;
  switch (dayOfWeekNum) {
    case 1:
      dayOfWeekStr = "Monday";
      break;
    case 2:
      dayOfWeekStr = "Tuesday";
      break;
    case 3:
      dayOfWeekStr = "Wednesday";
      break;
    case 4:
      dayOfWeekStr = "Thursday";
      break;
    case 5:
      dayOfWeekStr = "Friday";
      break;
    case 6:
      dayOfWeekStr = "Saturday";
      break;
    case 7:
      dayOfWeekStr = "Sunday";
      break;
    default:
      dayOfWeekStr = "";
  }
  return dayOfWeekStr;
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
    return currentHour-12;
  } else {
    return currentHour;
  }
}

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
}

/* outputs:
 * 17:21:51.492 -> Wednesday, July 06 2022 18:21:50
17:21:51.492 -> Day of week: Wednesday
17:21:51.492 -> Month: July
17:21:51.492 -> Day of Month: 06
17:21:51.492 -> Year: 2022
17:21:51.492 -> Hour: 18
17:21:51.492 -> Hour (12 hour format): 06
17:21:51.492 -> Minute: 21
17:21:51.492 -> Second: 50
17:21:51.492 -> Time variables
17:21:51.492 -> 18
17:21:51.492 -> Wednesday
17:21:51.492 -> 
17:21:51.492 -> -----------------
17:21:53.242 -> Ethereum: $1186.02
17:21:53.242 -> 24 hr change: +$3.50 (0.30%)
17:21:53.242 -> Price updated Wednesday, 7/6/2022 at 05:20 PM.
*/
 */
