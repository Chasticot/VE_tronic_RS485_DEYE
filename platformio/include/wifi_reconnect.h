#pragma once

#include <WiFi.h>
#include <AutoConnectCredential.h>
#include <esp_wifi.h>
#include <cstring>

// Les relances AutoConnect passent par une attente de connexion bloquante.
// Cette surveillance ne lance que des connexions asynchrones, sans redemarrage.
class WifiReconnect {
 public:
  static constexpr uint32_t retryIntervalMs = 30000U;

  void begin(const char* defaultSsid, const char* defaultPassword) {
    WiFi.setAutoReconnect(true);
    noteAttempt();
    if (!ssid_[0]) {
      if (defaultSsid && defaultSsid[0]) {
        copyCredentials(defaultSsid, defaultPassword ? defaultPassword : "");
      } else {
        // AutoConnect peut avoir efface la configuration STA si le routeur
        // etait absent au demarrage. Garder un reseau memorise a reessayer.
        AutoConnectCredential credentials;
        station_config_t saved = {};
        if (credentials.load(static_cast<int8_t>(0), &saved)) {
          memcpy(ssid_, saved.ssid, sizeof(saved.ssid));
          memcpy(password_, saved.password, sizeof(saved.password));
        }
      }
    }
    connected_ = WiFi.status() == WL_CONNECTED;
  }

  // A appeler aussi apres une connexion demandee depuis la page Wi-Fi :
  // le nouveau choix devient la cible, avec 30 s pour terminer sa tentative.
  void noteAttempt() {
    wifi_config_t current = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &current) == ESP_OK && current.sta.ssid[0]) {
      memcpy(ssid_, current.sta.ssid, sizeof(current.sta.ssid));
      memcpy(password_, current.sta.password, sizeof(current.sta.password));
    }
    lastAttemptAt_ = millis();
  }

  void tick() {
    const uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (!connected_) {
        noteAttempt();
        Serial.println("Wi-Fi reconnecte, IP: " + WiFi.localIP().toString());
      }
      connected_ = true;
      lastAttemptAt_ = now;
      return;
    }
    if (connected_) {
      connected_ = false;
      lastAttemptAt_ = now;
      Serial.println("Wi-Fi perdu, reconnexion automatique toutes les 30 s.");
    }
    if (!ssid_[0] || uint32_t(now - lastAttemptAt_) < retryIntervalMs) return;
    lastAttemptAt_ = now;

    // Ne couper que la station : conserver le SoftAP et les identifiants.
    // Canal et BSSID libres pour accepter les changements apres une MAJ box.
    Serial.println("Wi-Fi: nouvelle tentative de connexion.");
    WiFi.disconnect(false, false);
    WiFi.begin(ssid_, password_[0] ? password_ : nullptr, 0, nullptr);
  }

 private:
  void copyCredentials(const char* ssid, const char* password) {
    strncpy(ssid_, ssid, sizeof(ssid_) - 1);
    strncpy(password_, password, sizeof(password_) - 1);
  }

  char ssid_[33] = {};
  char password_[65] = {};
  uint32_t lastAttemptAt_ = 0;
  bool connected_ = false;
};
