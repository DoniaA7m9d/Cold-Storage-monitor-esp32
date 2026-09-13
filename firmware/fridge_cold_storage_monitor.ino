/*
  Fridge / Cold Storage Monitor - V1
  ESP32 + DHT11 + Buzzer + Telegram + Google Sheets logging

  - No deep sleep: the fridge circuit is always powered, and this device
    needs to react fast, so it stays awake and polls continuously.
  - Confirmation logic: requires several consecutive high-temp readings
    before firing an alarm, to avoid false alarms from sensor noise.
  - Telegram alert is edge-triggered (sent once when the alarm STARTS,
    and once when it CLEARS) instead of spamming every loop.
  - Local buzzer fires immediately regardless of WiFi/Telegram status.

  Libraries needed (Library Manager):
    - DHT sensor library (Adafruit)
    - Adafruit Unified Sensor
    - UniversalTelegramBot (Brian Lough)
    - ArduinoJson (v6.x — v7 has breaking changes with UniversalTelegramBot)
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <UniversalTelegramBot.h>

//  CONFIG 
#define DHTPIN        4
#define DHTTYPE       DHT11
#define BUZZER_PIN    5

const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* TELEGRAM_BOT_TOKEN = "YOUR_BOT_TOKEN";
const char* TELEGRAM_CHAT_ID   = "YOUR_CHAT_ID";

// Google Apps Script Web App URL (deployed as "Anyone" access)
const char* GOOGLE_SCRIPT_URL = "YOUR_GOOGLE_APPS_SCRIPT_URL";

const float TEMP_THRESHOLD_C   = 8.0;   // alarm if temp exceeds this
const int   CONFIRM_READINGS   = 3;     // consecutive high readings needed
const unsigned long READ_INTERVAL_MS   = 10UL * 1000;   // read every 10s
const unsigned long LOG_INTERVAL_MS    = 5UL * 60 * 1000; // log every 5 min
const unsigned long WIFI_RETRY_MS      = 15UL * 1000;

//  GLOBALS 
DHT dht(DHTPIN, DHTTYPE);
WiFiClientSecure secureClient;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, secureClient);

unsigned long lastReadTime  = 0;
unsigned long lastLogTime   = 0;
unsigned long lastWifiRetry = 0;

int   highTempStreak = 0;
bool  alarmActive     = false;

//  SETUP 
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();
  connectWiFi();

  secureClient.setInsecure(); // for Telegram TLS (no cert store needed)
}

//  LOOP 
void loop() {
  unsigned long now = millis();

  // Keep WiFi alive without blocking the loop
  if (WiFi.status() != WL_CONNECTED && now - lastWifiRetry > WIFI_RETRY_MS) {
    lastWifiRetry = now;
    connectWiFi();
  }

  if (now - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = now;
    handleReading();
  }

  if (now - lastLogTime >= LOG_INTERVAL_MS) {
    lastLogTime = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      logToGoogleSheets(t, h, alarmActive);
    }
  }
}

//  CORE LOGIC 
void handleReading() {
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Sensor read failed, skipping this cycle.");
    return;
  }

  Serial.printf("Temp: %.1f C | Humidity: %.1f%%\n", temp, hum);

  if (temp > TEMP_THRESHOLD_C) {
    highTempStreak++;
  } else {
    highTempStreak = 0;
    if (alarmActive) {
      // Temp is back to normal -> clear alarm
      alarmActive = false;
      digitalWrite(BUZZER_PIN, LOW);
      sendTelegramMessage("✅ رجعت الحرارة لطبيعتها في الثلاجة (" +
                           String(temp, 1) + " °C). الإنذار اتلغى.");
    }
  }

  // Trigger alarm only after N consecutive confirmed high readings
  if (highTempStreak >= CONFIRM_READINGS && !alarmActive) {
    alarmActive = true;
    digitalWrite(BUZZER_PIN, HIGH); // local buzzer fires immediately
    sendTelegramMessage("🚨 تحذير! الحرارة في الثلاجة وصلت " +
                         String(temp, 1) + " °C (الحد الأقصى " +
                         String(TEMP_THRESHOLD_C, 1) +
                         " °C). الرطوبة: " + String(hum, 1) + "%.");
  }
}

//  WIFI 
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWiFi connected." : "\nWiFi connect failed, will retry.");
}

//  TELEGRAM 
void sendTelegramMessage(const String &message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi, cannot send Telegram message. Buzzer still active locally.");
    return;
  }
  bool sent = bot.sendMessage(TELEGRAM_CHAT_ID, message, "");
  Serial.println(sent ? "Telegram message sent." : "Telegram message failed.");
}

// GOOGLE SHEETS LOGGING 
void logToGoogleSheets(float temp, float hum, bool alarm) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(GOOGLE_SCRIPT_URL) +
               "?temp=" + String(temp, 1) +
               "&hum=" + String(hum, 1) +
               "&alarm=" + String(alarm ? "1" : "0");

  http.begin(url);
  int httpCode = http.GET();
  Serial.printf("Google Sheets log HTTP code: %d\n", httpCode);
  http.end();
}
