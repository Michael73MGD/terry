#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 17

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 3600;

MD_Parola ledMatrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

char currentTimeStr[6] = "-----";
char nextTimeStr[6] = "-----";
int lastMinute = -1;

enum State {
  SHOWING_TIME,
  SCROLL_OUT,
  SCROLL_IN
};

State state = SHOWING_TIME;

// const char* location = "Somerville";
// const char* endpoint = "https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/boston/next24hours?unitGroup=us&elements=datetime%2Ctempmax%2Ctempmin%2Ctemp%2Cfeelslikemax%2Cfeelslikemin%2Cfeelslike%2Chumidity%2Cprecip%2Cpreciptype%2Cwindspeedmean%2Cmoonphase%2Cconditions%2Cdescription%2Cicon&key=2WKEX8NVUHXNVV7SYHEX3EQBL&contentType=json";
const char* endpoint = "https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/boston/today?unitGroup=us&elements=datetime%2Ctempmax%2Ctempmin%2Ctemp%2Cfeelslikemax%2Cfeelslikemin%2Cfeelslike%2Chumidity%2Cprecip%2Cpreciptype%2Cwindspeedmean%2Cmoonphase%2Cconditions%2Cdescription%2Cicon&include=days&key=2WKEX8NVUHXNVV7SYHEX3EQBL&contentType=json";
const unsigned long interval = 15 * 60 * 1000;  // 15 minutes
unsigned long lastRequestTime = 0;


void setup() {
  Serial.begin(115200);
  delay(1000);

  // Create an instance of WiFiManager
  WiFiManager wm;

  // Optionally reset saved settings (uncomment to test captive portal again)
  // wm.resetSettings();

  // Automatically connect to saved WiFi or start config portal
  if (!wm.autoConnect("ESP32-Setup")) {
    Serial.println("⚠️ Failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
  }

  // If connected:
  Serial.println("✅ Connected to WiFi!");
  Serial.print("🔗 IP Address: ");
  Serial.println(WiFi.localIP());
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Waiting for NTP time...");
    delay(1000);
  }

  ledMatrix.begin();
  ledMatrix.setIntensity(5);
  ledMatrix.setTextAlignment(PA_CENTER);

  // Start with initial time
  strftime(currentTimeStr, sizeof(currentTimeStr), "%H:%M", &timeinfo);
  ledMatrix.displayText(currentTimeStr, PA_CENTER, 100, 0, PA_SCROLL_RIGHT, PA_NO_EFFECT);
  ledMatrix.displayAnimate();
  
  // Make first request immediately
  makeApiRequest();
  lastRequestTime = millis();
}

