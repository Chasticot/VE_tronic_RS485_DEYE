#pragma once
#include <stdint.h>
#if __cplusplus >= 201402L
#define SOLAR_CONSTEXPR constexpr
#else
#define SOLAR_CONSTEXPR
#endif

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
  bool running = false, bridging = false, recoveryRequired = false, probing = false, exporting = false;
  uint32_t deficitSince = 0, probeStarted = 0, probeCooldownUntil = 0, exportSince = 0;
  SOLAR_CONSTEXPR void reset() { running = bridging = recoveryRequired = probing = exporting = false; deficitSince = 0; }
  SOLAR_CONSTEXPR int decide(uint32_t now, bool valid, int surplusW, int batteryW,
             int gridW, int volts, int maxA) {
    if (!valid || volts < 180 || volts > 260 || maxA < 6) {
      // Coupe toute charge/sonde en cours par sécurité, mais un simple glitch de
      // lecture (WB ou Deye) ne doit pas effacer le chrono d'export accumulé :
      // sinon la sonde ne peut jamais atteindre ses 2 min sur une liaison série
      // instable, alors même que le déblocage de la sonde sert justement à
      // corriger une situation où le surplus mesuré est artificiellement bas.
      running = bridging = probing = false; deficitSince = 0;
      recoveryRequired = true;
      return 0;
    }
    if (!running) {
      if (probing) {
        if (surplusW >= 1800 && surplusW / volts >= 6) {
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
        bool exportNow = batteryW <= 100 && -gridW >= 200;
        if (exportNow) {
          if (!exporting) { exporting = true; exportSince = now; }
          if (!probing && (uint32_t)(now - exportSince) >= 120000U && (int32_t)(now - probeCooldownUntil) >= 0) {
            probing = true; probeStarted = now; exporting = false; recoveryRequired = false; return 6;
          }
        } else {
          exporting = false;
        }
        if (recoveryRequired && (surplusW < 1800 || batteryW > 100)) return 0;
        recoveryRequired = false;
        if (surplusW < 1800 || surplusW / volts < 6) return 0;
        running = true;
      }
    }
    // A deficit or actual battery discharge starts one continuous five-minute window.
    bool deficit = surplusW < 6 * volts || batteryW > 100;
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
    int amps = surplusW / volts;
    // Reduce to minimum during a cloud, never hold a high battery-backed current.
    if (amps < 6) amps = 6;
    if (amps > maxA) amps = maxA;
    return amps;
  }
};