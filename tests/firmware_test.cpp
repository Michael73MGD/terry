#include <cassert>
#include <iostream>
#include "fakes/FakeArduino.h"
#include "Weather.h"

// Simulate hardware/library boundaries; exercise the actual sketch code.
bool resultAvailable = false;
WeatherResult queuedResult;
unsigned int requestCount = 0;
bool beginWeather() { return true; }
bool requestWeather(const AppConfig &, uint32_t) { ++requestCount; return true; }
bool receiveWeather(WeatherResult &result) {
  if (!resultAvailable) return false;
  result = queuedResult;
  resultAvailable = false;
  return true;
}

// Arduino normally generates these cross-sketch declarations.
void WM_setup();
void WM_loop();
void markSetupRequired(bool);
void startSetupPortal();
#define time fakeTime
#include "../terry.ino"
#include "../P1_WM.ino"
#undef time

void deliver(WeatherStatus status, uint32_t generation) {
  queuedResult = WeatherResult();
  queuedResult.status = status;
  queuedResult.generation = generation;
  queuedResult.data.temperature = 72;
  queuedResult.data.precipitationProbability = 75;
  snprintf(queuedResult.data.date, sizeof(queuedResult.data.date), "2026-10-25");
  resultAvailable = true;
  updateWeather();
}

