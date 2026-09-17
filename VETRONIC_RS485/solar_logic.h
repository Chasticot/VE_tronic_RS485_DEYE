#pragma once
#include <stdint.h>
#if __cplusplus >= 201402L
#define SOLAR_CONSTEXPR constexpr
#else
#define SOLAR_CONSTEXPR
#endif

// Marge de conversion et d'écart de mesure avant de demander une puissance à
// la borne. L'hystérésis évite de relancer une temporisation sur le bruit.
static constexpr int SOLAR_RESERVE_W = 200;
static constexpr int SOLAR_DISCHARGE_START_W = 300;
static constexpr int SOLAR_DISCHARGE_CLEAR_W = 150;

// Tracks a continuous loss of Deye telemetry separately from a solar deficit.
struct DeyeLossTimeout {
  bool lost = false;
  uint32_t since = 0;
  SOLAR_CONSTEXPR void reset() { lost = false; since = 0; }
  SOLAR_CONSTEXPR void update(uint32_t now, bool fresh) {
    if (fresh) reset();
    else if (!lost) { lost = true; since = now; }
  }
  SOLAR_CONSTEXPR uint32_t remaining(uint32_t now) const {
    return !lost || uint32_t(now-since) >= 300000U ? 0 : 300000U-uint32_t(now-since);
  }
  SOLAR_CONSTEXPR int current(uint32_t now, bool wbValid, bool running,
                              int previousA, bool bridging, uint32_t deficitSince) const {
    if (!lost || !remaining(now) || !wbValid || !running || previousA < 6) return 0;
    // A previously observed battery deficit keeps its original five-minute deadline.
    if (bridging && uint32_t(now-deficitSince) >= 300000U) return 0;
    return previousA;
  }
};

// Pure decision logic, independent of networking and Arduino.
struct SolarLogic {
  bool running = false, bridging = false, recoveryRequired = false, probing = false, exporting = false, batteryDeficit = false;
  uint32_t deficitSince = 0, probeStarted = 0, probeCooldownUntil = 0, exportSince = 0;
  SOLAR_CONSTEXPR void reset() { running = bridging = recoveryRequired = probing = exporting = batteryDeficit = false; deficitSince = 0; }
  SOLAR_CONSTEXPR int decide(uint32_t now, bool valid, int surplusW, int batteryW,
             int gridW, int volts, int maxA) {
    if (volts < 180 || volts > 260) volts = 230;
    if (!valid || maxA < 6) {
      // Coupe toute charge/sonde en cours par sécurité, mais un simple glitch de
      // lecture (WB ou Deye) ne doit pas effacer le chrono d'export accumulé :
      // sinon la sonde ne peut jamais atteindre ses 2 min sur une liaison série
      // instable, alors même que le déblocage de la sonde sert justement à
      // corriger une situation où le surplus mesuré est artificiellement bas.
      running = bridging = probing = batteryDeficit = false; deficitSince = 0;
      recoveryRequired = true;
      return 0;
    }
    const int availableW = surplusW > SOLAR_RESERVE_W ? surplusW - SOLAR_RESERVE_W : 0;
    if (batteryW > SOLAR_DISCHARGE_START_W) batteryDeficit = true;
    else if (batteryW <= SOLAR_DISCHARGE_CLEAR_W) batteryDeficit = false;
    if (!running) {
      if (probing) {
        if (surplusW >= 1800 && availableW / volts >= 6) {
          // La production s'est débridée : bascule directe en fonctionnement normal.
          probing = false; exporting = false; recoveryRequired = false; running = true;
        } else if ((uint32_t)(now - probeStarted) >= 120000U) { // 2 min de sonde écoulées
          probing = false; probeCooldownUntil = now + 300000U; return 0; // 5 min avant de retenter
        } else {
          return 6; // Poursuite de la sonde à courant minimal.
        }
      }
      if (!running) {
        // Le déclenchement de la sonde passe AVANT le verrou recoveryRequired :
        // sinon, une lecture invalide ponctuelle qui active recoveryRequired
        // bloquerait indéfiniment la sonde, puisque recoveryRequired n'est levé
        // normalement qu'en atteignant 1800 W — exactement ce que la sonde sert
        // à débloquer quand la production est bridée faute de consommateur local.
        bool exportNow = batteryW <= SOLAR_DISCHARGE_CLEAR_W && -gridW >= 200;
        if (exportNow) {
          if (!exporting) { exporting = true; exportSince = now; }
          if (!probing && (uint32_t)(now - exportSince) >= 120000U && (int32_t)(now - probeCooldownUntil) >= 0) {
            probing = true; probeStarted = now; exporting = false; recoveryRequired = false; return 6;
          }
        } else {
          exporting = false;
        }
        if (recoveryRequired && (surplusW < 1800 || batteryW > SOLAR_DISCHARGE_CLEAR_W)) return 0;
        recoveryRequired = false;
        if (surplusW < 1800 || availableW / volts < 6) return 0;
        running = true;
      }
    }
    // La marge de calcul seule ne doit pas déclencher l'appoint batterie.
    bool deficit = surplusW < 6 * volts || batteryDeficit;
    if (deficit) {
      if (!bridging) { bridging = true; deficitSince = now; }
      if ((uint32_t)(now - deficitSince) >= 300000U) {
        reset(); recoveryRequired = true; return 0;
      }
      // No grid fallback when the battery cannot supply the minimum charge.
      if (gridW > 200 && surplusW < 6 * volts) { reset(); recoveryRequired = true; return 0; }
    } else {
      bridging = false;
    }
    int amps = availableW / volts;
    // Reduce to minimum during a cloud, never hold a high battery-backed current.
    if (amps < 6) amps = 6;
    if (amps > maxA) amps = maxA;
    return amps;
  }
};
