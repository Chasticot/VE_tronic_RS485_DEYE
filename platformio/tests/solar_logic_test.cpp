// Compile with C++14; every assertion evaluates the production algorithm.
// For the other variants, replace this include with their solar_logic.h.
#include "../src/rs485/solar_logic.h"

constexpr bool startAndLimits() {
  SolarLogic s;
  return s.decide(0,true,1799,0,0,230,32)==0 &&
    s.decide(5000,true,1800,0,0,230,32)==6 &&
    s.decide(10000,true,4600,0,0,230,32)==19 &&
    s.decide(15000,true,12000,0,0,230,16)==16;
}
static_assert(startAndLimits(),"Keep solar start threshold and current limit");

constexpr bool gridGrace() {
  SolarLogic s;
  if(s.decide(0,true,2300,0,0,230,32)!=9) return false;
  // Grid import above 200 W must no longer cause an immediate stop.
  if(s.decide(5000,true,300,0,1080,230,32)!=6 || !s.bridging) return false;
  if(s.decide(200000,true,300,0,1080,230,32)!=6 || s.deficitSince!=5000) return false;
  if(s.decide(304999,true,300,0,1080,230,32)!=6) return false;
  if(s.decide(305000,true,300,0,1080,230,32)!=0) return false;
  if(s.decide(310000,true,300,0,1080,230,32)!=0) return false;
  return s.decide(315000,true,2300,0,0,230,32)==9;
}
static_assert(gridGrace(),"Allow grid at 6 A for exactly five minutes, then stop and wait for sun");

constexpr bool batteryGraceAlwaysSix() {
  SolarLogic s;
  if(s.decide(0,true,4600,0,0,230,32)!=19) return false;
  if(s.decide(5000,true,4600,400,0,230,32)!=6) return false;
  if(s.decide(304999,true,4600,400,0,230,32)!=6) return false;
  if(s.decide(305000,true,4600,400,0,230,32)!=0) return false;
  if(s.decide(310000,true,4600,200,0,230,32)!=0) return false;
  return s.decide(315000,true,4600,150,0,230,32)==19;
}
static_assert(batteryGraceAlwaysSix(),"Battery discharge must also reduce a high calculated current to 6 A");

constexpr bool changingSupplyDoesNotExtendGrace() {
  SolarLogic s;
  s.decide(0,true,4600,0,0,230,32);
  if(s.decide(5000,true,4600,400,0,230,32)!=6) return false;
  if(s.decide(100000,true,1000,0,380,230,32)!=6 || s.deficitSince!=5000) return false;
  return s.decide(305000,true,1000,0,380,230,32)==0;
}
static_assert(changingSupplyDoesNotExtendGrace(),"Switching from battery to grid keeps the original deadline");

constexpr bool recovery() {
  SolarLogic s;
  s.decide(0,true,2300,0,0,230,32);
  s.decide(5000,true,1000,0,380,230,32);
  if(s.decide(20000,true,2300,0,0,230,32)!=9 || s.bridging) return false;
  s.decide(25000,true,1000,0,380,230,32);
  return s.decide(310000,true,1000,0,380,230,32)==6 &&
    s.decide(325000,true,1000,0,380,230,32)==0;
}
static_assert(recovery(),"Sun recovery cancels the timer; a later deficit starts a fresh window");

constexpr bool safetyStops() {
  SolarLogic s;
  s.decide(0,true,2300,0,0,230,32);
  s.decide(5000,true,1000,0,380,230,32);
  if(s.decide(10000,false,1000,0,380,230,32)!=0 || s.running || s.bridging) return false;
  s.decide(15000,true,2300,0,0,230,32);
  return s.decide(20000,true,1000,0,380,230,5)==0;
}
static_assert(safetyStops(),"Invalid telemetry or a limit below 6 A overrides grace");

constexpr bool smallLossesDoNotCycle() {
  SolarLogic s;
  if(s.decide(0,true,4600,0,0,230,32)!=19) return false;
  for(uint32_t t=5000;t<=900000;t+=5000) {
    if(s.decide(t,true,4600,180,0,230,32)!=19 || s.bridging) return false;
  }
  return s.decide(905000,true,1500,150,0,230,32)==6 && !s.bridging;
}
static_assert(smallLossesDoNotCycle(),"Preserve reserve and discharge hysteresis against noisy measurements");

constexpr bool dischargeHysteresis() {
  SolarLogic s;
  s.decide(0,true,4600,0,0,230,32);
  if(s.decide(5000,true,4600,300,0,230,32)!=19 || s.bridging) return false;
  if(s.decide(10000,true,4600,301,0,230,32)!=6 || s.deficitSince!=10000) return false;
  if(s.decide(20000,true,4600,250,0,230,32)!=6 || s.deficitSince!=10000) return false;
  return s.decide(30000,true,4600,150,0,230,32)==19 && !s.bridging;
}
static_assert(dischargeHysteresis(),"Discharge clears at 150 W, not at every fluctuation below 300 W");

constexpr bool wraparound() {
  SolarLogic s;
  uint32_t t=0xfffffff0U;
  s.decide(t-5000U,true,2300,0,0,230,32);
  s.decide(t,true,1000,0,380,230,32);
  return s.decide(uint32_t(t+299999U),true,1000,0,380,230,32)==6 &&
    s.decide(uint32_t(t+300000U),true,1000,0,380,230,32)==0;
}
static_assert(wraparound(),"Grid grace survives millis overflow");

constexpr bool deyeLossKeepsEarlierDeadline() {
  SolarLogic s;
  s.decide(0,true,2300,0,0,230,32);
  const int previous=s.decide(5000,true,1000,0,380,230,32);
  DeyeLossTimeout d;
  d.update(100000,false);
  d.update(200000,false);
  if(d.current(304999,true,s.running,previous,s.bridging,s.deficitSince)!=6) return false;
  if(d.current(305000,true,s.running,previous,s.bridging,s.deficitSince)!=0) return false;
  if(d.current(100001,false,s.running,previous,s.bridging,s.deficitSince)!=0) return false;
  if(d.current(100001,true,false,previous,s.bridging,s.deficitSince)!=0) return false;
  d.update(306000,true);
  return !d.lost;
}
static_assert(deyeLossKeepsEarlierDeadline(),"Deye loss cannot extend solar grace or bypass WB failure");
