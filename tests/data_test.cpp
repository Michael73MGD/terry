#include <cassert>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include "TerryData.h"

void testConfig() {
  AppConfig config;
  JsonDocument json;
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["api_key"] = "ExampleKey123";
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["zip_code"] = "01234";
  assert(readConfig(json.as<JsonVariantConst>(), config));
  assert(strcmp(config.zipCode, "01234") == 0); // Preserve leading zeroes.
  assert(strcmp(config.timeZone, "America/New_York") == 0); // Legacy migration.
  assert(!config.use24Hour); // Older settings keep the original 12-hour clock.
  json["clock_format"] = "24";
  assert(readConfig(json.as<JsonVariantConst>(), config));
  assert(config.use24Hour);
  json["clock_format"] = "12";
  assert(readConfig(json.as<JsonVariantConst>(), config));
  assert(!config.use24Hour);
  for (const char *invalid : {"", "25", "true", "24-hour"}) {
    json["clock_format"] = invalid;
    assert(!readConfig(json.as<JsonVariantConst>(), config));
  }
  json["clock_format"] = true;
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["clock_format"] = 24;
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["clock_format"] = "12";
  json["time_zone"] = "UTC";
  assert(readConfig(json.as<JsonVariantConst>(), config));
  const AppConfig previous = config;
  json["api_key"] = std::string(50, 'A');
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  assert(strcmp(config.apiKey, previous.apiKey) == 0);
  assert(strcmp(config.timeZone, previous.timeZone) == 0);
  json["api_key"] = std::string(49, 'A');
  assert(readConfig(json.as<JsonVariantConst>(), config));
  assert(strlen(config.apiKey) == 49);
  assert(config.apiKey[49] == '\0');
  json["api_key"] = 123;
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["api_key"] = nullptr;
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["api_key"] = "key&include=hours";
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["api_key"] = "ValidKey";
  json["zip_code"] = 12345;
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["zip_code"] = "12345-6789";
  assert(readConfig(json.as<JsonVariantConst>(), config));
  json["zip_code"] = "12345-67890";
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["zip_code"] = "12345/";
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["zip_code"] = "12345";
  json["time_zone"] = "Unknown/Zone";
  assert(!readConfig(json.as<JsonVariantConst>(), config));
  json["time_zone"] = 123;
  assert(!readConfig(json.as<JsonVariantConst>(), config));
}

void testWeather() {
  JsonDocument json;
  WeatherData weather;
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  assert(std::isnan(weather.temperature));
  assert(!deserializeJson(json, R"({"currentConditions":{"temp":0},"days":[{"precipprob":0,"datetime":"2026-10-25","hours":[{"precipprob":0,"preciptype":null}]}]})"));
  assert(readWeather(json.as<JsonVariantConst>(), weather));
  assert(weather.temperature == 0); // Real zero differs from a missing value.
  json["currentConditions"]["temp"] = -14;
  json["days"][0]["precipprob"] = 75.5;
  assert(readWeather(json.as<JsonVariantConst>(), weather));
  assert(weather.temperature == -14);
  assert(weather.precipitationProbability == 75.5);
  json["currentConditions"].remove("temp");
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  assert(weather.temperature == -14); // Failed updates preserve last good data.
  json["currentConditions"]["temp"] = "72";
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["currentConditions"]["temp"] = nullptr;
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["currentConditions"]["temp"] = 72;
  json["days"][0]["precipprob"] = 101;
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0]["precipprob"] = -1;
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0]["precipprob"] = 100;
  assert(readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0].remove("precipprob");
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0]["precipprob"] = 50;
  json["days"][0]["datetime"] = "bad-date";
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"].clear();
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
}

