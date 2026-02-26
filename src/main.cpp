/*
TFT
1 2 3 4 5 6 7 8

ESP32
1 2 3 4 5 6 7 8
USB
9 10 11 12 13 14 15 16

connection
TFT.          ESP32
1              2
2.             3
3              4
4.             10 
5              5
6              6
7              11
8              3
*/
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "esp_system.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <WiFiManager.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// ---------- TFT PINS ----------
//              GPIO
#define TFT_CS   7
#define TFT_DC   2
#define TFT_RST  3
#define TFT_MOSI 6   // SDA on TFT
#define TFT_SCK  4   // SCK on TFT

// ---------- WIFI ----------
//const char ssid[] = "Pnet-UK-mini-01";
//const char pass[] = "135721mh";
String formatted_date = "2000-01-01";
String formatted_time = "00:00:00";
String old_formatted_time = "00:00:00";

// ---------- NTP ----------
static const char ntpServerName[] = "uk.pool.ntp.org";
const int timeZone = 0;

WiFiUDP Udp;
unsigned int localPort = 8888;

// ---------- TFT OBJECT (hardware SPI) ----------
Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

GFXcanvas16 canvas(150, 40); // 1-bit, 120x30 pixels
GFXcanvas16 forexCanvas(160, 25);

// ---------- FOREX ----------
const char* finnhub_api_key = "d6eq0cpr01qvn4o0kcggd6eq0cpr01qvn4o0kch0";
WebSocketsClient webSocket;
double price_gbp_usd = 0.0;
double price_usd_sek = 0.0;
double last_gbp_sek = 0.0;

// ---------- FUNCTION DECLARATIONS ----------
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);


void drawWifiSignal(int16_t x, int16_t y, int8_t bars, uint16_t activeColor, uint16_t inactiveColor);
void updateWifiIndicator();


/**
 * @brief Draws a WiFi signal indicator with 4 vertical bars.
 * 
 * @param x Top-left x coordinate of the indicator's bounding box.
 * @param y Top-left y coordinate of the indicator's bounding box.
 * @param bars Number of active bars to show (0-4).
 * @param activeColor Color for the active bars (e.g., ST77XX_WHITE).
 * @param inactiveColor Color for the inactive bars (e.g., a dark gray).
 */

void drawWifiSignal(int16_t x, int16_t y, int8_t bars, uint16_t activeColor, uint16_t inactiveColor) {
    const int8_t bar_width = 2;
    const int8_t bar_spacing = 1;
    const int8_t max_bar_height = 8;

    // Bar 1 (left-most, shortest)
    int8_t h1 = max_bar_height / 4;
    display.fillRect(x, y + max_bar_height - h1, bar_width, h1, (bars >= 1) ? activeColor : inactiveColor);

    // Bar 2
    int8_t h2 = max_bar_height / 2;
    display.fillRect(x + bar_width + bar_spacing, y + max_bar_height - h2, bar_width, h2, (bars >= 2) ? activeColor : inactiveColor);

    // Bar 3
    int8_t h3 = max_bar_height * 3 / 4;
    display.fillRect(x + 2 * (bar_width + bar_spacing), y + max_bar_height - h3, bar_width, h3, (bars >= 3) ? activeColor : inactiveColor);

    // Bar 4 (right-most, tallest)
    int8_t h4 = max_bar_height;
    display.fillRect(x + 3 * (bar_width + bar_spacing), y + max_bar_height - h4, bar_width, h4, (bars >= 4) ? activeColor : inactiveColor);
}


/**
 * @brief Gets WiFi signal strength, converts it to a number of bars, and draws the indicator.
 *        Call this function repeatedly in your main loop().
 */

 
void updateWifiIndicator() {
    static unsigned long last_update = 0;
    if (millis() - last_update < 2000) return; // Update every 2 seconds
    last_update = millis();

    static int8_t last_bars = -1;
    int8_t bars = 0;

    if (WiFi.status() == WL_CONNECTED) {
        long rssi = WiFi.RSSI();
        if (rssi >= -55) bars = 4;
        else if (rssi >= -65) bars = 3;
        else if (rssi >= -75) bars = 2;
        else bars = 1;
    }

    if (bars != last_bars) {
        const int16_t indicator_width = 11; // 4 * bar_width + 3 * bar_spacing
        const int16_t indicator_height = 8; // max_bar_height
        int16_t x = display.width() - indicator_width - 2;
        int16_t y = 2;

        if (bars == 0) {
            display.fillRect(x, y, indicator_width, indicator_height, ST77XX_BLACK);
        } else {
            drawWifiSignal(x, y, bars, ST77XX_GREEN, 0x4208); // 0x4208 is a dark gray
        }
        last_bars = bars;
    }
}

