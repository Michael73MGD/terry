#define ESP_DRD_USE_SPIFFS true
#define JSON_CONFIG_FILE "/1st_config.json" // JSON configuration file

bool shouldSaveConfig = true; // Flag for saving data

bool dev_mode = false; // Flag to reset all WM settings to force portal.

WiFiManager wm; // Define WiFiManager Object

void saveConfigFile(){ // Save Config in JSON format
  Serial.println(F("Saving configuration..."));
  StaticJsonDocument<512> json; // Create a JSON document
  json["api_key"] = api_key;
  json["zip_code"] = zip_code;
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w"); // Open config file
  if (!configFile) { // Error, file did not open
    Serial.println("failed to open config file for writing");
  }
  serializeJsonPretty(json, Serial); // Serialize JSON data to write to file
  if (serializeJson(json, configFile) == 0){ // Error writing file
    Serial.println(F("Failed to write to file"));
  }
  configFile.close(); // Close file
}

bool loadConfigFile(){ // Load existing configuration file
  // SPIFFS.format(); // Uncomment if we need to format filesystem

  Serial.println("Mounting File System..."); // Read configuration from FS json

  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      // The file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        Serial.println("Opened configuration file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error){
          Serial.println("Parsing JSON");          
          strcpy(api_key, json["api_key"]);
          strcpy(zip_code, json["zip_code"]);
          return true;
        }
        else{ // Error loading JSON data
          Serial.println("Failed to load json config");
        }
      }
    }
  }
  else { // Error mounting file system
    Serial.println("Failed to mount FS");
  }
  return false;
}

void saveConfigCallback(){ // Callback notifying us of the need to save configuration
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configModeCallback(WiFiManager *myWiFiManager){ // Called when config mode launched
  Serial.println("Entered Configuration Mode");
  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void WM_setup(){ // Change to true when testing to force configuration every time we run
  bool forceConfig = true;
  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup){
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }
  WiFi.mode(WIFI_STA); // Explicitly set WiFi mode
  Serial.begin(115200); // Setup Serial monitor
  delay(10);

  if(dev_mode) wm.resetSettings(); // Reset settings (only for development)
  wm.setSaveConfigCallback(saveConfigCallback); // Set config save notify callback
  wm.setAPCallback(configModeCallback); // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode

  // Custom elements
  WiFiManagerParameter api_key_note("<p>Get your weather API key here: <a href='https://www.visualcrossing.com/sign-up' target='_blank'>visualcrossing.com/sign-up</a></p>");
  wm.addParameter(&api_key_note);

  WiFiManagerParameter api_key_box("api_key", "Enter your api_key", api_key, 50); // Text box (String) - 50 characters maximum
  WiFiManagerParameter zip_code_box("zip_code", "Enter your zip_code", zip_code, 50); // Text box (String) - 50 characters maximum
  
  // Add all defined parameters  
  wm.addParameter(&api_key_box);
  wm.addParameter(&zip_code_box);

  if (forceConfig) {// Run if we need a configuration
    if (!wm.autoConnect("Terry")){
      Serial.println("failed to connect and hit timeout");
      delay(3000); //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }
  }
  else{
    if (!wm.autoConnect("Terry")){
      Serial.println("failed to connect and hit timeout");
      delay(3000); // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }

  // If we get here, we are connected to the WiFi
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Lets deal with the user config values
  strncpy(api_key, api_key_box.getValue(), sizeof(api_key)); // Copy the string value
  strncpy(zip_code, zip_code_box.getValue(), sizeof(zip_code)); // Copy the string value
  Serial.print("api_key: ");
  Serial.println(api_key);
  Serial.print("zip_code: ");
  Serial.println(zip_code);

  // Save the custom parameters to FS
  if (shouldSaveConfig) {
    saveConfigFile();
  }
}