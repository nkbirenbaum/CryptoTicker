// cryptoTracker tracks price movement of selected cryptocurrency
// v0.1 uses hard-coded WiFi credentials & does not display time
// v0.2 displays time and implements splash screen
// v0.3 implements coin class, bitmaps class, and snprintf()
// v0.4 changes DIN_PIN & adds ability to connect to ESP32 to change ssid/password
// v0.5 adds SPIFFS ssid/password storage and corrects coin price change error
// v0.6 adds pushbuttons for resetting device and for entering setup mode
// v0.7 adds ability to exit setup mode by pressing button again & reorganizes borders/screens
// v1.0 has minor changes and removes some debugging code
// Created by Nathan Birenbaum 02/13/2022
// Last modified by Nathan Birenbaum 11/26/2022

// To do:
// Fix wifi reconnection after updating settings
// Fix price last updated time when using DST
// Remove new connection flag?
// Add ability to change coin
// Shade missing Wifi status pieces
// Add server ping if price is not retrieved correctly
// Display available networks: https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/ & https://randomnerdtutorials.com/esp32-wifimulti/
// Improve security: remove "client.setInsecure();" and use long-lasting certificate
// Encrypt flash storage of ssid/password: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#nvs-encryption
// Improve w/ events (tutorial): https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
// Improve w/ events (video): https://www.youtube.com/watch?v=qwmLCWhfn9s

// Libraries
#include <WiFi.h>              // WiFi functionality for ESP32
#include <WiFiClientSecure.h>  // Secure client needed to connect to coingecko.com API
#include <HTTPClient.h>        // Secure client needed to connect to coingecko.com API
#include <ArduinoJson.h>       // Get json doc from coingecko.com API
#include <TimeLib.h>           // Convert last updated date/time from UTC to readable GMT
#include <SPIFFS.h>            // Save ssid, password, time zone, & dst info in EEPROM
#include <AsyncTCP.h>          // Client for entering setup mode after settings pushbutton press
#include <ESPAsyncWebServer.h> // Client for entering setup mode after settings pushbutton press
#include "Ucglib.h"            // OLED screen graphics
#include "coin.h"              // Coin class contains coin price data & strings
#include "bitmaps.h"           // Bitmaps class contains WiFi symbol bitmap

// True required for initial upload (false can be used later to save time)
#define SPIFFS_MODE true

// ESP32 pin definitions for settings button & OLED screen
#define SETTINGS_BUTTON_PIN 32
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
bool newConnectionFlag = false;

// Global server variables for updating WiFi credentials
AsyncWebServer server(80);
volatile bool submitButtonPressedFlag = false;
const char* serverSsid = "Crypto Tracker Setup";
const char* serverPassword = "123456789";
const char* PARAM_1 = "submittedSsid";
const char* PARAM_2 = "submittedPassword";
const char* PARAM_3 = "submittedTimeZone";
const char* PARAM_4 = "submittedDstStr";

// Global Coin variables
char urlPrice[] = "https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=usd&include_market_cap=true&include_24hr_vol=true&include_24hr_change=true&include_last_updated_at=true";
char urlPing[] = "https://api.coingecko.com/api/v3/ping";
char coinName[] = "ethereum";
Coin coin(urlPrice, urlPing, coinName);

// Global ntpServer and time zone variables
const char* ntpServer = "pool.ntp.org";
String timeZoneStr = "";
String dstStr = "";
long gmtOffsetSec = 0;
long dstOffsetSec = 0;
tmElements_t lastUpdatedDateTime;

// HTML webpage served by ESP32 access point when settings button is pressed
volatile bool settingsButtonPressedFlag = false;
bool settingsButtonPressedThisLoopFlag = false;
const char webpageConfirmation[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
  <head>
    <title>ESP32 WiFi Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
  <body><h1>ESP32 WiFi Setup</h1><br><br>
    HTTP GET request sent to ESP32.<br><br>
    <a href=\"/\">Back</a>
  </body>
</html>)rawliteral";

