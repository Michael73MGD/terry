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
}

void loop() {
  // Put your main code here
}
