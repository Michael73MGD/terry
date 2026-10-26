#pragma once

#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstring>

// These types and validation functions also compile on a computer for regression tests.
struct AppConfig {
  char apiKey[50] = "";
  char zipCode[11] = "";
  char timeZone[40] = "America/New_York";
  bool use24Hour = false;
};

struct TimeZone {
  const char *name;
  const char *rule;
  const char *label;
};

inline constexpr TimeZone TIME_ZONES[] = {
  {"America/New_York", "EST5EDT,M3.2.0/2,M11.1.0/2", "Eastern (New York)"},
  {"America/Chicago", "CST6CDT,M3.2.0/2,M11.1.0/2", "Central (Chicago)"},
  {"America/Denver", "MST7MDT,M3.2.0/2,M11.1.0/2", "Mountain (Denver)"},
  {"America/Los_Angeles", "PST8PDT,M3.2.0/2,M11.1.0/2", "Pacific (Los Angeles)"},
  {"America/Phoenix", "MST7", "Arizona (no daylight saving)"},
  {"America/Anchorage", "AKST9AKDT,M3.2.0/2,M11.1.0/2", "Alaska (Anchorage)"},
  {"Pacific/Honolulu", "HST10", "Hawaii (no daylight saving)"},
  {"UTC", "UTC0", "UTC"}
};

inline const char *timeZoneRule(const char *name) {
  if (name) {
    for (const auto &zone : TIME_ZONES) {
      if (strcmp(name, zone.name) == 0) return zone.rule;
    }
  }
  return nullptr;
}

inline bool validApiKey(const char *value) {
  if (!value || !*value || strlen(value) >= sizeof(AppConfig().apiKey)) return false;
  for (const char *p = value; *p; ++p) {
    if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
          (*p >= '0' && *p <= '9'))) return false;
  }
  return true;
}

inline bool validZipCode(const char *value) {
  if (!value) return false;
  const size_t length = strlen(value);
  if (length != 5 && length != 10) return false;
  for (size_t i = 0; i < length; ++i) {
    if (i == 5) {
      if (value[i] != '-') return false;
    } else if (value[i] < '0' || value[i] > '9') {
      return false;
    }
  }
  return true;
}

inline bool readConfig(JsonVariantConst json, AppConfig &destination) {
  const char *key = json["api_key"].as<const char *>();
  const char *zip = json["zip_code"].as<const char *>();
  // Older configuration files have no timezone; preserve their Eastern default.
  const char *zone = json["time_zone"].isNull()
                       ? "America/New_York" : json["time_zone"].as<const char *>();
  const char *format = json["clock_format"].isNull()
                         ? "12" : json["clock_format"].as<const char *>();
  if (!validApiKey(key) || !validZipCode(zip) || !timeZoneRule(zone)) return false;
  if (!format || (strcmp(format, "12") != 0 && strcmp(format, "24") != 0)) return false;

  AppConfig candidate;
  snprintf(candidate.apiKey, sizeof(candidate.apiKey), "%s", key);
  snprintf(candidate.zipCode, sizeof(candidate.zipCode), "%s", zip);
  snprintf(candidate.timeZone, sizeof(candidate.timeZone), "%s", zone);
  candidate.use24Hour = strcmp(format, "24") == 0;
  destination = candidate;
  return true;
}

struct WeatherData {
  float temperature = NAN;
  float precipitationProbability = NAN;
  char date[11] = "";
};

inline bool readWeather(JsonVariantConst json, WeatherData &destination) {
  JsonVariantConst temp = json["currentConditions"]["temp"];
  JsonVariantConst precip = json["days"][0]["precipprob"];
  const char *date = json["days"][0]["datetime"].as<const char *>();
  if (!temp.is<float>() || !precip.is<float>() || !date || strlen(date) != 10) return false;
  for (size_t i = 0; i < 10; ++i) {
    if (i == 4 || i == 7) {
      if (date[i] != '-') return false;
    } else if (date[i] < '0' || date[i] > '9') {
      return false;
    }
  }
  const float temperature = temp.as<float>();
  const float probability = precip.as<float>();
  if (!std::isfinite(temperature) || !std::isfinite(probability) ||
      temperature < -150 || temperature > 160 || probability < 0 || probability > 100) return false;

  WeatherData candidate;
  candidate.temperature = temperature;
  candidate.precipitationProbability = probability;
  snprintf(candidate.date, sizeof(candidate.date), "%s", date);
  destination = candidate;
  return true;
}

inline unsigned long weatherRetryDelay(unsigned long previousDelay) {
  const unsigned long minimum = 60UL * 1000;
  const unsigned long maximum = 15UL * 60 * 1000;
  if (previousDelay < minimum) return minimum;
  if (previousDelay >= maximum / 2) return maximum;
  return previousDelay * 2;
}