void loop() {
  if (millis() - lastRequestTime >= interval) {
    makeApiRequest();
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
        ledMatrix.displayText(currentTimeStr, PA_CENTER, 100, 0, PA_NO_EFFECT, PA_SCROLL_LEFT);
        ledMatrix.displayAnimate();
      }
      break;

    case SCROLL_OUT:
      if (ledMatrix.displayAnimate()) {
        // Old time has scrolled out → scroll new time in
        strftime(currentTimeStr, sizeof(currentTimeStr), "%I:%M %p", &timeinfo);
        state = SCROLL_IN;
        ledMatrix.displayText(currentTimeStr, PA_CENTER, 100, 0, PA_SCROLL_LEFT, PA_NO_EFFECT);
        ledMatrix.displayAnimate();
      }
      break;

    case SCROLL_IN:
      if (ledMatrix.displayAnimate()) {
        // New time is now centered and showing
        state = SHOWING_TIME;
      }
      break;
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
      DynamicJsonDocument doc(16384);

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
      const char* description = doc["description"]; // "Cooling down with no rain expected."

      for (JsonObject day : doc["days"].as<JsonArray>()) {

        const char* day_datetime = day["datetime"]; // "2025-07-10", "2025-07-11"
        float day_tempmax = day["tempmax"]; // 67.9, 72.1
        float day_tempmin = day["tempmin"]; // 65.1, 65
        float day_temp = day["temp"]; // 66.5, 68.5
        float day_feelslikemax = day["feelslikemax"]; // 67.9, 72.1
        float day_feelslikemin = day["feelslikemin"]; // 65.1, 65
        float day_feelslike = day["feelslike"]; // 66.5, 68.5
        float day_humidity = day["humidity"]; // 94.9, 87.6
        float day_precip = day["precip"]; // 1.319, 0

        const char* day_preciptype_0 = day["preciptype"][0]; // "rain", nullptr

        float day_windspeedmean = day["windspeedmean"]; // 7.9, 5.2
        float day_moonphase = day["moonphase"]; // 0.5, 0.53
        const char* day_conditions = day["conditions"]; // "Rain, Overcast", "Partially cloudy"
        const char* day_description = day["description"]; // "Cloudy skies throughout the day with a chance of ...
        const char* day_icon = day["icon"]; // "rain", "partly-cloudy-day"

        
        Serial.print("day_temp: ");
        Serial.println(day_temp);
        Serial.print("day_feelslike: ");
        Serial.println(day_feelslike);
        Serial.print("day_humidity: ");
        Serial.println(day_humidity);
        Serial.print("day_icon: ");
        Serial.println(day_icon);
        Serial.print("day_description: ");
        Serial.println(day_description);

        for (JsonObject day_hour : day["hours"].as<JsonArray>()) {

          const char* day_hour_datetime = day_hour["datetime"]; // "00:00:00", "01:00:00", "02:00:00", "03:00:00", ...
          float day_hour_temp = day_hour["temp"]; // 66.8, 66, 66, 66.8, 66.9, 66, 66, 66, 66, 66, 66, 65.1, 65.1, ...
          float day_hour_feelslike = day_hour["feelslike"]; // 66.8, 66, 66, 66.8, 66.9, 66, 66, 66, 66, 66, 66, ...
          float day_hour_humidity = day_hour["humidity"]; // 93.89, 96.76, 96.78, 96.82, 96.82, 99.79, 99.78, ...
          float day_hour_precip = day_hour["precip"]; // 0, 0, 0, 0, 0.001, 0.07, 0.371, 0.034, 0.321, 0.253, ...
          // day_hour["preciptype"] is null
          const char* day_hour_conditions = day_hour["conditions"]; // "Overcast", "Overcast", "Overcast", ...
          const char* day_hour_icon = day_hour["icon"]; // "cloudy", "cloudy", "cloudy", "cloudy", "rain", "rain", ...

        }

      }

      JsonObject alerts_0 = doc["alerts"][0];
      const char* alerts_0_event = alerts_0["event"]; // "Flood Watch"
      const char* alerts_0_headline = alerts_0["headline"]; // "Flood Watch issued July 10 at 8:53AM EDT until ...
      const char* alerts_0_ends = alerts_0["ends"]; // "2025-07-10T16:00:00"
      long alerts_0_endsEpoch = alerts_0["endsEpoch"]; // 1752177600
      const char* alerts_0_onset = alerts_0["onset"]; // "2025-07-10T08:53:00"
      long alerts_0_onsetEpoch = alerts_0["onsetEpoch"]; // 1752151980
      const char* alerts_0_id = alerts_0["id"];
      const char* alerts_0_language = alerts_0["language"]; // "en"
      const char* alerts_0_link = alerts_0["link"]; // "http://www.weather.gov"
      const char* alerts_0_description = alerts_0["description"]; // "* WHAT...Flooding caused by excessive ...

      JsonObject currentConditions = doc["currentConditions"];
      const char* currentConditions_datetime = currentConditions["datetime"]; // "13:50:00"
      float currentConditions_temp = currentConditions["temp"]; // 64.1
      float currentConditions_feelslike = currentConditions["feelslike"]; // 64.1
      float currentConditions_humidity = currentConditions["humidity"]; // 95.7
      float currentConditions_precip = currentConditions["precip"]; // 0.001

      const char* currentConditions_preciptype_0 = currentConditions["preciptype"][0]; // "rain"

      const char* currentConditions_conditions = currentConditions["conditions"]; // "Rain, Partially cloudy"
      const char* currentConditions_icon = currentConditions["icon"]; // "rain"
      float currentConditions_moonphase = currentConditions["moonphase"]; // 0.5

    } else {
      Serial.printf("❌ HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end(); // Free resources
  } else {
    Serial.println("⚠️ Not connected to WiFi");
  }
}