//
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
  <head>
    <title>ESP32 WiFi Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
  <body>
    <h1>ESP32 WiFi Setup</h1>
    <form action="/get" id="settingsForm">
      <label for="ssidText">SSID:</label>
      <input type="text" name="submittedSsid" id="ssidText"><br>
      <label for="passwordPassword">Password:</label>
      <input type="password" name="submittedPassword" id="passwordPassword"><br>
      <label for="timeZoneSelect">Time zone:</label>
      <select name="submittedTimeZone" id="timeZoneSelect">
        <option value="no selection">--Please select a time zone--</option>
        <option value="-12:00">(GMT -12:00) Eniwetok, Kwajalein</option>
        <option value="-11:00">(GMT -11:00) Midway Island, Samoa</option>
        <option value="-10:00">(GMT -10:00) Hawaii</option>
        <option value="-09:50">(GMT -9:30) Taiohae</option>
        <option value="-09:00">(GMT -9:00) Alaska</option>
        <option value="-08:00">(GMT -8:00) Pacific Time (US &amp; Canada)</option>
        <option value="-07:00">(GMT -7:00) Mountain Time (US &amp; Canada)</option>
        <option value="-06:00">(GMT -6:00) Central Time (US &amp; Canada), Mexico City</option>
        <option value="-05:00">(GMT -5:00) Eastern Time (US &amp; Canada), Bogota, Lima</option>
        <option value="-04:50">(GMT -4:30) Caracas</option>
        <option value="-04:00">(GMT -4:00) Atlantic Time (Canada), Caracas, La Paz</option>
        <option value="-03:50">(GMT -3:30) Newfoundland</option>
        <option value="-03:00">(GMT -3:00) Brazil, Buenos Aires, Georgetown</option>
        <option value="-02:00">(GMT -2:00) Mid-Atlantic</option>
        <option value="-01:00">(GMT -1:00) Azores, Cape Verde Islands</option>
        <option value="+00:00">(GMT) Western Europe Time, London, Lisbon, Casablanca</option>
        <option value="+01:00">(GMT +1:00) Brussels, Copenhagen, Madrid, Paris</option>
        <option value="+02:00">(GMT +2:00) Kaliningrad, South Africa</option>
        <option value="+03:00">(GMT +3:00) Baghdad, Riyadh, Moscow, St. Petersburg</option>
        <option value="+03:50">(GMT +3:30) Tehran</option>
        <option value="+04:00">(GMT +4:00) Abu Dhabi, Muscat, Baku, Tbilisi</option>
        <option value="+04:50">(GMT +4:30) Kabul</option>
        <option value="+05:00">(GMT +5:00) Ekaterinburg, Islamabad, Karachi, Tashkent</option>
        <option value="+05:50">(GMT +5:30) Bombay, Calcutta, Madras, New Delhi</option>
        <option value="+05:75">(GMT +5:45) Kathmandu, Pokhara</option>
        <option value="+06:00">(GMT +6:00) Almaty, Dhaka, Colombo</option>
        <option value="+06:50">(GMT +6:30) Yangon, Mandalay</option>
        <option value="+07:00">(GMT +7:00) Bangkok, Hanoi, Jakarta</option>
        <option value="+08:00">(GMT +8:00) Beijing, Perth, Singapore, Hong Kong</option>
        <option value="+08:75">(GMT +8:45) Eucla</option>
        <option value="+09:00">(GMT +9:00) Tokyo, Seoul, Osaka, Sapporo, Yakutsk</option>
        <option value="+09:50">(GMT +9:30) Adelaide, Darwin</option>
        <option value="+10:00">(GMT +10:00) Eastern Australia, Guam, Vladivostok</option>
        <option value="+10:50">(GMT +10:30) Lord Howe Island</option>
        <option value="+11:00">(GMT +11:00) Magadan, Solomon Islands, New Caledonia</option>
        <option value="+11:50">(GMT +11:30) Norfolk Island</option>
        <option value="+12:00">(GMT +12:00) Auckland, Wellington, Fiji, Kamchatka</option>
        <option value="+12:75">(GMT +12:45) Chatham Islands</option>
        <option value="+13:00">(GMT +13:00) Apia, Nukualofa</option>
        <option value="+14:00">(GMT +14:00) Line Islands, Tokelau</option>
      </select><br>
      <label for="dstCheckbox">DST?</label>
      <input type="checkbox" name="submittedDstStr" id="dstCheckbox"><br><br>
      <input type="submit" value="Submit"><br><br>
    </form><br>
  </body>
