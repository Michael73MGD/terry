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

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 3600;

MD_Parola ledMatrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

char currentTimeStr[24] = "";
char nextTimeStr[24] = "";
int lastMinute = -1;
float temperature = NAN;

enum State {
  SHOWING_TIME,
  SCROLL_OUT,
  SCROLL_IN
};

State state = SHOWING_TIME;

// const char* location = "Somerville";
// const char* endpoint = "https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/boston/next24hours?unitGroup=us&elements=datetime%2Ctempmax%2Ctempmin%2Ctemp%2Cfeelslikemax%2Cfeelslikemin%2Cfeelslike%2Chumidity%2Cprecip%2Cpreciptype%2Cwindspeedmean%2Cmoonphase%2Cconditions%2Cdescription%2Cicon&key=2WKEX8NVUHXNVV7SYHEX3EQBL&contentType=json";
// const char* endpoint = "https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/boston/today?unitGroup=us&elements=datetime%2Ctempmax%2Ctempmin%2Ctemp%2Cfeelslikemax%2Cfeelslikemin%2Cfeelslike%2Chumidity%2Cprecip%2Cpreciptype%2Cwindspeedmean%2Cmoonphase%2Cconditions%2Cdescription%2Cicon&include=days&key=2WKEX8NVUHXNVV7SYHEX3EQBL&contentType=json";

const char* endpoint = "https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/boston/today?unitGroup=us&include=current&key=2WKEX8NVUHXNVV7SYHEX3EQBL&contentType=json";
const unsigned long interval = 15 * 60 * 1000;  // 15 minute interval for updating temperature
unsigned long lastRequestTime = 0;

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

  // Create an instance of WiFiManager
  WiFiManager wm;

  // Optionally reset saved settings (uncomment to test captive portal again)
  // wm.resetSettings();

  // Automatically connect to saved WiFi or start config portal
  if (!wm.autoConnect("ESP32-Setup")) {
    Serial.println("⚠️ Failed to connect and hit timeout");
    ledMatrix.displayScroll("Connect to ESP32 WiFi from your phone to setup. ", PA_CENTER, PA_SCROLL_LEFT, 75);
    while (!ledMatrix.displayAnimate()) {
      // wait for scroll to finish
    }
    delay(3000);
    ESP.restart();
  }

  // If connected:
  Serial.println("✅ Connected to WiFi!");
  ledMatrix.displayScroll("WiFi Connected!", PA_CENTER, PA_SCROLL_LEFT, 75);
  while (!ledMatrix.displayAnimate()) {
    // wait for scroll to finish
  }
  Serial.print("🔗 IP Address: ");
  Serial.println(WiFi.localIP());
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Set font for time
  ledMatrix.setFont(mFactory);
  
  // Make first request immediately
  temperature = get_current_temperature();
  Serial.printf("Current temperature: %.1f°F\n", temperature);
  // makeApiRequest();
  lastRequestTime = 0;
}




void loop() {
  if (millis() - lastRequestTime >= interval) {
    // If it's been longer than interval, grab the current temperature
    temperature = get_current_temperature();
    Serial.printf("Updating temperature: %.1f°F\n", temperature);
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

        // Use the temperature fetched at the start of loop() every interval
        snprintf(currentTimeStr, sizeof(currentTimeStr), "%s %.0f", timeBuf, temperature); // use ° at your own risk

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

float get_current_temperature() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Not connected to WiFi");
    return NAN;
  }

  HTTPClient http;
  http.begin(endpoint);
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
    Serial.printf("❌ HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return NAN;
  }
}

