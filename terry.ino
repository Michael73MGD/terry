#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

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
}

void loop() {
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