</html>)rawliteral";


// Initialize serial monitor, WiFi connection, & initial price fetch
void setup() {

  // Setup serial monitor for error messages
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Serial monitor initialized.");

  // Setup SPIFFS
  if (!SPIFFS.begin(SPIFFS_MODE)) {
    Serial.println("Error mounting SPIFFS.");
    return;
  }

  // Sets internal pull-up & interrupt for settings button
  pinMode(SETTINGS_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(SETTINGS_BUTTON_PIN, settingsButtonPressed, FALLING);

  // Initialize OLED screen, background color, & display splash screen
  ucg.begin(UCG_FONT_MODE_SOLID);
  displaySplashScreen();

  // Set host name & connect to WiFi
  WiFi.setHostname(deviceName.c_str());
  connectWiFi();
  drawWifi((int8_t)WiFi.RSSI());

  // Initialize ntpServer and update the time
  updateGmtOffset();
  updateDstOffset();
  configTime(gmtOffsetSec, dstOffsetSec, ntpServer);
  updateClock();

  // Initial price fetch
  fetchPrice();
  updatePrice();

  // Draw border lines
  drawBorderLines("price");
  Serial.println("Setup complete.");

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

  // Serve settings webpage when settings button is pressed
  if (settingsButtonPressedFlag) {
    serveSettingsWebpage();
  }

  // Periodically draw updated WiFi connection strength
  currentMs = millis();
  if ( currentMs - priorCxnCheckMs >= cxnCheckIntervalMs ) {
    drawWifi((int8_t)WiFi.RSSI());
    priorCxnCheckMs = currentMs;
  }

  // Periodically update clock
  currentMs = millis();
  if ( currentMs - priorUpdateTimeMs >= priorUpdateTimeIntervalMs ) {
    updateClock();
    priorUpdateTimeMs = currentMs;
  }

  // Periodically fetch & update price
  currentMs = millis();
  if ( currentMs - priorPriceFetchMs >= priceFetchIntervalMs ) {
    fetchPrice();
    updatePrice();
    priorPriceFetchMs = currentMs;
  }

  // Draw price borders if settings button was pressed
  if ( settingsButtonPressedThisLoopFlag ) {
    drawBorderLines("price");
  }

  // Reset flags
  settingsButtonPressedThisLoopFlag = false;

}


// Displays splash screen
void displaySplashScreen() {

  // Prepare screen
  ucg.clearScreen();
  ucg.setColor(255, 255, 255);
  ucg.setFont(ucg_font_ncenR12_hr);

  // Line 1
  ucg.setPrintPos(6, 15);
  ucg.print("Cryptocurrency");

  // Line 2
  ucg.setPrintPos(18, 35);
  ucg.print("tracker v1.0");

  // Line 3
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setPrintPos(8, 53);
  ucg.print("By Nathan Birenbaum");

  // Draw border lines
  drawBorderLines("splash");

  // Line 4
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(0, 74);
  ucg.print("Source code:");

  // Line 5
  ucg.setPrintPos(0, 89);
  ucg.print("github.com/nkbirenbaum");

  // Line 6
  ucg.setPrintPos(0, 104);
  ucg.print("/CryptoTracker");

  // Line 7
  ucg.setPrintPos(8, 125);
  ucg.print("Powered by CoinGecko");
  delay(3000);

}


// Connects to WiFi (status messages displayed over splash screen)
void connectWiFi() {

  // Prepare screen
  ucg.clearScreen();
  ucg.setColor(255, 255, 255);

  // Read ssid & password from SPIFFS
  ssid = readFile(SPIFFS, "/ssid.txt");
  password = readFile(SPIFFS, "/password.txt");

  // Display title & borders
  ucg.setFont(ucg_font_ncenR12_hr);
  ucg.setPrintPos(0, 20);
  ucg.print("WiFi Connection");
  drawBorderLines("wifi");

  // Display connection status on OLED screen
  WiFi.mode(WIFI_STA);

  // Create new strings for displaying ssid & password
  char ssidStr[128];
  snprintf(ssidStr, 128, "WiFi: %s", ssid.c_str());
  char passwordStr[128];
  String passwordCopy = password;
  obscurePassword(passwordCopy);
  snprintf(passwordStr, 128, "Password: %s", passwordCopy.c_str());

  // Display ssid & obscured password
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(0, 62);
  ucg.print(ssidStr);
  ucg.setPrintPos(0, 78);
  ucg.print(passwordStr);

  // Display connection status
  ucg.setPrintPos(0, 106);
  ucg.print("Connecting to network...");
  ucg.setPrintPos(0, 122);
  String connectionStatusStr = ".";
  ucg.print(connectionStatusStr);

  // Attempt to begin WiFi connection or serveSettingsWebpage() to change credentials
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
      serveSettingsWebpage();
      return;
    }
  }
  client.setInsecure();

  // Display connected message then clear screen
  connectionStatusStr += "connected!";
  ucg.print(connectionStatusStr);
  delay(3000);
  ucg.clearScreen();
  newConnectionFlag = true;

}