void showPartialUpdate(){
 if (old_formatted_time == formatted_time) return;

  canvas.setFont(&FreeSansBold18pt7b); // Use custom font
  canvas.setTextWrap(false);   

  canvas.fillScreen(ST77XX_BLACK);
  canvas.fillRoundRect(0,0,150,40, 5, ST77XX_RED);
  
  // Get text bounds to center it in the canvas
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  canvas.getTextBounds(formatted_time, 0, 0, &tbx, &tby, &tbw, &tbh);
  
  // Center vertically within the canvas
  int16_t x = 4;  // Adjusted position
  int16_t y = (canvas.height() + tbh) / 2;
  
  canvas.setCursor(x, y);
  canvas.setTextColor(ST77XX_WHITE);
  canvas.print(formatted_time);
  
  // Center the canvas bitmap horizontally on the display
  int16_t canvas_x = (display.width() - canvas.width()) / 2;
  display.drawRGBBitmap(canvas_x, 80, canvas.getBuffer(), canvas.width(), canvas.height());

  old_formatted_time = formatted_time;
}

void showPartialUpdate_original()
{
  if (old_formatted_time == formatted_time)
  {return;}
  
  display.setFont(&FreeSansBold18pt7b);
  display.setTextSize(1);
  
    //display.fillRect(10, 60, 140, 40, ST77XX_RED);

    display.setCursor(10, 90);
    display.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
    display.print(old_formatted_time);
    
    display.setCursor(10, 90);
    display.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

    display.print(formatted_time);
    old_formatted_time = formatted_time;

}




