#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <time.h>
#include "TerryData.h"
#include "Weather.h"
#include "SettingsDropdown.h"
#include "mfactoryfont.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 17

AppConfig config;
bool configured = false;
bool configurationPersisted = false;
uint32_t configGeneration = 0;
MD_Parola ledMatrix(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
extern WiFiManager wm;

enum class DisplayState { WaitingForTime, ShowingTime, ScrollOut, ScrollIn, Notice, Setup, Startup };
DisplayState displayState = DisplayState::WaitingForTime;
char displayText[160] = ""; // Parola retains this pointer until an animation finishes.
char pendingNotice[160] = "";
char lastDisplayedMinute[24] = "";
bool refreshDisplay = true;
bool weatherReady = false;
bool weatherInFlight = false;
bool haveWeather = false;
bool weatherSettingsRejected = false;
bool reportedWeatherFailure = false;
bool connectionWasReady = false;
bool initialWeatherPending = true;
bool initialWeatherStarted = false;
unsigned long initialWeatherStartedAt = 0;
unsigned long startupWaitStartedAt = 0;
char startupMessage[32] = "";
WeatherData weather;
unsigned long weatherReceived = 0;
unsigned long lastWeatherAttempt = 0;
unsigned long retryDelay = 0;
unsigned long failureBackoff = 0;
unsigned long lastSerialInput = 0;
constexpr unsigned long WEATHER_INTERVAL = 15UL * 60 * 1000;
constexpr unsigned long WEATHER_MAX_AGE = 60UL * 60 * 1000;
constexpr unsigned long STARTUP_WEATHER_WAIT = 20000;
constexpr const char *SETUP_MESSAGE = "Connect to Terry WiFi to complete setup. Open 192.168.4.1";

void showNotice(const char *message) {
  Serial.println(message);
  snprintf(pendingNotice, sizeof(pendingNotice), "%s", message);
}

void applyConfiguration() {
  configTzTime(timeZoneRule(config.timeZone), "pool.ntp.org", "time.nist.gov");
  haveWeather = false;
  weatherSettingsRejected = false;
  reportedWeatherFailure = false;
  retryDelay = 0;
  failureBackoff = 0;
  connectionWasReady = false;
  initialWeatherPending = true;
  initialWeatherStarted = false;
  startupWaitStartedAt = millis();
  refreshDisplay = true;
}

void updateStartupSync() {
  const bool ready = configured && !wm.getConfigPortalActive() && WiFi.status() == WL_CONNECTED;
  if (ready && !connectionWasReady) {
    // SNTP may have been started before WiFi was configured. Restart it once
    // the connection is usable so the first sync is requested immediately.
    configTzTime(timeZoneRule(config.timeZone), "pool.ntp.org", "time.nist.gov");
    if (initialWeatherPending) {
      retryDelay = 0;
      if (!initialWeatherStarted) startupWaitStartedAt = millis();
    }
  }
  connectionWasReady = ready;
}

// Measure the actual custom glyphs, including spacing, before placing text on
// the 32-column display. Negative and three-digit temperatures must also fit.
bool textFits(const char *text) {
  unsigned int width = 0;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
    const uint8_t *glyph = mFactory;
    for (unsigned int c = 0; c < *p; ++c) glyph += pgm_read_byte(glyph) + 1;
    if (p != reinterpret_cast<const unsigned char *>(text)) ++width;
    width += pgm_read_byte(glyph);
  }
  return width <= MAX_DEVICES * 8;
}

void formatClock(const struct tm &timeinfo) {
  char clock[10];
  strftime(clock, sizeof(clock), config.use24Hour ? "%H:%M" : "%I:%M", &timeinfo);
  const bool usableWeather = haveWeather && millis() - weatherReceived < WEATHER_MAX_AGE;
  char today[11];
  strftime(today, sizeof(today), "%Y-%m-%d", &timeinfo);
  // Hide yesterday's umbrella after midnight in the configured timezone.
  const bool rainy = usableWeather && strcmp(today, weather.date) == 0 &&
                     weather.precipitationProbability > 50;
  if (usableWeather) {
    snprintf(displayText, sizeof(displayText), "%s%s%.0f%s", clock,
             rainy ? "{" : "  ", weather.temperature, rainy ? "" : "|");
    if (textFits(displayText)) return;
    snprintf(displayText, sizeof(displayText), "%s%s%.0f", clock,
             rainy ? "{" : " ", weather.temperature);
    if (textFits(displayText)) return;
    if (!config.use24Hour && clock[0] == '0') memmove(clock, clock + 1, strlen(clock));
    snprintf(displayText, sizeof(displayText), "%s%s%.0f", clock,
             rainy ? "{" : "", weather.temperature);
    if (textFits(displayText)) return;
  }
  snprintf(displayText, sizeof(displayText), "%s", clock);
}

