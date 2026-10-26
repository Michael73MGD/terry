// Parameters must outlive setup(): the portal is serviced from loop().
WiFiManager wm;
Preferences preferences;
bool storageReady = false;
bool submittedConfig = false;
String submittedTimeZone;
String submittedClockFormat;
bool reconnectAfterPortal = false;
bool connectingAtBoot = false;
bool portalConfigAccepted = false;
bool portalSaveCompleted = false;
unsigned long connectionStarted = 0;
unsigned long portalStarted = 0;
unsigned long lastReconnect = 0;
constexpr unsigned long PORTAL_TIMEOUT = 5UL * 60 * 1000;

WiFiManagerParameter apiKeyBox("api_key", "Weather API key", "", 49, "type=\"password\"");
WiFiManagerParameter zipCodeBox("zip_code", "US ZIP code (12345 or 12345-6789)", "", 10);
SettingsDropdown zoneBox(SettingsDropdown::Field::TimeZone);
SettingsDropdown clockFormatBox(SettingsDropdown::Field::ClockFormat);
WiFiManagerParameter configNote(
  "<p>Get a weather key at <a href='https://www.visualcrossing.com/sign-up' "
  "target='_blank' rel='noopener'>Visual Crossing</a>.</p>"
  "<p>Choose your local time zone. Daylight saving is handled automatically "
  "where applicable.</p>"
  "<p>If setup fails, restart Terry and reconnect to its WiFi network.</p>");

bool saveConfigFile() {
  if (!storageReady) return false;
  JsonDocument json;
  json["api_key"] = config.apiKey;
  json["zip_code"] = config.zipCode;
  json["time_zone"] = config.timeZone;
  json["clock_format"] = config.use24Hour ? "24" : "12";
  String serialized;
  serializeJson(json, serialized);
  // NVS commits a complete value rather than truncating a live config file.
  return preferences.putString("config", serialized) == serialized.length();
}

bool loadConfigFile() {
  storageReady = preferences.begin("terry", false);
  if (!storageReady) {
    Serial.println("Settings storage unavailable; configuration will not survive a restart.");
    return false;
  }
  if (preferences.isKey("config")) {
    const String serialized = preferences.getString("config");
    if (serialized.length() > 1024) return false;
    JsonDocument json;
    configurationPersisted = !deserializeJson(json, serialized) &&
                             readConfig(json.as<JsonVariantConst>(), config);
    return configurationPersisted;
  }

  // Migrate existing devices without formatting or erasing their SPIFFS data.
  if (!SPIFFS.begin(false)) return false;
  File file = SPIFFS.open("/1st_config.json", "r");
  JsonDocument json;
  const bool valid = file && file.size() <= 1024 && !deserializeJson(json, file) &&
                     readConfig(json.as<JsonVariantConst>(), config);
  file.close();
  SPIFFS.end();
  if (valid) {
    configurationPersisted = saveConfigFile();
    if (!configurationPersisted) Serial.println("Could not migrate settings to NVS.");
  }
  return valid;
}

void markSetupRequired(bool required) {
  if (storageReady && preferences.getBool("needs_setup", false) != required &&
      preferences.putBool("needs_setup", required) != 1) {
    Serial.println("Could not save setup recovery flag; send 'setup' in Serial Monitor to recover.");
  }
}

void populateConfigFields() {
  apiKeyBox.setValue(config.apiKey, sizeof(config.apiKey) - 1);
  zipCodeBox.setValue(config.zipCode, sizeof(config.zipCode) - 1);
  zoneBox.setSelected(config.timeZone);
  clockFormatBox.setSelected(config.use24Hour ? "24" : "12");
}

void startSetupPortal() {
  if (wm.getConfigPortalActive()) return;
  connectingAtBoot = false;
  portalConfigAccepted = configured && !(storageReady && preferences.getBool("needs_setup", false));
  portalSaveCompleted = false;
  populateConfigFields();
  // Deliberately open, as requested by the owner. Restrict its lifetime.
  wm.startConfigPortal("Terry");
  portalStarted = millis();
  if (wm.getConfigPortalActive()) {
    Serial.println("Setup: join Terry WiFi and open http://192.168.4.1");
    showNotice(SETUP_MESSAGE);
  } else {
    showNotice("Setup failed. Restart Terry and reconnect over WiFi.");
  }
}