void header_text(){
  display.fillRect(20, 15, 120, 45, ST77XX_BLACK);
  display.setFont(&FreeSansBold9pt7b);
  display.setTextSize(1);
  formatted_date = String(year()) + "-" + String(month()) + "-" + String(day());
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  
  // Center date horizontally
  display.getTextBounds(formatted_date, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t x_date = ((display.width() - tbw) / 2) - tbx;

  display.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  display.setCursor(x_date, 30);
  display.print(formatted_date);

  const char* weekday_names[] = {
      "Sunday",
      "Monday",
      "Tuesday",
      "Wednesday",
      "Thursday",
      "Friday",
      "Saturday"
    };

  int wday = weekday();
  String weekday_string = "";
  weekday_string = weekday_names[wday - 1];
  
  // Center weekday horizontally
  display.getTextBounds(weekday_string, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t x_weekday = ((display.width() - tbw) / 2) - tbx;
  
  display.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  display.setCursor(x_weekday, 50);
  display.print(weekday_string);


}

void updateForexDisplay() {
  if (price_gbp_usd > 0 && price_usd_sek > 0) {
    double rate = price_gbp_usd * price_usd_sek;
    if (abs(rate - last_gbp_sek) < 0.0001) return;
    last_gbp_sek = rate;

    forexCanvas.setFont(&FreeSansBold9pt7b);
    forexCanvas.setTextWrap(false);
    forexCanvas.fillScreen(ST77XX_BLACK);
    
    String text = "GBP/SEK: " + String(rate, 4);
    
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    forexCanvas.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
    
    int16_t x = 2; // Left-align with 10px padding
    int16_t y = (forexCanvas.height() + tbh) / 2;
    
    forexCanvas.setCursor(x, y);
    forexCanvas.setTextColor(ST77XX_CYAN);
    forexCanvas.print(text);

    int16_t draw_x = (display.width() - forexCanvas.width()) / 2;
    display.drawRGBBitmap(draw_x, 55, forexCanvas.getBuffer(), forexCanvas.width(), forexCanvas.height());
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected!");
      break;
    case WStype_CONNECTED:
      Serial.println("[WSc] Connected to Finnhub");
      webSocket.sendTXT("{\"type\":\"subscribe\",\"symbol\":\"OANDA:GBP_USD\"}");
      webSocket.sendTXT("{\"type\":\"subscribe\",\"symbol\":\"OANDA:USD_SEK\"}");
      break;
    case WStype_TEXT:
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        if (doc["type"] == "trade") {
          for (JsonObject trade : doc["data"].as<JsonArray>()) {
            const char* s = trade["s"];
            double p = trade["p"];
            if (strcmp(s, "OANDA:GBP_USD") == 0) price_gbp_usd = p;
            else if (strcmp(s, "OANDA:USD_SEK") == 0) price_usd_sek = p;
          }
          updateForexDisplay();
        }
      }
      break;
  }
}


// ================== NTP FUNCTIONS ==================

time_t getNtpTime() {
  IPAddress ntpServerIP;

  while (Udp.parsePacket() > 0) ;
  WiFi.hostByName(ntpServerName, ntpServerIP);
  sendNTPpacket(ntpServerIP);

  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= 48) {
      byte packetBuffer[48];
      Udp.read(packetBuffer, 48);

      unsigned long secsSince1900 =
        (unsigned long)packetBuffer[40] << 24 |
        (unsigned long)packetBuffer[41] << 16 |
        (unsigned long)packetBuffer[42] << 8 |
        (unsigned long)packetBuffer[43];

      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0;
}

void sendNTPpacket(IPAddress &address) {
  byte packetBuffer[48] = {0};
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;

  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, 48);
  Udp.endPacket();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // ---------- SPI INIT (MANDATORY ON ESP32-C3) ----------
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);

  // ---------- TFT INIT ----------
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(200);
  digitalWrite(TFT_RST, HIGH);
  delay(200);

  display.initR(INITR_BLACKTAB);   // try REDTAB if white
  display.setRotation(1);
  display.fillScreen(ST77XX_BLACK);
  display.setTextSize(1);
  display.setTextColor(ST77XX_GREEN);
  display.setCursor(0, 0);

  // ---------- WIFI ----------
  //display.print("Connecting to ");
  //display.println(ssid);
  //Serial.print("Connecting to ");
  //Serial.println(ssid);
  
  uint64_t mac = ESP.getEfuseMac();;

 
  // esp_efuse_mac_get_default((uint8_t*)&mac);

  String macString = String((uint8_t)(mac >> 40), HEX)  +
              String((uint8_t)(mac >> 32), HEX)  +
              String((uint8_t)(mac >> 24), HEX)  +
              String((uint8_t)(mac >> 16), HEX)  +
              String((uint8_t)(mac >> 8),  HEX)  +
              String((uint8_t)(mac),        HEX);
  macString.toUpperCase();

  String hostname = "clock_" + macString;
  display.setTextColor(ST77XX_GREEN);
  display.println("Connect to");
  display.println("  ");
  display.setTextColor(ST77XX_YELLOW);
  display.println(hostname);
  display.setTextColor(ST77XX_GREEN);
  display.println("  ");
  display.println("For setting!");
  hostname.replace(":", "");
  WiFi.setHostname(hostname.c_str());
  WiFiManager wm;
  delay(50);
  wm.resetSettings();
  wm.setHostname(hostname.c_str());
  bool res = wm.autoConnect(hostname.c_str());

  if (!res) {
    Serial.println("Failed to connect");
    ESP.restart();
  }

  /*
  WiFi.setHostname(hostname.c_str());
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.print(".");
  }
  */
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  display.fillScreen(ST77XX_BLACK);
  display.setCursor(0, 0);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.setFont(&FreeSansBold9pt7b);
  display.setTextSize(1);

  // ---------- NTP ----------
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  header_text();
  delay(1000);

  // ---------- WEBSOCKET ----------
  webSocket.beginSSL("ws.finnhub.io", 443, "/?token=" + String(finnhub_api_key));
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

}

void loop() {
  formatted_time = String(hour() < 10 ? "0" : "") + String(hour()) + ":" +
                   String(minute() < 10 ? "0" : "") + String(minute()) + ":" +
                   String(second() < 10 ? "0" : "") + String(second());
  showPartialUpdate();
  if (formatted_date != String(year()) + "-" + String(month()) + "-" + String(day())) {
    header_text();
    delay(1000);
  }
  webSocket.loop();
  updateWifiIndicator();
}