#pragma once

#include <stdint.h>
#include "TerryData.h"

enum class WeatherStatus : uint8_t {
  Success, Offline, WaitingForTime, InvalidSettings, RateLimited,
  NetworkError, HttpError, InvalidResponse
};

struct WeatherRequest {
  AppConfig config;
  uint32_t generation;
};

struct WeatherResult {
  WeatherData data;
  WeatherStatus status = WeatherStatus::NetworkError;
  int httpCode = 0;
  uint32_t generation = 0;
};

bool beginWeather();
bool requestWeather(const AppConfig &config, uint32_t generation);
bool receiveWeather(WeatherResult &result);
