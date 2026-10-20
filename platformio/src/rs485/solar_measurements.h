#pragma once
#include "solar_logic.h"

// Deye and WB-01 are polled independently. Keep the same calculation in the
// controller and dashboard, including the exclusion of standby current.
struct SolarPower {
  int evW, houseW, surplusW;
  SOLAR_CONSTEXPR SolarPower(int pvW, int loadW, bool includesEV,
                            bool wbFresh, unsigned state, int milliamps, int volts)
    : evW(wbFresh && state==2 ? milliamps*volts/1000 : 0),
      houseW(loadW-(includesEV?evW:0)), surplusW(pvW-houseW) {}
};

struct SolarMeasurementGuard {
  bool pending=false, blocked=false;
  uint32_t since=0;
  int faultHouseW=0;
  SOLAR_CONSTEXPR void reset() {
    pending=blocked=false; since=0; faultHouseW=0;
  }
  SOLAR_CONSTEXPR void update(uint32_t now, bool fresh, int houseW) {
    // Stopping the car makes load-EV positive again even with a wrong setup.
    // That alone must NEVER acknowledge a previously confirmed fault.
    if(blocked) return;
    if(fresh && houseW>=0) { pending=false; return; }
    if(fresh && houseW<0) {
      if(!pending) { pending=true; since=now; }
      faultHouseW=houseW;
    }
    // Missing telemetry must not extend an already started settling window.
    if(pending && uint32_t(now-since)>=15000U) blocked=true;
  }
  SOLAR_CONSTEXPR int holdCurrent(uint32_t now, bool wbFresh, bool active,
                                 int previousA, bool bridging, uint32_t deficitSince,
                                 bool probing, uint32_t probeStarted) const {
    if(!pending || blocked || uint32_t(now-since)>=15000U ||
       !wbFresh || !active || previousA<6) return 0;
    if(bridging && uint32_t(now-deficitSince)>=300000U) return 0;
    if(probing && uint32_t(now-probeStarted)>=120000U) return 0;
    return previousA; // Never start or increase from inconsistent measurements.
  }
};
