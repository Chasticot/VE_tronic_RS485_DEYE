#include "../VETRONIC_RS485/solar_logic.h"
#include "../VETRONIC_RS485/manual_current_limit.h"
constexpr bool solarInstallationLimit() {
  SolarLogic s;
  return s.decide(0,true,4600,0,0,230,wb_solar_current_limit(32))==19 &&
    s.decide(5000,true,7560,0,0,230,wb_solar_current_limit(32))==32 &&
    s.decide(10000,true,12000,0,0,230,wb_solar_current_limit(63))==32 &&
    s.decide(15000,true,7360,0,0,230,wb_solar_current_limit(16))==16;
}
static_assert(solarInstallationLimit(),"Solar exceeds 16A, reaches 32A and respects configured and installation caps");
// Compile with C++14: these assertions execute the production algorithm at compile time.
constexpr bool threshold() {
  SolarLogic s;
  return s.decide(0,true,1799,0,0,230,32)==0 &&
    s.decide(5000,true,1800,0,0,230,32)==6 &&
    s.decide(10000,true,4600,0,0,230,32)==19 &&
    s.decide(15000,true,16000,0,0,230,16)==16;
}
constexpr bool grace() {
  SolarLogic s;
  if(s.decide(0,true,2300,0,0,230,32)!=9) return false;
  if(s.decide(5000,true,300,1080,0,230,32)!=6) return false;
  if(s.decide(304999,true,300,1080,0,230,32)!=6) return false;
  if(s.decide(305000,true,300,1080,0,230,32)!=0) return false;
  if(s.decide(310000,true,1900,500,0,230,32)!=0) return false;
  return s.decide(315000,true,2300,0,0,230,32)==9;
}
constexpr bool recovery() {
  SolarLogic s;
  s.decide(0,true,2300,0,0,230,32);
  s.decide(5000,true,1000,400,0,230,32);
  s.decide(20000,true,2300,0,0,230,32);
  s.decide(25000,true,1000,400,0,230,32);
  return s.decide(310000,true,1000,400,0,230,32)==6 &&
    s.decide(325000,true,1000,400,0,230,32)==0;
}
constexpr bool failures() {
  SolarLogic s;
  s.decide(0,true,2300,0,0,230,32);
  if(s.decide(5000,false,2300,0,0,230,32)!=0 || s.running) return false;
  if(s.decide(10000,true,2300,0,0,0,32)!=9) return false;
  if(s.decide(11000,true,2300,0,0,120,32)!=9) return false;
  if(s.decide(12000,true,2300,0,0,280,32)!=9) return false;
  s.decide(15000,true,2300,0,0,230,32);
  return s.decide(20000,true,200,0,1000,230,32)==0;
}
constexpr bool wraparound() {
  SolarLogic s;
  uint32_t t=0xfffffff0;
  s.decide(t-5000,true,2300,0,0,230,32);
  s.decide(t,true,1000,400,0,230,32);
  return s.decide(uint32_t(t+299999U),true,1000,400,0,230,32)==6 &&
    s.decide(uint32_t(t+300000U),true,1000,400,0,230,32)==0;
}
static_assert(threshold(),"1800W start, current adaptation, current cap");
static_assert(grace(),"five-minute battery window and restart lockout");
static_assert(recovery(),"sun recovery resets the grace window");
static_assert(failures(),"Missing telemetry and grid fallback stop; voltage alone never stops");
static_assert(wraparound(),"millis overflow must preserve five-minute window");

constexpr bool lossesDoNotCycle() {
  SolarLogic s;
  // 200 W headroom reduces a former 20 A request to 19 A.
  if(s.decide(0,true,4600,0,0,230,32)!=19) return false;
  for(uint32_t t=5000;t<=900000;t+=5000) {
    if(s.decide(t,true,4600,180,0,230,32)!=19 || s.bridging) return false;
  }
  // At minimum current the reserve may be partly consumed, without an actual deficit.
  return s.decide(905000,true,1500,150,0,230,32)==6 && !s.bridging;
}
constexpr bool dischargeHysteresis() {
  SolarLogic s;
  s.decide(0,true,4600,0,0,230,32);
  s.decide(5000,true,4600,300,0,230,32);
  if(s.bridging) return false;
  s.decide(10000,true,4600,301,0,230,32);
  if(!s.bridging || s.deficitSince!=10000) return false;
  s.decide(20000,true,4600,250,0,230,32);
  if(!s.bridging || s.deficitSince!=10000) return false;
  s.decide(30000,true,4600,150,0,230,32);
  if(s.bridging) return false;
  s.decide(40000,true,4600,400,0,230,32);
  if(s.decide(339999,true,4600,200,0,230,32)==0) return false;
  if(s.decide(340000,true,4600,200,0,230,32)!=0) return false;
  if(s.decide(345000,true,4600,200,0,230,32)!=0) return false;
  return s.decide(350000,true,4600,150,0,230,32)==19;
}
static_assert(lossesDoNotCycle(),"Small conversion losses must not cause repeated five-minute stops");
static_assert(dischargeHysteresis(),"Significant discharge keeps its deadline until recovery below 150W");

constexpr bool deyeTimeout() {
  DeyeLossTimeout d;
  d.update(1000,false);
  if(d.current(1000,true,true,16,false,0)!=16) return false;
  d.update(200000,false); // A failed retry must not extend the deadline.
  if(d.current(300999,true,true,16,false,0)!=16) return false;
  if(d.current(301000,true,true,16,false,0)!=0) return false;
  if(d.current(301001,true,true,16,false,0)!=0) return false;
  d.update(302000,true);
  if(d.lost) return false;
  d.update(310000,false);
  return d.remaining(310000)==300000 && d.current(310000,true,true,10,false,0)==10;
}
constexpr bool deyeSafety() {
  DeyeLossTimeout d;
  d.update(100000,false);
  return d.current(100001,true,false,16,false,0)==0 && // Never start on stale telemetry.
    d.current(100001,true,true,0,false,0)==0 &&       // Respect a prior stop.
    d.current(100001,false,true,16,false,0)==0 &&     // WB failure still stops immediately.
    d.current(299999,true,true,6,true,0)==6 &&
    d.current(300000,true,true,6,true,0)==0;          // Earlier battery deadline preserved.
}
constexpr bool deyeWraparound() {
  DeyeLossTimeout d;
  uint32_t t=0xfffffff0;
  d.update(t,false);
  return d.current(uint32_t(t+299999U),true,true,16,false,0)==16 &&
    d.current(uint32_t(t+300000U),true,true,16,false,0)==0;
}
static_assert(deyeTimeout(),"Deye loss holds current for five minutes, recovery resets timeout");
static_assert(deyeSafety(),"Deye grace cannot bypass stop, WB fault or battery deadline");
static_assert(deyeWraparound(),"Deye timeout survives millis overflow");
