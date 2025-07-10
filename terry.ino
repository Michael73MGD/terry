#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include "mfactoryfont.h"  // Custom font

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
const char* endpoint = "https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/Boston?unitGroup=us&elements=datetime%2CdatetimeEpoch%2Cname%2Clatitude%2Clongitude%2Ctempmax%2Ctempmin%2Ctemp%2Cfeelslikemax%2Cfeelslikemin%2Cfeelslike%2Cprecip%2Cprecipprob%2Cprecipcover%2Cpreciptype%2Csnow%2Csnowdepth%2Cwindgust%2Cwindspeed%2Csunrise%2Csunset%2Cmoonphase%2Cconditions%2Cdescription%2Cicon&include=days%2Chours&key=2WKEX8NVUHXNVV7SYHEX3EQBL&contentType=json";
const unsigned long interval = 5 * 60 * 1000;  // 5 minutes
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
  ledMatrix.setIntensity(15);
  ledMatrix.setTextAlignment(PA_CENTER);
  ledMatrix.setFont(mFactory);

  // Start with initial time
  strftime(currentTimeStr, sizeof(currentTimeStr), "%I:%M %p", &timeinfo);
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
      String payload = http.getString();
      Serial.println("📥 Response:");
      Serial.println(payload);
    } else {
      Serial.printf("❌ HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end(); // Free resources
  } else {
    Serial.println("⚠️ Not connected to WiFi");
  }
}