void makeApiRequest() {
  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(endpoint);

    int httpCode = http.GET(); // Send the request

    if (httpCode > 0) {
      Serial.printf("✅ HTTP %d\n", httpCode);
      String input = http.getString();
      Serial.println("📥 Response:");
      Serial.println(input);

      // Use https://arduinojson.org/v6/assistant for this section
      // Stream& input;

      // Stream& input;

      DynamicJsonDocument doc(4096);

      DeserializationError error = deserializeJson(doc, input);

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }

      int queryCost = doc["queryCost"]; // 1
      float latitude = doc["latitude"]; // 42.3587
      float longitude = doc["longitude"]; // -71.0567
      const char* resolvedAddress = doc["resolvedAddress"]; // "Boston, MA, United States"
      const char* address = doc["address"]; // "boston"
      const char* timezone = doc["timezone"]; // "America/New_York"
      int tzoffset = doc["tzoffset"]; // -4

      /* This data pertains to the entire day- ignore it
      JsonObject days_0 = doc["days"][0];
      const char* days_0_datetime = days_0["datetime"]; // "2025-07-11"
      long days_0_datetimeEpoch = days_0["datetimeEpoch"]; // 1752206400
      int days_0_tempmax = days_0["tempmax"]; // 74
      float days_0_tempmin = days_0["tempmin"]; // 63.1
      float days_0_temp = days_0["temp"]; // 68.5
      int days_0_feelslikemax = days_0["feelslikemax"]; // 74
      float days_0_feelslikemin = days_0["feelslikemin"]; // 63.1
      float days_0_feelslike = days_0["feelslike"]; // 68.5
      float days_0_dew = days_0["dew"]; // 64.6
      int days_0_humidity = days_0["humidity"]; // 88
      int days_0_precip = days_0["precip"]; // 0
      int days_0_precipprob = days_0["precipprob"]; // 7
      int days_0_precipcover = days_0["precipcover"]; // 0
      // days_0["preciptype"] is null
      int days_0_snow = days_0["snow"]; // 0
      int days_0_snowdepth = days_0["snowdepth"]; // 0
      float days_0_windgust = days_0["windgust"]; // 13.9
      float days_0_windspeed = days_0["windspeed"]; // 12.5
      float days_0_winddir = days_0["winddir"]; // 107.2
      float days_0_pressure = days_0["pressure"]; // 1017.2
      int days_0_cloudcover = days_0["cloudcover"]; // 89
      float days_0_visibility = days_0["visibility"]; // 7.4
      float days_0_solarradiation = days_0["solarradiation"]; // 430.4
      float days_0_solarenergy = days_0["solarenergy"]; // 36.9
      int days_0_uvindex = days_0["uvindex"]; // 8
      int days_0_severerisk = days_0["severerisk"]; // 10
      const char* days_0_sunrise = days_0["sunrise"]; // "05:17:58"
      long days_0_sunriseEpoch = days_0["sunriseEpoch"]; // 1752225478
      const char* days_0_sunset = days_0["sunset"]; // "20:21:25"
      long days_0_sunsetEpoch = days_0["sunsetEpoch"]; // 1752279685
      float days_0_moonphase = days_0["moonphase"]; // 0.53
      const char* days_0_conditions = days_0["conditions"]; // "Partially cloudy"
      const char* days_0_description = days_0["description"]; // "Partly cloudy throughout the day."
      const char* days_0_icon = days_0["icon"]; // "partly-cloudy-day"

      JsonArray days_0_stations = days_0["stations"];
      const char* days_0_stations_0 = days_0_stations[0]; // "KOWD"
      const char* days_0_stations_1 = days_0_stations[1]; // "AV085"
      const char* days_0_stations_2 = days_0_stations[2]; // "KBED"
      const char* days_0_stations_3 = days_0_stations[3]; // "0518W"
      const char* days_0_stations_4 = days_0_stations[4]; // "KBOS"

      const char* days_0_source = days_0["source"]; // "comb"
      */

      JsonObject currentConditions = doc["currentConditions"];
      const char* currentConditions_datetime = currentConditions["datetime"]; // "17:50:00"
      long currentConditions_datetimeEpoch = currentConditions["datetimeEpoch"]; // 1752270600
      float currentConditions_temp = currentConditions["temp"]; // 73.7
      float currentConditions_feelslike = currentConditions["feelslike"]; // 73.7
      float currentConditions_humidity = currentConditions["humidity"]; // 77.6
      float currentConditions_dew = currentConditions["dew"]; // 66.2
      int currentConditions_precip = currentConditions["precip"]; // 0
      int currentConditions_precipprob = currentConditions["precipprob"]; // 0
      int currentConditions_snow = currentConditions["snow"]; // 0
      int currentConditions_snowdepth = currentConditions["snowdepth"]; // 0
      // currentConditions["preciptype"] is null
      int currentConditions_windgust = currentConditions["windgust"]; // 0
      float currentConditions_windspeed = currentConditions["windspeed"]; // 5.1
      int currentConditions_winddir = currentConditions["winddir"]; // 138
      int currentConditions_pressure = currentConditions["pressure"]; // 1017
      float currentConditions_visibility = currentConditions["visibility"]; // 9.9
      int currentConditions_cloudcover = currentConditions["cloudcover"]; // 50
      int currentConditions_solarradiation = currentConditions["solarradiation"]; // 291
      int currentConditions_solarenergy = currentConditions["solarenergy"]; // 1
      int currentConditions_uvindex = currentConditions["uvindex"]; // 3
      const char* currentConditions_conditions = currentConditions["conditions"]; // "Partially cloudy"
      const char* currentConditions_icon = currentConditions["icon"]; // "partly-cloudy-day"

      JsonArray currentConditions_stations = currentConditions["stations"];
      const char* currentConditions_stations_0 = currentConditions_stations[0]; // "BHBM3"
      const char* currentConditions_stations_1 = currentConditions_stations[1]; // "KBOS"
      const char* currentConditions_stations_2 = currentConditions_stations[2]; // "E2727"

      const char* currentConditions_source = currentConditions["source"]; // "obs"
      const char* currentConditions_sunrise = currentConditions["sunrise"]; // "05:17:58"
      long currentConditions_sunriseEpoch = currentConditions["sunriseEpoch"]; // 1752225478
      const char* currentConditions_sunset = currentConditions["sunset"]; // "20:21:25"
      long currentConditions_sunsetEpoch = currentConditions["sunsetEpoch"]; // 1752279685
      float currentConditions_moonphase = currentConditions["moonphase"]; // 0.53

      Serial.println(currentConditions_conditions);
      Serial.println(currentConditions_temp);



    } else {
      Serial.printf("❌ HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end(); // Free resources
  } else {
    Serial.println("⚠️ Not connected to WiFi");
  }
}