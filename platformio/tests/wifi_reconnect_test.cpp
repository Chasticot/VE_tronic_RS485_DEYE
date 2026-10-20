#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

enum { WL_IDLE_STATUS, WL_NO_SSID_AVAIL, WL_CONNECTED, WL_DISCONNECTED };
constexpr int WIFI_IF_STA = 0;
constexpr int ESP_OK = 0;
struct wifi_config_t {
  struct { uint8_t ssid[32] = {}; uint8_t password[64] = {}; } sta;
};
struct station_config_t { uint8_t ssid[32] = {}; uint8_t password[64] = {}; };
uint32_t fakeNow = 0;
uint32_t millis() { return fakeNow; }
wifi_config_t currentConfig;
station_config_t savedConfig;
bool hasSavedConfig = false;
int esp_wifi_get_config(int, wifi_config_t* config) { *config = currentConfig; return ESP_OK; }
struct AutoConnectCredential {
  bool load(int8_t, station_config_t* config) {
    if (!hasSavedConfig) return false;
    *config = savedConfig;
    return true;
  }
};
struct FakeSerial { void println(const std::string&) {} } Serial;
struct FakeIp { std::string toString() { return "192.168.1.50"; } };
struct FakeWifi {
  int state = WL_DISCONNECTED;
  int attempts = 0;
  bool autoReconnect = false;
  bool apActive = true;
  std::string lastSsid, lastPassword;
  void setAutoReconnect(bool enabled) { autoReconnect = enabled; }
  int status() { return state; }
  FakeIp localIP() { return {}; }
  void disconnect(bool off, bool erase) {
    assert(!off && !erase);
    state = WL_DISCONNECTED;
  }
  void begin(const char* ssid, const char* password, int channel, const uint8_t* bssid) {
    assert(channel == 0 && bssid == nullptr);
    lastSsid = ssid;
    lastPassword = password ? password : "";
    ++attempts;
    // The driver returns immediately while association continues in background.
    state = WL_IDLE_STATUS;
    currentConfig = {};
    memcpy(currentConfig.sta.ssid, ssid, lastSsid.size());
    memcpy(currentConfig.sta.password, lastPassword.data(), lastPassword.size());
  }
} WiFi;

// The runner copies the production header, removing only its hardware includes.
#include "wifi_reconnect_under_test.h"

void reset() {
  WiFi = {};
  fakeNow = 0;
  currentConfig = {};
  savedConfig = {};
  hasSavedConfig = false;
}
void configure(const char* ssid, const char* password = "password") {
  currentConfig = {};
  memcpy(currentConfig.sta.ssid, ssid, strlen(ssid));
  memcpy(currentConfig.sta.password, password, strlen(password));
}

int main() {
  reset();
  configure("Router");
  WiFi.state = WL_CONNECTED;
  WifiReconnect recovery;
  recovery.begin("Default", "default-password");
  assert(WiFi.autoReconnect);
  // Simulate a router unavailable for ten minutes, including NO_SSID states.
  fakeNow = 100;
  WiFi.state = WL_NO_SSID_AVAIL;
  recovery.tick();
  fakeNow = 30099;
  recovery.tick();
  assert(WiFi.attempts == 0);
  for (int i = 1; i <= 20; ++i) {
    fakeNow = 100 + i * 30000;
    WiFi.state = WL_NO_SSID_AVAIL;
    recovery.tick();
    assert(WiFi.attempts == i);
    assert(WiFi.lastSsid == "Router" && WiFi.apActive);
  }
  WiFi.state = WL_CONNECTED;
  recovery.tick();
  fakeNow += 60000;
  recovery.tick();
  assert(WiFi.attempts == 20);

  // A manual network change must not be replaced by the former router.
  configure("NewRouter", "new-password");
  WiFi.state = WL_IDLE_STATUS;
  recovery.noteAttempt();
  recovery.tick();
  fakeNow += 29999;
  recovery.tick();
  assert(WiFi.attempts == 20);
  ++fakeNow;
  recovery.tick();
  assert(WiFi.attempts == 21 && WiFi.lastSsid == "NewRouter");
  assert(WiFi.lastPassword == "new-password");

  // Startup with the router already absent and the SDK config cleared.
  reset();
  WifiReconnect bootDefault;
  bootDefault.begin("Default", "default-password");
  fakeNow = 30000;
  bootDefault.tick();
  assert(WiFi.lastSsid == "Default");
  reset();
  hasSavedConfig = true;
  memcpy(savedConfig.ssid, "SavedRouter", 11);
  WifiReconnect bootSaved;
  bootSaved.begin("", "");
  fakeNow = 30000;
  bootSaved.tick();
  assert(WiFi.lastSsid == "SavedRouter" && WiFi.lastPassword.empty());
  reset();
  WifiReconnect unconfigured;
  unconfigured.begin("", "");
  fakeNow = 60000;
  unconfigured.tick();
  assert(WiFi.attempts == 0);

  // Maximum length SDK credentials need explicit trailing NUL bytes.
  reset();
  memset(currentConfig.sta.ssid, 's', 32);
  memset(currentConfig.sta.password, 'p', 64);
  WifiReconnect fullLength;
  fullLength.begin("", "");
  fakeNow = 30000;
  fullLength.tick();
  assert(WiFi.lastSsid.size() == 32 && WiFi.lastPassword.size() == 64);

  // The retry interval must survive millis() wrapping after about 49 days.
  reset();
  configure("Router");
  fakeNow = UINT32_MAX - 10000;
  WifiReconnect wrapping;
  wrapping.begin("", "");
  fakeNow += 29999;
  wrapping.tick();
  assert(WiFi.attempts == 0);
  ++fakeNow;
  wrapping.tick();
  assert(WiFi.attempts == 1);
  std::cout << "Wi-Fi recovery: all scenarios passed\n";
}