// Replaces password with asteriks
void obscurePassword(String& phrase) {

  for (int ii = 0; ii < phrase.length(); ii++) {
    if (isalpha(phrase[ii])) {
      phrase[ii] = '*';
    }
  }

}


// Set settingsButtonPressedFlag to call serveSettingsWebpage() function in main loop
void settingsButtonPressed() {

  settingsButtonPressedFlag = true;

}


// Starts ESP32 as access point to serve settings webpage for user to input ssid, password, and time zone
void serveSettingsWebpage() {

  // Reset flag (may be set again to exit settings)
  settingsButtonPressedFlag = false;

  // Start ESP32 server and display IP address
  Serial.println("Starting access point for settings input.");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(serverSsid, serverPassword);
  IPAddress serverIp = WiFi.softAPIP();
  submitButtonPressedFlag = false;
  Serial.print("ESP32 server IP address: ");
  Serial.println(serverIp);

  // Display info for reconnecting to WiFi
  ucg.clearScreen();
  ucg.setFont(ucg_font_ncenR14_hr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(16, 20);
  ucg.print("Setup mode");
  drawBorderLines("setup");
  ucg.setFont(ucg_font_ncenR08_hr);
  ucg.setPrintPos(0, 46);
  ucg.print("Please connect to network:");
  ucg.setPrintPos(0, 62);
  ucg.print(serverSsid);
  ucg.setPrintPos(0, 78);
  ucg.print("Password:");
  ucg.setPrintPos(0, 94);
  ucg.print(serverPassword);
  ucg.setPrintPos(0, 110);
  ucg.print("And go to web address: ");
  ucg.setPrintPos(0, 126);
  ucg.print(serverIp);

  // Send webpage with input fields when client connects
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", webpage);
    Serial.println("Client connected to server.");
  });

  // Send GET request to IP to return PARAM_1 to PARAM_4 values
  // This block executes only when submit button is pressed
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    if (request->hasParam(PARAM_1)) {
      Serial.println("Param 1 submitted.");
      String tempSsid = request->getParam(PARAM_1)->value();
      if (tempSsid != "") {
        ssid = tempSsid;
        writeFile(SPIFFS, "/ssid.txt", ssid.c_str());
      }
    }
    if (request->hasParam(PARAM_2)) {
      Serial.println("Param 2 submitted.");
      String tempPassword = request->getParam(PARAM_2)->value();
      if (tempPassword != "") {
        password = tempPassword;
        writeFile(SPIFFS, "/password.txt", password.c_str());
      }
    }
    if (request->hasParam(PARAM_3)) {
      Serial.println("Param 3 submitted.");
      String tempTimeZoneStr = request->getParam(PARAM_3)->value();
      if (tempTimeZoneStr != "no selection") {
        timeZoneStr = tempTimeZoneStr;
        writeFile(SPIFFS, "/timeZoneStr.txt", timeZoneStr.c_str());

      }
    }
    if (request->hasParam(PARAM_4)) {
      Serial.println("Param 4 submitted.");
      dstStr = "on";
    } else {
      dstStr = "off";
    }
    writeFile(SPIFFS, "/dstStr.txt", dstStr.c_str());
    updateGmtOffset();
    updateDstOffset();
    configTime(gmtOffsetSec, dstOffsetSec, ntpServer);
    request->send(200, "text/html", webpageConfirmation);
    submitButtonPressedFlag = true;
    delay(500);
    Serial.println("End of HTTP get request. Restarting...");
    ESP.restart();
  });
  server.onNotFound(notFound);

  // Start server and wait. ESP.restart() called when response submitted or settings button pressed again.
  server.begin();
  while ( true ) {
    if ( settingsButtonPressedFlag ) {
      ESP.restart();
    }
  }

}


