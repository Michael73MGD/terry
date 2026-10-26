#pragma once

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <vector>

using String = std::string;
inline uint32_t fakeMillis = 0;
inline unsigned long millis() { return fakeMillis; }
inline void delay(unsigned long value) { fakeMillis += value; }
inline time_t fakeEpoch = 1792929600;
inline time_t fakeTime(time_t *destination) {
  if (destination) *destination = fakeEpoch;
  return fakeEpoch;
}
inline std::string appliedTimeZone;
inline unsigned int timeSyncRequests = 0;
inline void configTzTime(const char *zone, const char *, const char *) {
  ++timeSyncRequests;
  appliedTimeZone = zone;
  setenv("TZ", zone, 1);
  tzset();
}

struct FakeSerial {
  std::vector<std::string> messages;
  void begin(int) {}
  int available() { return 0; }
  char read() { return 0; }
  void println(const char *text) { messages.emplace_back(text); }
  void printf(const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    messages.emplace_back(buffer);
  }
};
inline FakeSerial Serial;

constexpr int WL_CONNECTED = 3;
constexpr int WIFI_STA = 1;
struct FakeWiFi {
  int connection = 0;
  unsigned int beginCalls = 0;
  int status() { return connection; }
  void mode(int) {}
  void setAutoReconnect(bool) {}
  void begin() { ++beginCalls; }
};
inline FakeWiFi WiFi;

class WiFiManagerParameter {
public:
  explicit WiFiManagerParameter(const char *) {}
  WiFiManagerParameter(const char *, const char *, const char *initial, int length,
                       const char * = nullptr) { setValue(initial, length); }
  void setValue(const char *text, int length) { value = std::string(text).substr(0, length); }
  const char *getValue() { return value.c_str(); }
  virtual const char *getCustomHTML() const { return ""; }
private:
  std::string value;
};

struct FakeWebServer {
  std::map<std::string, std::string> arguments;
  String arg(const char *name) { return arguments[name]; }
};

class WiFiManager {
public:
  FakeWebServer http;
  FakeWebServer *server = &http;
  bool active = false;
  void (*paramsCallback)() = nullptr;
  void (*wifiCallback)() = nullptr;
  void process() {}
  bool getConfigPortalActive() { return active; }
  void startConfigPortal(const char *) { active = true; }
  void stopConfigPortal() { active = false; }
  void setDebugOutput(bool) {}
  void setConfigPortalBlocking(bool) {}
  void setConnectTimeout(int) {}
  void setDisableConfigPortal(bool) {}
  void setSaveParamsCallback(void (*callback)()) { paramsCallback = callback; }
  void setSaveConfigCallback(void (*callback)()) { wifiCallback = callback; }
  void setParamsPage(bool) {}
  void setCustomMenuHTML(const char *html) { menuInstructions = html; }
  const char *menuInstructions = "";
  void setMenu(const char **, int) {}
  void addParameter(WiFiManagerParameter *) {}
};

class Preferences {
public:
  bool writable = true;
  bool configWritable = true;
  std::map<std::string, std::string> strings;
  std::map<std::string, bool> flags;
  bool begin(const char *, bool) { return true; }
  bool isKey(const char *key) { return strings.count(key); }
  String getString(const char *key) { return strings[key]; }
  size_t putString(const char *key, const String &value) {
    if (!writable || !configWritable) return 0;
    strings[key] = value;
    return value.length();
  }
  bool getBool(const char *key, bool fallback) {
    return flags.count(key) ? flags[key] : fallback;
  }
  size_t putBool(const char *key, bool value) {
    if (!writable) return 0;
    flags[key] = value;
    return 1;
  }
};

struct File {
  explicit operator bool() const { return false; }
  size_t size() { return 0; }
  int read() { return -1; }
  size_t readBytes(char *, size_t) { return 0; }
  void close() {}
};
struct FakeSPIFFS {
  bool begin(bool) { return false; }
  File open(const char *, const char *) { return File(); }
  void end() {}
};
inline FakeSPIFFS SPIFFS;

#define PROGMEM
inline uint8_t pgm_read_byte(const uint8_t *value) { return *value; }
struct MD_MAX72XX {
  using fontType_t = uint8_t;
  enum { FC16_HW };
};
enum { PA_LEFT, PA_SCROLL_LEFT, PA_NO_EFFECT, PA_PRINT };
struct MD_Parola {
  bool animationDone = true;
  const char *retainedText = nullptr;
  const uint8_t *font = nullptr;
  int entranceEffect = PA_NO_EFFECT;
  unsigned int displayCalls = 0;
  MD_Parola(int, int, int) {}
  void begin() {}
  void setIntensity(int) {}
  void setFont(const uint8_t *value) { font = value; }
  void displayClear() {}
  bool displayAnimate() { return animationDone; }
  void displayText(const char *text, int, int, int, int effect, int) {
    retainedText = text;
    entranceEffect = effect;
    ++displayCalls;
  }
};
