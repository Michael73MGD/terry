#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <time.h>
#include <memory>
#include <new>
#include "Weather.h"

namespace {
QueueHandle_t requests = nullptr;
QueueHandle_t results = nullptr;
constexpr size_t MAX_RESPONSE_BYTES = 8192;

// HTTPClient decodes chunked responses into this bounded sink.
class ResponseBody : public Stream {
public:
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t *data, size_t size) override {
    if (size > sizeof(buffer) - length) return 0;
    memcpy(buffer + length, data, size);
    length += size;
    return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  char buffer[MAX_RESPONSE_BYTES];
  size_t length = 0;
};

WeatherResult fetchWeather(const WeatherRequest &request) {
  WeatherResult result;
  result.generation = request.generation;
  if (WiFi.status() != WL_CONNECTED) {
    result.status = WeatherStatus::Offline;
    return result;
  }
  // Certificate validity checks need a synchronized clock.
  if (time(nullptr) < 1704067200) {
    result.status = WeatherStatus::WaitingForTime;
    return result;
  }

  WiFiClientSecure client;
  client.useBuiltinCACertBundle();
  client.setHandshakeTimeout(8); // seconds
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setReuse(false);
  const String endpoint =
    String("https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/") +
    request.config.zipCode + ",US/today?unitGroup=us&include=current,days,hours" +
    "&elements=datetime,temp,precipprob,preciptype&contentType=json&key=" + request.config.apiKey;
  if (!http.begin(client, endpoint)) return result;

  result.httpCode = http.GET();
  if (result.httpCode != HTTP_CODE_OK) {
    if (result.httpCode == 400 || result.httpCode == 401 || result.httpCode == 403) {
      result.status = WeatherStatus::InvalidSettings;
    } else if (result.httpCode == 429) {
      result.status = WeatherStatus::RateLimited;
    } else {
      result.status = result.httpCode < 0 ? WeatherStatus::NetworkError : WeatherStatus::HttpError;
    }
    http.end();
    return result;
  }

  // Hourly forecasts need a larger bounded body. Keep it off the task stack
  // so TLS processing still has the same amount of stack available.
  std::unique_ptr<ResponseBody> body(new (std::nothrow) ResponseBody);
  if (!body) {
    http.end();
    return result;
  }
  const int bodyLength = http.getSize();
  if (bodyLength > static_cast<int>(MAX_RESPONSE_BYTES) || http.writeToStream(body.get()) < 0) {
    http.end();
    result.status = WeatherStatus::InvalidResponse;
    return result;
  }
  http.end();

  JsonDocument filter;
  configureWeatherFilter(filter);
  JsonDocument json;
  const auto error = deserializeJson(json, body->buffer, body->length,
                                    DeserializationOption::Filter(filter),
                                    DeserializationOption::NestingLimit(8));
  result.status = !error && readWeather(json.as<JsonVariantConst>(), result.data)
                    ? WeatherStatus::Success : WeatherStatus::InvalidResponse;
  return result;
}

void weatherTask(void *) {
  WeatherRequest request;
  for (;;) {
    if (xQueueReceive(requests, &request, portMAX_DELAY) == pdTRUE) {
      const WeatherResult result = fetchWeather(request);
      xQueueOverwrite(results, &result);
    }
  }
}
} // namespace

bool beginWeather() {
  requests = xQueueCreate(1, sizeof(WeatherRequest));
  results = xQueueCreate(1, sizeof(WeatherResult));
  if (requests && results &&
      xTaskCreate(weatherTask, "weather", 16384, nullptr, 1, nullptr) == pdPASS) return true;
  if (requests) vQueueDelete(requests);
  if (results) vQueueDelete(results);
  requests = results = nullptr;
  return false;
}

bool requestWeather(const AppConfig &config, uint32_t generation) {
  const WeatherRequest request{config, generation};
  return requests && xQueueSend(requests, &request, 0) == pdTRUE;
}

bool receiveWeather(WeatherResult &result) {
  return results && xQueueReceive(results, &result, 0) == pdTRUE;
}
