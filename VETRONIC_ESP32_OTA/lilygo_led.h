#pragma once
#include <stdint.h>

#if __cplusplus >= 201402L
#define LILYGO_LED_CONSTEXPR constexpr
#else
#define LILYGO_LED_CONSTEXPR inline
#endif

// Couleurs de la WS2812B intégrée à la LILYGO T-CAN485 (GPIO4).
// La sélection est isolée de l'écriture matérielle pour pouvoir la tester.
enum LilygoLedState : uint8_t {
  LILYGO_LED_STARTUP,
  LILYGO_LED_READY,
  LILYGO_LED_CHARGING,
  LILYGO_LED_FAULT
};

constexpr uint32_t LILYGO_LED_STARTUP_GRACE_MS = 20000U;
constexpr uint32_t LILYGO_LED_DEYE_FRESH_MS = 15000U;
constexpr uint32_t LILYGO_LED_WB_FRESH_MS = 10000U;

constexpr bool lilygoLedFresh(uint32_t now, bool valid, uint32_t at, uint32_t maxAge) {
  return valid && uint32_t(now-at)<=maxAge;
}

LILYGO_LED_CONSTEXPR LilygoLedState lilygoLedSelect(uint32_t now, uint32_t startedAt,
                                          bool deyeConfigured, bool deyeValid, uint32_t deyeAt,
                                          bool wbValid, uint32_t wbAt, bool vehicleCharging) {
  // Une configuration Deye absente est une attente de paramétrage, pas un défaut.
  if(!deyeConfigured || uint32_t(now-startedAt)<LILYGO_LED_STARTUP_GRACE_MS)
    return LILYGO_LED_STARTUP;
  if(!lilygoLedFresh(now,deyeValid,deyeAt,LILYGO_LED_DEYE_FRESH_MS) ||
     !lilygoLedFresh(now,wbValid,wbAt,LILYGO_LED_WB_FRESH_MS))
    return LILYGO_LED_FAULT;
  return vehicleCharging ? LILYGO_LED_CHARGING : LILYGO_LED_READY;
}
