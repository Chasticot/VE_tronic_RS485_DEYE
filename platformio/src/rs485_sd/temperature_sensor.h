#pragma once

#if ENABLE_HISTORY_DS18B20 && __has_include(<OneWire.h>) && __has_include(<DallasTemperature.h>)
#include <OneWire.h>
#include <DallasTemperature.h>
#define HISTORY_DS18B20_AVAILABLE 1

class HistoryTemperatureSensor {
  OneWire oneWire;
  DallasTemperature sensors;
  uint32_t requestedAt=0;
  bool waiting=false, validValue=false;
  float lastValue=0;
public:
  HistoryTemperatureSensor():oneWire(HISTORY_TEMP_PIN),sensors(&oneWire) {}
  void begin() {
    sensors.begin();
    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();
    requestedAt=millis();
    waiting=true;
  }
  void tick() {
    if (!waiting || uint32_t(millis()-requestedAt)<800U) return;
    float value=sensors.getTempCByIndex(0);
    validValue=value>-55.0f && value<125.0f && value!=DEVICE_DISCONNECTED_C;
    if (validValue) lastValue=value;
    sensors.requestTemperatures();
    requestedAt=millis();
  }
  bool valid() const { return validValue; }
  float value() const { return lastValue; }
  const char *status() const { return validValue?"DS18B20 opérationnelle":"DS18B20 absente ou invalide"; }
};

#else
#define HISTORY_DS18B20_AVAILABLE 0
class HistoryTemperatureSensor {
public:
  void begin() {}
  void tick() {}
  bool valid() const { return false; }
  float value() const { return 0; }
  const char *status() const {
    return ENABLE_HISTORY_DS18B20?"Bibliothèques OneWire/DallasTemperature absentes":"Sonde DS18B20 désactivée";
  }
};
#endif

