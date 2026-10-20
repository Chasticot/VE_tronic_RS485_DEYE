#pragma once

#if __cplusplus >= 201402L
#define SOC_GUARD_CONSTEXPR constexpr
#else
#define SOC_GUARD_CONSTEXPR inline
#endif

// Verrou à hystérésis : une fois bloquée, la charge ne repart que lorsque le
// SOC atteint le seuil haut. Une perte de télémétrie ne libère jamais le verrou.
struct BatterySocGuard {
  bool blocked=false;
  SOC_GUARD_CONSTEXPR void reset() { blocked=false; }
  SOC_GUARD_CONSTEXPR bool update(bool enabled,bool fresh,int soc,int stopAt,int resumeAt) {
    if(!enabled) { blocked=false; return false; }
    if(!fresh) return blocked;
    if(blocked) {
      if(soc>=resumeAt) blocked=false;
    } else if(soc<=stopAt) blocked=true;
    return blocked;
  }
};