void updateDisplay() {
  // Reading system time is immediate even before NTP has synchronized.
  const time_t now = time(nullptr);
  struct tm timeinfo;
  const bool timeReady = now >= 1704067200 && localtime_r(&now, &timeinfo);
  const bool animationDone = ledMatrix.displayAnimate();

  // Setup instructions repeat for the entire portal window, including before
  // the device has WiFi or a synchronized clock. A one-shot notice gets lost
  // after its first scroll and leaves only the unsynchronized time placeholder.
  if (wm.getConfigPortalActive()) {
    if (displayState != DisplayState::Setup || animationDone) {
      snprintf(displayText, sizeof(displayText), "%s",
               pendingNotice[0] ? pendingNotice : SETUP_MESSAGE);
      pendingNotice[0] = '\0';
      ledMatrix.setFont(nullptr);
      ledMatrix.displayText(displayText, PA_LEFT, 50, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      displayState = DisplayState::Setup;
    }
    return;
  }
  if (displayState == DisplayState::Setup) {
    ledMatrix.displayClear();
    displayState = DisplayState::WaitingForTime;
    refreshDisplay = true;
  }

  const unsigned long waitingSince = initialWeatherStarted ? initialWeatherStartedAt : startupWaitStartedAt;
  if (initialWeatherPending && millis() - waitingSince >= STARTUP_WEATHER_WAIT) {
    initialWeatherPending = false; // A slow weather service must not hide the clock indefinitely.
  }
  if (configured && (!timeReady || (initialWeatherPending && weatherReady))) {
    // Both labels fit without scrolling. Keep the same indicator across NTP
    // and weather fetching so a phase change cannot cut a word in half.
    const char *message = WiFi.status() != WL_CONNECTED ? "WiFi" : "Sync";
    if (displayState != DisplayState::Startup || strcmp(startupMessage, message) != 0) {
      snprintf(startupMessage, sizeof(startupMessage), "%s", message);
      snprintf(displayText, sizeof(displayText), "%s", message);
      ledMatrix.setFont(nullptr);
      ledMatrix.displayText(displayText, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      ledMatrix.displayAnimate();
      displayState = DisplayState::Startup;
    }
    return;
  }
  if (displayState == DisplayState::Startup) {
    ledMatrix.displayClear();
    displayState = DisplayState::WaitingForTime;
    refreshDisplay = true;
  }

  if (displayState == DisplayState::Notice) {
    if (!animationDone) return;
    displayState = DisplayState::WaitingForTime;
    refreshDisplay = true;
  }
  if (pendingNotice[0] && (displayState == DisplayState::ShowingTime ||
                          displayState == DisplayState::WaitingForTime)) {
    snprintf(displayText, sizeof(displayText), "%s", pendingNotice);
    pendingNotice[0] = '\0';
    ledMatrix.setFont(nullptr);
    ledMatrix.displayText(displayText, PA_LEFT, 50, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    displayState = DisplayState::Notice;
    return;
  }
  if (!timeReady) {
    if (displayState != DisplayState::WaitingForTime || refreshDisplay) {
      snprintf(displayText, sizeof(displayText), "--:--");
      ledMatrix.setFont(mFactory);
      ledMatrix.displayText(displayText, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      displayState = DisplayState::WaitingForTime;
      refreshDisplay = false;
    }
    return;
  }

  char minute[24];
  strftime(minute, sizeof(minute), "%Y-%m-%d %H:%M", &timeinfo);
  switch (displayState) {
  case DisplayState::WaitingForTime:
    ledMatrix.setFont(mFactory);
    formatClock(timeinfo);
    snprintf(lastDisplayedMinute, sizeof(lastDisplayedMinute), "%s", minute);
    // Paint the first complete reading immediately, even halfway through a
    // minute. Subsequent minute changes keep the normal scrolling transition.
    ledMatrix.displayText(displayText, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
    ledMatrix.displayAnimate();
    displayState = DisplayState::ShowingTime;
    refreshDisplay = false;
    break;
  case DisplayState::ShowingTime:
    if (refreshDisplay || strcmp(minute, lastDisplayedMinute) != 0) {
      ledMatrix.displayText(displayText, PA_LEFT, 100, 0, PA_NO_EFFECT, PA_SCROLL_LEFT);
      displayState = DisplayState::ScrollOut;
    }
    break;
  case DisplayState::ScrollOut:
    if (animationDone) {
      formatClock(timeinfo);
      snprintf(lastDisplayedMinute, sizeof(lastDisplayedMinute), "%s", minute);
      ledMatrix.displayText(displayText, PA_LEFT, 100, 0, PA_SCROLL_LEFT, PA_NO_EFFECT);
      displayState = DisplayState::ScrollIn;
      refreshDisplay = false;
    }
    break;
  case DisplayState::ScrollIn:
    if (animationDone) displayState = DisplayState::ShowingTime;
    break;
  case DisplayState::Notice:
  case DisplayState::Setup:
  case DisplayState::Startup:
    break;
  }
}

void updateWeather() {
  WeatherResult result;
  if (receiveWeather(result)) {
    weatherInFlight = false;
    // A response for the previous key/ZIP must not replace new settings.
    if (result.generation == configGeneration) {
      lastWeatherAttempt = millis();
      if (result.status != WeatherStatus::WaitingForTime) initialWeatherPending = false;
      if (result.status == WeatherStatus::Success) {
        weather = result.data;
        haveWeather = true;
        weatherReceived = millis();
        retryDelay = WEATHER_INTERVAL;
        failureBackoff = 0;
        reportedWeatherFailure = false;
        refreshDisplay = true;
        if (configurationPersisted) markSetupRequired(false);
        Serial.printf("Weather updated: %.1f F, precipitation probability %.0f%%\n",
                      weather.temperature, weather.precipitationProbability);
      } else {
        Serial.printf("Weather update failed: status=%u, HTTP=%d\n",
                      static_cast<unsigned int>(result.status), result.httpCode);
        if (result.status == WeatherStatus::InvalidSettings) {
          weatherSettingsRejected = true;
          markSetupRequired(true);
          showNotice("Weather setup failed. Restart Terry and reconnect over WiFi.");
        } else if (result.status == WeatherStatus::WaitingForTime) {
          retryDelay = 0; // Retry as soon as the clock becomes valid, not ten seconds later.
        } else {
          failureBackoff = weatherRetryDelay(failureBackoff);
          retryDelay = result.status == WeatherStatus::RateLimited
                         ? WEATHER_INTERVAL : failureBackoff;
          if (!reportedWeatherFailure) {
            showNotice("Weather unavailable. Retrying automatically.");
            reportedWeatherFailure = true;
          }
        }
      }
    }
  }
  if (haveWeather && millis() - weatherReceived >= WEATHER_MAX_AGE) {
    haveWeather = false;
    refreshDisplay = true;
  }
  if (weatherReady && configured && !weatherInFlight && !weatherSettingsRejected &&
      !wm.getConfigPortalActive() && WiFi.status() == WL_CONNECTED &&
      time(nullptr) >= 1704067200 &&
      (retryDelay == 0 || millis() - lastWeatherAttempt >= retryDelay)) {
    weatherInFlight = requestWeather(config, configGeneration);
    if (weatherInFlight && initialWeatherPending && !initialWeatherStarted) {
      initialWeatherStarted = true;
      initialWeatherStartedAt = millis();
    }
  }
}

void processSerial() {
  static char command[16] = "";
  static size_t length = 0;
  while (Serial.available()) {
    const char c = Serial.read();
    lastSerialInput = millis();
    if (c == '\r' || c == '\n') {
      command[length] = '\0';
      if (strcmp(command, "setup") == 0) startSetupPortal();
      length = 0;
    } else if (length < sizeof(command) - 1) {
      command[length++] = c;
    } else {
      length = 0;
    }
  }
  if (millis() - lastSerialInput > 10000) length = 0;
}

void setup() {
  Serial.begin(115200);
  ledMatrix.begin();
  ledMatrix.setIntensity(15);
  WM_setup();
  applyConfiguration();
  weatherReady = beginWeather();
  if (!weatherReady) showNotice("Weather task could not start. Clock still available.");
}

void loop() {
  WM_loop();
  processSerial();
  updateStartupSync();
  updateWeather();
  updateDisplay();
  delay(1);
}
