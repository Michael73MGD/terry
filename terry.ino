#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include "mfactoryfont.h"  // Custom font
#include <ArduinoJson.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 17

// These parameters require setting manually. Get your weather api key here: https://www.visualcrossing.com/sign-up/
char apiKey[64] = "";
char zipCode[16] = "";
// 

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 3600;
char currentTimeStr[24] = "";
char nextTimeStr[24] = "";
int lastMinute = -1;
float temperature = NAN;
bool rainLikely = false;

MD_Parola ledMatrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

enum State {
  SHOWING_TIME,
  SCROLL_OUT,
  SCROLL_IN
};

State state = SHOWING_TIME;

const unsigned long interval = 15 * 60 * 1000;  // 15 minute interval for updating temperature
unsigned long lastRequestTime = 0;

// Returns true if there is >50% chance of rain in the next `hours_ahead` hours, false otherwise
bool rain_likely(int hours_ahead = 12) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Not connected to WiFi");
    return false;
  }

  // Build rain endpoint using zipCode and apiKey
  String rain_endpoint = String("https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/") +
                         String(zipCode) +
                         String("/today?unitGroup=us&include=hours&key=") +
                         String(apiKey) +
                         String("&contentType=json");

  HTTPClient http;
  http.begin(rain_endpoint);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String input = http.getString();
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, input);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }

    JsonArray hours = doc["hours"];
    int count = 0;
    for (JsonObject hour : hours) {
      int precipprob = hour["precipprob"] | 0;
      if (precipprob > 50) {
        http.end();
        return true;
      }
      count++;
      if (count >= hours_ahead) break;
    }
    http.end();
    return false;
  } else {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
}

float get_current_temperature() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Not connected to WiFi");
    return NAN;
  }

  // Build endpoint string using zipCode and apiKey
  String temperature_endpoint = String("https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/") +
             String(zipCode) +
             String("/today?unitGroup=us&include=current&key=") +
             String(apiKey) +
             String("&contentType=json");
  
  HTTPClient http;
  http.begin(temperature_endpoint); 
  int httpCode = http.GET();

  if (httpCode > 0) {
    String input = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, input);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      http.end();
      return NAN;
    }

    JsonObject currentConditions = doc["currentConditions"];
    float temp = currentConditions["temp"]; // degrees F
    http.end();
    return temp;
  } else {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return NAN;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  ledMatrix.begin();
  ledMatrix.setIntensity(15);
  ledMatrix.setTextAlignment(PA_CENTER);
  
  // Show startup message
  ledMatrix.displayClear();

  // Welcome message. Comment out to save time when testing
  ledMatrix.displayScroll("Hi there! I'm Terry", PA_CENTER, PA_SCROLL_LEFT, 75);
  while (!ledMatrix.displayAnimate()) {
    // wait for scroll to finish
  }

  ledMatrix.displayScroll("Connect to the ESP32 WiFi to setup. ", PA_CENTER, PA_SCROLL_LEFT, 75);

  // Create an instance of WiFiManager
  WiFiManager wm;

  // TODO Need a way to save the API key and ZIP code to a file for persistence https://github.com/tzapu/WiFiManager/tree/master/examples/Parameters
  // WiFiManagerParameter apiKeyHelp("<a href='https://www.visualcrossing.com/sign-up/' target='_blank'>Get your Visual Crossing API key here</a><br>");
  // wm.addParameter(&apiKeyHelp);
  // Add custom parameters for weather API key and ZIP code
  // WiFiManagerParameter custom_api_key("api_key", "Weather API Key", "", 64);
  // WiFiManagerParameter custom_zip("zip_code", "ZIP Code", "", 16);
  // wm.addParameter(&custom_api_key);
  // wm.addParameter(&custom_zip);

  // set dark theme
  wm.setClass("invert");

  // Optionally reset saved settings (uncomment to test captive portal again)
  // wm.resetSettings(); 

  // Automatically connect to saved WiFi or start config portal
  if (!wm.autoConnect("ESP32-Setup")) {
    Serial.println("Failed to connect and hit timeout");
    ledMatrix.displayScroll("WiFi connection failed, try again. ", PA_CENTER, PA_SCROLL_LEFT, 75);
    while (!ledMatrix.displayAnimate()) {
      // wait for scroll to finish
    }
    delay(3000);
    ESP.restart();
  }

  // After wm.autoConnect("ESP32-Setup")
  // const char* savedApiKey = custom_api_key.getValue();
  // const char* savedZipCode = custom_zip.getValue();

  // Test API call 
  float testTemp = get_current_temperature();
  if (isnan(testTemp)) {
    Serial.println("Weather API call failed. Please check your API key and ZIP code and try again.");
    ledMatrix.displayScroll("Weather API failed! Check key/ZIP.", PA_CENTER, PA_SCROLL_LEFT, 75);
    while (!ledMatrix.displayAnimate()) {
      // wait for scroll to finish
    }
    delay(3000);
    ESP.restart();
  }

  // If connected:
  Serial.println("Connected to WiFi!");
  ledMatrix.displayScroll("Connections successful!", PA_CENTER, PA_SCROLL_LEFT, 75);
  while (!ledMatrix.displayAnimate()) {
    // wait for scroll to finish
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Set font for time
  ledMatrix.setFont(mFactory);
  
  // Make first temperature and rain request
  temperature = testTemp;
  Serial.printf("\nCurrent temperature: %.1f°F\n", temperature);
  rainLikely = rain_likely();
  Serial.printf("\nRain likely: %s\n", rainLikely ? "Yes" : "No");

  lastRequestTime = 0;
}




void loop() {
  if (millis() - lastRequestTime >= interval) {
    // If it's been longer than interval, grab the current temperature
    temperature = get_current_temperature();
    Serial.printf("Updating temperature: %.1f°F\n", temperature);
    // Check if rain is likely
    rainLikely = rain_likely();
    Serial.printf("Rain likely: %s\n", rainLikely ? "Yes" : "No");
    lastRequestTime = millis();
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  int currentMinute = timeinfo.tm_min;

  switch (state) {
    case SHOWING_TIME:
      if (currentMinute != lastMinute) {
        // Time changed → scroll old time out
        strcpy(nextTimeStr, currentTimeStr); // Save old time
        lastMinute = currentMinute;
        state = SCROLL_OUT;

        ledMatrix.setTextAlignment(PA_LEFT);
        ledMatrix.displayText(currentTimeStr, PA_LEFT, 100, 0, PA_NO_EFFECT, PA_SCROLL_LEFT);
        ledMatrix.displayAnimate();
      }
      break;

    case SCROLL_OUT:
      if (ledMatrix.displayAnimate()) {
        // Old time has scrolled out → scroll new time in
        char timeBuf[10];
        strftime(timeBuf, sizeof(timeBuf), "%I:%M", &timeinfo);

        if (rainLikely) {
            // Show umbrella, no degrees symbol
            snprintf(currentTimeStr, sizeof(currentTimeStr), "%s{%0.f", timeBuf, temperature);
        } else {
            // Show degrees symbol
            snprintf(currentTimeStr, sizeof(currentTimeStr), "%s  %.0f|", timeBuf, temperature);
        }

        state = SCROLL_IN;
        ledMatrix.setTextAlignment(PA_LEFT);
        ledMatrix.displayText(currentTimeStr, PA_LEFT, 100, 0, PA_SCROLL_LEFT, PA_NO_EFFECT);
        ledMatrix.displayAnimate();
      }
      break;

    case SCROLL_IN:
      if (ledMatrix.displayAnimate()) {
        state = SHOWING_TIME;
      }
      break;
  }
}