// ESP32 WiFi Setup webpage not found
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Webpage not found.");
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
  coin.oldPrice = coin.price / ( (coin.percentChange / 100) + 1);
  coin.priceChange = coin.price - coin.oldPrice;
  coin.flagPositiveChange = (coin.percentChange > 0.0) ? true : false;
  String plusMinus = (coin.percentChange > 0.0) ? "+" : "-";

  // Create price-related strings
  snprintf(coin.priceBuffer1, Coin::bufferSize, "ETH: $%0.2f          ", coin.price);
  snprintf(coin.priceBuffer2, Coin::bufferSize, "24h: ");
  snprintf(coin.priceBuffer3, Coin::bufferSize, "%s$%0.2f             ", plusMinus, abs(coin.priceChange));
  snprintf(coin.priceBuffer4, Coin::bufferSize, "%s%0.2f%%            ", plusMinus, abs(coin.percentChange));

  // Get update time data from json doc & create update time-related strings
  time_t utcTimestamp = doc["ethereum"]["last_updated_at"];
  utcTimestamp = utcTimestamp + gmtOffsetSec + dstOffsetSec;
  breakTime(utcTimestamp, lastUpdatedDateTime);
  int monthInt = (int)lastUpdatedDateTime.Month;
  int dayInt = (int)lastUpdatedDateTime.Day;
  int yearInt = (int)lastUpdatedDateTime.Year + 1970;
  int hourInt = hourMilitaryToRegular((int)lastUpdatedDateTime.Hour);
  int minuteInt = (int)lastUpdatedDateTime.Minute;
  String ampm = amOrPm((int)lastUpdatedDateTime.Hour);
  snprintf(coin.timeBuffer1, Coin::bufferSize, "Price updated:");
  snprintf(coin.timeBuffer2, Coin::bufferSize, "%d/%d/%d at %02d:%02d %s          ", monthInt, dayInt, yearInt, hourInt, minuteInt, ampm);

}