void testSnowForecast() {
  JsonDocument json;
  assert(!deserializeJson(json, R"({
    "currentConditions":{"temp":28},
    "days":[{"datetime":"2026-10-25","precipprob":95,"preciptype":["rain","snow"],
      "hours":[{"precipprob":95,"preciptype":["rain"]},
               {"precipprob":20,"preciptype":["snow"]}]}]
  })"));
  WeatherData weather;
  assert(readWeather(json.as<JsonVariantConst>(), weather));
  assert(weather.snowProbability == 20);
  assert(precipitationIcon(weather) == PrecipitationIcon::Rain); // Heavy rain + unlikely snow is still rain.
  json["days"][0]["hours"][1]["precipprob"] = 50;
  assert(readWeather(json.as<JsonVariantConst>(), weather));
  assert(precipitationIcon(weather) == PrecipitationIcon::Rain);
  json["days"][0]["hours"][1]["precipprob"] = 50.1;
  assert(readWeather(json.as<JsonVariantConst>(), weather));
  assert(precipitationIcon(weather) == PrecipitationIcon::Snow); // Snow overrides the 95% rain hour.
  json["days"][0]["hours"][1]["preciptype"][1] = "rain";
  assert(readWeather(json.as<JsonVariantConst>(), weather));
  assert(precipitationIcon(weather) == PrecipitationIcon::Snow); // Mixed precipitation within one hour too.

  json["days"][0]["hours"][1]["precipprob"] = nullptr;
  assert(readWeather(json.as<JsonVariantConst>(), weather));
  assert(weather.snowProbability == 0); // Missing probability never invents a high snow chance.
  assert(precipitationIcon(weather) == PrecipitationIcon::Rain);
  json["days"][0]["hours"][1]["precipprob"] = 101;
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  assert(weather.snowProbability == 0); // Invalid updates leave last good data alone.
  json["days"][0]["hours"][1]["precipprob"] = -1;
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0]["hours"][1]["precipprob"] = "75";
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0]["hours"][1]["precipprob"] = 75;
  json["days"][0]["hours"][1]["preciptype"] = "snow";
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0]["hours"][1]["preciptype"].clear();
  json["days"][0]["hours"][1]["preciptype"][0] = 42;
  assert(!readWeather(json.as<JsonVariantConst>(), weather));

  json["days"][0]["hours"].clear();
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0].remove("hours");
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0]["hours"] = "snow";
  assert(!readWeather(json.as<JsonVariantConst>(), weather));
  json["days"][0]["hours"].clear();
  json["days"][0]["hours"][0] = 42;
  assert(!readWeather(json.as<JsonVariantConst>(), weather));

  // Keep all 25 hours on a daylight-saving fall-back day through the actual
  // production JSON filter, including snow late in the day and extra fields.
  json["days"][0]["hours"].clear();
  for (int hour = 0; hour < 25; ++hour) {
    auto item = json["days"][0]["hours"][hour];
    item["precipprob"] = hour == 24 ? 80 : 0;
    if (hour == 24) item["preciptype"][0] = "snow";
    item["temp"] = 28;
    item["datetime"] = "23:00:00";
  }
  JsonDocument filter;
  configureWeatherFilter(filter);
  std::string serialized;
  serializeJson(json, serialized);
  assert(serialized.size() < 8192);
  JsonDocument filtered;
  assert(!deserializeJson(filtered, serialized, DeserializationOption::Filter(filter),
                          DeserializationOption::NestingLimit(8)));
  assert(filtered["days"][0]["hours"].size() == 25);
  assert(filtered["days"][0]["hours"][0]["temp"].isNull());
  assert(readWeather(filtered.as<JsonVariantConst>(), weather));
  assert(weather.snowProbability == 80);
  assert(precipitationIcon(weather) == PrecipitationIcon::Snow);
  json["days"][0]["hours"][25]["precipprob"] = 80;
  assert(!readWeather(json.as<JsonVariantConst>(), weather));

  weather.snowProbability = 50;
  weather.precipitationProbability = 50;
  assert(precipitationIcon(weather) == PrecipitationIcon::None);
  weather.precipitationProbability = 50.1;
  assert(precipitationIcon(weather) == PrecipitationIcon::Rain);
  weather.snowProbability = 50.1;
  assert(precipitationIcon(weather) == PrecipitationIcon::Snow);
}

void expectLocalTime(time_t utc, int month, int day, int hour, int minute, bool dst) {
  const tm *local = localtime(&utc);
  assert(local && local->tm_mon == month - 1 && local->tm_mday == day);
  assert(local->tm_hour == hour && local->tm_min == minute);
  assert((local->tm_isdst > 0) == dst);
}

void testTimeZones() {
  setenv("TZ", timeZoneRule("America/New_York"), 1);
  tzset();
  // Check both sides of the 2026 spring and autumn transitions.
  expectLocalTime(1772953140, 3, 8, 1, 59, false);
  expectLocalTime(1772953200, 3, 8, 3, 0, true);
  expectLocalTime(1793512740, 11, 1, 1, 59, true);
  expectLocalTime(1793512800, 11, 1, 1, 0, false);
  setenv("TZ", timeZoneRule("America/Phoenix"), 1);
  tzset();
  expectLocalTime(1772953200, 3, 8, 0, 0, false);
  setenv("TZ", timeZoneRule("UTC"), 1);
  tzset();
  expectLocalTime(1772953200, 3, 8, 7, 0, false);
}

void testRetries() {
  unsigned long delay = 0;
  const unsigned long expected[] = {60000, 120000, 240000, 480000, 900000, 900000};
  for (const auto value : expected) {
    delay = weatherRetryDelay(delay);
    assert(delay == value);
  }
}

int main() {
  testConfig();
  testWeather();
  testSnowForecast();
  testTimeZones();
  testRetries();
  std::cout << "Configuration, weather, timezone and retry regression tests passed.\n";
}