void WM_setup() {
  configured = loadConfigFile();
  wm.setDebugOutput(false); // Never let library diagnostics print credentials.
  wm.setConfigPortalBlocking(false);
  wm.setConnectTimeout(10);
  wm.setDisableConfigPortal(false); // Close it ourselves after validation.
  wm.setSaveParamsCallback([] {
    submittedTimeZone = wm.server->arg("time_zone");
    submittedClockFormat = wm.server->arg("clock_format");
    submittedConfig = true;
  });
  wm.setSaveConfigCallback([] {
    portalSaveCompleted = true;
    if (!portalConfigAccepted) {
      showNotice("WiFi connected. Complete Setup at 192.168.4.1");
    }
  });
  wm.setParamsPage(true);
  wm.setCustomMenuHTML(
    "<p><strong>Complete these two steps:</strong></p>"
    "<ol style='text-align:left'><li>Open <strong>Setup</strong>, enter your weather "
    "key and ZIP code, choose your settings, and save.</li>"
    "<li>Open <strong>Configure WiFi</strong> and connect to your home network.</li></ol>"
    "<p>Terry closes setup when both steps are complete. If you connect WiFi first, "
    "rejoin Terry if needed and finish Setup at http://192.168.4.1.</p>");
  const char *menu[] = {"custom", "param", "wifi", "sep", "exit"};
  wm.setMenu(menu, 5);
  wm.addParameter(&configNote);
  wm.addParameter(&apiKeyBox);
  wm.addParameter(&zipCodeBox);
  wm.addParameter(&zoneBox);
  wm.addParameter(&clockFormatBox);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  populateConfigFields();

  if (!configured || (storageReady && preferences.getBool("needs_setup", false))) {
    startSetupPortal();
  } else {
    WiFi.begin(); // Nonblocking attempt with saved network credentials.
    connectingAtBoot = true;
    connectionStarted = millis();
  }
}

void WM_loop() {
  wm.process();
  if (submittedConfig) {
    submittedConfig = false;
    JsonDocument json;
    json["api_key"] = apiKeyBox.getValue();
    json["zip_code"] = zipCodeBox.getValue();
    json["time_zone"] = submittedTimeZone;
    json["clock_format"] = submittedClockFormat;
    AppConfig candidate;
    if (!readConfig(json.as<JsonVariantConst>(), candidate)) {
      portalConfigAccepted = false;
      ++configGeneration;
      weatherSettingsRejected = true;
      markSetupRequired(true);
      showNotice("Invalid setup. Check key, ZIP and settings. Restart to reconnect.");
    } else {
      config = candidate;
      populateConfigFields();
      configured = true;
      portalConfigAccepted = true;
      portalSaveCompleted = true;
      ++configGeneration;
      // Keep next-boot recovery enabled until the weather API accepts settings.
      markSetupRequired(true);
      configurationPersisted = saveConfigFile();
      if (!configurationPersisted) {
        showNotice("Settings not saved. Check Serial Monitor. Retry setup after restart.");
      }
      applyConfiguration();
    }
  }

  if (connectingAtBoot) {
    if (WiFi.status() == WL_CONNECTED) connectingAtBoot = false;
    else if (millis() - connectionStarted >= 15000) startSetupPortal();
  }
  if (wm.getConfigPortalActive()) {
    if (portalSaveCompleted && portalConfigAccepted && WiFi.status() == WL_CONNECTED) {
      // Connection/setup reminders are obsolete once both steps succeed. Do
      // retain a storage failure notice if these settings could not be saved.
      if (configurationPersisted) pendingNotice[0] = '\0';
      wm.stopConfigPortal();
    } else if (millis() - portalStarted >= PORTAL_TIMEOUT) {
      wm.stopConfigPortal();
      reconnectAfterPortal = true;
      showNotice("Setup timed out. Restart Terry and reconnect over WiFi.");
    }
  } else if (reconnectAfterPortal ||
             (!connectingAtBoot && WiFi.status() != WL_CONNECTED && millis() - lastReconnect >= 30000)) {
    reconnectAfterPortal = false;
    lastReconnect = millis();
    WiFi.begin();
  }
}