// Updates displayed price information
void updatePrice() {

  drawWifi((int8_t)WiFi.RSSI());
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


// Updates time zone offset for use with NTP server
void updateGmtOffset() {

  timeZoneStr = readFile(SPIFFS, "/timeZoneStr.txt");
  if (timeZoneStr == "") {
    Serial.println("Failed to update GMT offset. 'timeZoneStr.txt' is empty.");
    return;
  }
  String plusMinus = timeZoneStr.substring(0, 1);
  String hoursStr = timeZoneStr.substring(1, 3);
  long hours = hoursStr.toInt();
  String percentHoursStr = timeZoneStr.substring(4);
  long percentHours = percentHoursStr.toInt();
  if (plusMinus == "-") {
    gmtOffsetSec = -3600 * hours - 3600 * percentHours / 100;
  } else {
    gmtOffsetSec = 3600 * hours + 3600 * percentHours / 100;
  }

}


// Updates daylight savings time offset for use with NTP server
void updateDstOffset() {

  dstStr = readFile(SPIFFS, "/dstStr.txt");
  if (dstStr == "") {
    Serial.println("Failed to update DST offset. 'dstStr.txt' is empty.");
    return;
  }
  if (dstStr == "on") {
    dstOffsetSec = 3600;
  } else {
    dstOffsetSec = 0;
  }

}


// Updates clock on OLED screen
void updateClock() {

  // Get timeinfo structure from ntpServer
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time using getLocalTime().");
    return;
  }

  // Compute new time string
  char hourStr[4];
  strftime(hourStr, 4, "%H", &timeinfo);
  String ampm = amOrPm(atoi(hourStr));
  char hour12Str[4];
  strftime(hour12Str, 4, "%I", &timeinfo);
  char hour12StrPlus1;
  if ( gmtOffsetSec == 3600) {
    if ( hour12Str == "0" || hour12Str == "00" ) {
      hour12StrPlus1 = "01";
    } else if (hour12Str == "01") {
      hour12StrPlus1 = "02";
    } else if (hour12Str == "02") {
      hour12StrPlus1 = "03";
    } else if (hour12Str == "03") {
      hour12StrPlus1 = "04";
    } else if (hour12Str == "04") {
      hour12StrPlus1 = "05";
    } else if (hour12Str == "05") {
      hour12StrPlus1 = "06";
    } else if (hour12Str == "06") {
      hour12StrPlus1 = "07";
    } else if (hour12Str == "07") {
      hour12StrPlus1 = "08";
    } else if (hour12Str == "08") {
      hour12StrPlus1 = "09";
    } else if (hour12Str == "09") {
      hour12StrPlus1 = "10";
    } else if (hour12Str == "10") {
      hour12StrPlus1 = "11";
    } else if (hour12Str == "11") {
      hour12StrPlus1 = "12";
    } else if (hour12Str == "12") {
      hour12StrPlus1 = "00";
    }
  }
  char minStr[4];
  strftime(minStr, 4, "%M", &timeinfo);
  char currentTime[16];
  if ( gmtOffsetSec == 3600) {
    snprintf(currentTime, 16, "%s:%s %s", hour12StrPlus1, minStr, ampm);
  } else {
    snprintf(currentTime, 16, "%s:%s %s", hour12Str, minStr, ampm);
  }

  // Display new time
  ucg.setFont(ucg_font_ncenR14_hr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(0, 20);
  ucg.print(currentTime);

}


// Draws horizontal borders above & below price seciton of OLED screen
void drawBorderLines(String screen) {

  ucg.setColor(100, 100, 100);
  if (screen == "splash") {
    ucg.drawHLine(1, 60, 126);
    ucg.drawHLine(1, 110, 126);
  } else if (screen == "wifi") {
    ucg.drawHLine(1, 29, 126);
    ucg.drawHLine(1, 88, 126);
  } else if (screen == "price") {
    ucg.drawHLine(1, 29, 126);
    ucg.drawHLine(1, 98, 126);
  } else if (screen == "setup") {
    ucg.drawHLine(1, 29, 126);
  }
  ucg.setColor(255, 255, 255);

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
String readFile(fs::FS &fs, const char *path) {

  // Open file for reading
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("Error reading file: file is empty or failed to open.");
    return "ERROR_READING_SPIFFS";
  }

  // Get contents
  String contents = "";
  while (file.available()) {
    contents += String((char)file.read());
  }
  file.close();
  return contents;

}


// Writes file to SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message) {

  // Open file for writing
  File file = fs.open(path, "w");
  if (!file) {
    Serial.println("Error opening file for writing.");
    return;
  }

  // Write contents
  if (file.print(message)) {
    Serial.println("File written.");
  } else {
    Serial.println("File write failed.");
  }
  file.close();

}