int main(int argc, char **argv) {
  if (argc > 1 && (strcmp(argv[1], "wifi-first") == 0 || strcmp(argv[1], "settings-first") == 0)) {
    const bool wifiFirst = strcmp(argv[1], "wifi-first") == 0;
    setup();
    assert(wm.active && !configured);
    assert(strstr(wm.menuInstructions, "Complete these two steps"));
    if (wifiFirst) {
      WiFi.connection = WL_CONNECTED;
      wm.wifiCallback();
      WM_loop();
      assert(wm.active && !configured); // WiFi alone cannot finish first-time setup.
      assert(strstr(pendingNotice, "Complete Setup"));
      updateWeather();
      assert(requestCount == 0); // Do not call the weather API with empty credentials.
    }
    apiKeyBox.setValue("ValidKey123", 49);
    zipCodeBox.setValue("01234", 10);
    wm.http.arguments["time_zone"] = "America/New_York";
    wm.http.arguments["clock_format"] = "24";
    wm.paramsCallback();
    WM_loop();
    assert(configured && configurationPersisted);
    assert(wm.active == !wifiFirst);
    if (!wifiFirst) {
      updateWeather();
      assert(requestCount == 0); // Settings alone cannot finish setup either.
      WiFi.connection = WL_CONNECTED;
      wm.wifiCallback();
      WM_loop();
    }
    assert(!wm.active);
    updateStartupSync();
    const auto previousEpoch = fakeEpoch;
    fakeEpoch = 0;
    updateWeather();
    assert(requestCount == 0);
    updateDisplay();
    assert(displayState == DisplayState::Startup);
    assert(strcmp(displayText, "Sync") == 0);
    assert(ledMatrix.entranceEffect == PA_PRINT);
    const auto syncDisplayCalls = ledMatrix.displayCalls;
    fakeEpoch = previousEpoch + 17; // Deliberately not at a minute boundary.
    updateWeather();
    assert(requestCount == 1 && weatherInFlight);
    updateDisplay();
    assert(displayState == DisplayState::Startup && strcmp(displayText, "Sync") == 0);
    assert(ledMatrix.displayCalls == syncDisplayCalls); // NTP-to-weather transition keeps the same static label.
    deliver(WeatherStatus::Success, configGeneration);
    ledMatrix.animationDone = false; // First reading must interrupt the loading animation.
    updateDisplay();
    assert(displayState == DisplayState::ShowingTime);
    assert(strstr(displayText, "72"));
    assert(!initialWeatherPending);
    std::cout << argv[1] << " setup-order regression test passed.\n";
    return 0;
  }
  if (argc > 1 && strncmp(argv[1], "startup-", 8) == 0) {
    preferences.strings["config"] =
      R"({"api_key":"ValidKey123","zip_code":"01234","time_zone":"UTC","clock_format":"24"})";
    WiFi.connection = WL_CONNECTED;
    fakeEpoch = 0;
    setup();
    updateStartupSync();
    const auto syncRequests = timeSyncRequests;
    updateStartupSync();
    assert(timeSyncRequests == syncRequests); // Don't restart SNTP on every loop.
    assert(!wm.active && configured);
    updateWeather();
    assert(requestCount == 0); // No TLS request until the clock is valid.
    updateDisplay();
    assert(strcmp(displayText, "Sync") == 0);
    assert(ledMatrix.entranceEffect == PA_PRINT);
    const auto syncDisplayCalls = ledMatrix.displayCalls;
    fakeEpoch = 1792929617;
    updateWeather();
    assert(requestCount == 1 && weatherInFlight); // No ten-second or minute-boundary delay.
    updateDisplay();
    assert(displayState == DisplayState::Startup);
    assert(strcmp(displayText, "Sync") == 0);
    assert(ledMatrix.displayCalls == syncDisplayCalls);
    fakeMillis += 1000;
    updateWeather();
    assert(requestCount == 1); // One initial request, not one per display iteration.

    if (strcmp(argv[1], "startup-success") == 0) {
      deliver(WeatherStatus::Success, configGeneration);
      ledMatrix.animationDone = false;
      updateDisplay();
      assert(displayState == DisplayState::ShowingTime);
      assert(strstr(displayText, "72"));
      const std::string firstReading = displayText;
      ledMatrix.animationDone = true;
      updateDisplay();
      assert(displayState == DisplayState::ShowingTime && firstReading == displayText);
      fakeEpoch += 60;
      updateDisplay();
      assert(displayState == DisplayState::ScrollOut); // Later minutes still animate normally.
    } else if (strcmp(argv[1], "startup-failure") == 0) {
      deliver(WeatherStatus::NetworkError, configGeneration);
      assert(!initialWeatherPending);
      updateDisplay(); // Failure notice.
      updateDisplay(); // Clock fallback after the notice finishes.
      assert(displayState == DisplayState::ShowingTime);
      assert(!haveWeather && retryDelay == 60000);
    } else if (strcmp(argv[1], "startup-timeout") == 0) {
      fakeMillis = initialWeatherStartedAt + STARTUP_WEATHER_WAIT - 1;
      updateDisplay();
      assert(displayState == DisplayState::Startup);
      ++fakeMillis;
      updateDisplay();
      assert(displayState == DisplayState::ShowingTime && !haveWeather);
    } else {
      assert(false && "Unknown startup scenario");
    }
    std::cout << argv[1] << " startup regression test passed.\n";
    return 0;
  }
  if (argc > 1) {
    preferences.strings["config"] =
      R"({"api_key":"ValidKey123","zip_code":"01234","time_zone":"UTC"})";
    if (strcmp(argv[1], "boot-recovery") == 0) {
      preferences.flags["needs_setup"] = true;
      WiFi.connection = WL_CONNECTED;
      setup();
      assert(configured && wm.active); // Reboot after rejection must force setup despite working WiFi.
      updateWeather();
      assert(requestCount == 0);
      std::cout << "Next-boot setup recovery test passed.\n";
    } else if (strcmp(argv[1], "offline-boot") == 0) {
      setup();
      assert(configured && connectingAtBoot && !wm.active);
      fakeMillis = 14999;
      WM_loop();
      assert(!wm.active);
      fakeMillis = 15000;
      WM_loop();
      assert(wm.active);
      fakeMillis += PORTAL_TIMEOUT;
      WM_loop();
      assert(!wm.active);
      const auto oldBeginCalls = WiFi.beginCalls;
      WM_loop();
      assert(WiFi.beginCalls == oldBeginCalls + 1);
      std::cout << "Offline boot, setup timeout and reconnection test passed.\n";
    } else {
      assert(false && "Unknown test scenario");
    }
    return 0;
  }
  setup();
  assert(wm.active); // Empty storage always opens setup, even with saved WiFi.
  assert(!configured);
  assert(weatherReady);
  assert(appliedTimeZone == timeZoneRule("America/New_York"));

  // First-boot setup must be visible without a clock, and repeat after each scroll.
  const auto previousEpoch = fakeEpoch;
  fakeEpoch = 0;
  updateDisplay();
  assert(displayState == DisplayState::Setup);
  assert(strstr(displayText, "Connect to Terry WiFi"));
  assert(ledMatrix.font == nullptr); // Full font is required for readable instructions.
  for (int scroll = 0; scroll < 3; ++scroll) {
    updateDisplay();
    assert(displayState == DisplayState::Setup);
    assert(strcmp(displayText, SETUP_MESSAGE) == 0);
  }
  ledMatrix.animationDone = false;
  showNotice("Invalid setup. Try again.");
  updateDisplay();
  assert(strcmp(displayText, SETUP_MESSAGE) == 0); // Retained animation buffer is stable.
  ledMatrix.animationDone = true;
  updateDisplay();
  assert(strstr(displayText, "Invalid setup"));
  updateDisplay();
  assert(strcmp(displayText, SETUP_MESSAGE) == 0); // Resume instructions after an error.
  fakeEpoch = previousEpoch;

  // Custom native selects expose friendly labels, valid values, and saved selections.
  assert(strstr(zoneBox.getCustomHTML(), "<select id='time_zone' name='time_zone'"));
  assert(strstr(zoneBox.getCustomHTML(), "Eastern (New York)"));
  assert(strstr(zoneBox.getCustomHTML(), "value='America/New_York' selected"));
  assert(strstr(clockFormatBox.getCustomHTML(), "<select id='clock_format'"));
  assert(strstr(clockFormatBox.getCustomHTML(), "value='12' selected"));
  for (const auto &zone : TIME_ZONES) {
    assert(timeZoneRule(zone.name));
    zoneBox.setSelected(zone.name);
    assert(strstr(zoneBox.getCustomHTML(), (std::string("value='") + zone.name + "' selected").c_str()));
  }

  WiFi.connection = WL_CONNECTED;
  apiKeyBox.setValue("ValidKey123", 49);
  zipCodeBox.setValue("01234", 10);
  wm.http.arguments["time_zone"] = "UTC";
  wm.http.arguments["clock_format"] = "24";
  wm.paramsCallback();
  WM_loop();
  assert(configured && configurationPersisted);
  assert(!wm.active);
  assert(preferences.flags["needs_setup"]);
  assert(appliedTimeZone == "UTC0");
  assert(config.use24Hour);
  assert(strstr(zoneBox.getCustomHTML(), "value='UTC' selected"));
  assert(strstr(clockFormatBox.getCustomHTML(), "value='24' selected"));
  JsonDocument persisted;
  assert(!deserializeJson(persisted, preferences.strings["config"]));
  AppConfig reloaded;
  assert(readConfig(persisted.as<JsonVariantConst>(), reloaded));
  assert(reloaded.use24Hour && strcmp(reloaded.timeZone, "UTC") == 0);
  updateDisplay();
  assert(displayState != DisplayState::Setup); // Successful setup releases the display to the clock.

  wm.http.arguments["clock_format"] = "25";
  startSetupPortal();
  wm.paramsCallback();
  WM_loop();
  assert(wm.active && config.use24Hour); // Invalid format must not replace valid settings.
  wm.http.arguments["clock_format"] = "24";
  wm.paramsCallback();
  WM_loop();
  assert(!wm.active);

  updateWeather();
  assert(weatherInFlight && requestCount == 1);
  deliver(WeatherStatus::Success, configGeneration);
  assert(haveWeather && weather.temperature == 72);
  assert(!preferences.flags["needs_setup"]);
  deliver(WeatherStatus::NetworkError, configGeneration);
  assert(haveWeather && weather.precipitationProbability == 75);
  assert(retryDelay == 60000); // First failure after success retries in one minute.
  deliver(WeatherStatus::NetworkError, configGeneration);
  assert(retryDelay == 120000);
  deliver(WeatherStatus::RateLimited, configGeneration);
  assert(retryDelay == WEATHER_INTERVAL);
  fakeMillis += WEATHER_MAX_AGE;
  updateWeather();
  assert(!haveWeather); // Never display indefinitely stale weather.

  deliver(WeatherStatus::InvalidSettings, configGeneration);
  assert(weatherSettingsRejected);
  assert(preferences.flags["needs_setup"]);
  assert(strstr(pendingNotice, "Restart Terry"));
  startSetupPortal();
  assert(wm.active);
  const auto oldGeneration = configGeneration;
  zipCodeBox.setValue("bad", 10);
  wm.paramsCallback();
  WM_loop();
  assert(wm.active); // Invalid form must not close setup or replace good settings.
  assert(strcmp(config.zipCode, "01234") == 0);
  deliver(WeatherStatus::Success, oldGeneration);
  assert(preferences.flags["needs_setup"]); // Ignore responses from before an invalid save.

  zipCodeBox.setValue("90210", 10);
  wm.paramsCallback();
  WM_loop();
  assert(!wm.active);
  assert(!weatherSettingsRejected);
  assert(!haveWeather);
  assert(strcmp(config.zipCode, "90210") == 0);
  deliver(WeatherStatus::Success, oldGeneration);
  assert(!haveWeather); // An old location cannot replace the new location's data.
  deliver(WeatherStatus::Success, configGeneration);
  assert(haveWeather);

  startSetupPortal();
  preferences.configWritable = false;
  zipCodeBox.setValue("10001", 10);
  wm.paramsCallback();
  WM_loop();
  assert(!configurationPersisted);
  preferences.configWritable = true;
  deliver(WeatherStatus::Success, configGeneration);
  assert(preferences.flags["needs_setup"]); // Unsaved settings must not disable recovery.

  startSetupPortal();
  fakeMillis += PORTAL_TIMEOUT;
  WM_loop();
  assert(!wm.active);
  assert(strstr(pendingNotice, "Restart Terry"));

  // Exhaust the small display's realistic input range, using its actual font.
  struct tm timeinfo = {};
  timeinfo.tm_year = 126;
  timeinfo.tm_mon = 9;
  timeinfo.tm_mday = 25;
  haveWeather = true;
  weatherReceived = millis();
  for (bool use24Hour : {false, true}) {
    config.use24Hour = use24Hour;
    for (int hour = 0; hour < 24; ++hour) {
      timeinfo.tm_hour = hour;
      for (int minute = 0; minute < 60; ++minute) {
        timeinfo.tm_min = minute;
        for (int temperature : {-150, -100, -14, -1, 0, 72, 99, 100, 160}) {
          weather.temperature = temperature;
          for (float probability : {0.0f, 100.0f}) {
            weather.precipitationProbability = probability;
            formatClock(timeinfo);
            assert(textFits(displayText));
            char expectedClock[10];
            strftime(expectedClock, sizeof(expectedClock), use24Hour ? "%H:%M" : "%I:%M", &timeinfo);
            // 24-hour midnight/early hours must retain their leading zero even
            // when the temperature is too wide and weather has to be omitted.
            if (use24Hour) assert(strncmp(displayText, expectedClock, 5) == 0);
          }
        }
      }
    }
  }

  haveWeather = false;
  const int hours[] = {0, 12, 13, 23};
  const char *expected12[] = {"12:05", "12:05", "01:05", "11:05"};
  const char *expected24[] = {"00:05", "12:05", "13:05", "23:05"};
  timeinfo.tm_min = 5;
  for (size_t i = 0; i < 4; ++i) {
    timeinfo.tm_hour = hours[i];
    config.use24Hour = false;
    formatClock(timeinfo);
    assert(strcmp(displayText, expected12[i]) == 0);
    config.use24Hour = true;
    formatClock(timeinfo);
    assert(strcmp(displayText, expected24[i]) == 0);
  }

  displayState = DisplayState::ScrollOut;
  ledMatrix.animationDone = false;
  snprintf(displayText, sizeof(displayText), "12:59  72|");
  showNotice("Setup failed. Restart Terry.");
  updateDisplay();
  assert(strcmp(displayText, "12:59  72|") == 0); // Don't mutate Parola's active buffer.
  ledMatrix.animationDone = true;
  updateDisplay();
  assert(displayState == DisplayState::ScrollIn);
  updateDisplay();
  updateDisplay();
  assert(displayState == DisplayState::Notice);
  assert(strstr(displayText, "Restart Terry"));

  for (const auto &message : Serial.messages) {
    assert(message.find(config.apiKey) == std::string::npos);
  }

  std::cout << "Setup, persistence, weather recovery and display regression tests passed.\n";
}
