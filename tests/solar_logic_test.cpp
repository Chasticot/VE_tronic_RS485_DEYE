#include "../VETRONIC_ESP32_OTA/solar_logic.h"
// Compile with C++14: these assertions execute the production algorithm at compile time.
constexpr bool threshold() {
  SolarLogic s;
  return s.decide(0,true,1799,0,0,230,32)==0 &&
    s.decide(5000,true,1800,0,0,230,32)==7 &&
    s.decide(10000,true,4600,0,0,230,32)==20 &&
    s.decide(15000,true,16000,0,0,230,16)==16;
}
constexpr bool grace() {
  SolarLogic s;
  if(s.decide(0,true,2300,0,0,230,32)!=10) return false;
  if(s.decide(5000,true,300,1080,0,230,32)!=6) return false;
  if(s.decide(304999,true,300,1080,0,230,32)!=6) return false;
  if(s.decide(305000,true,300,1080,0,230,32)!=0) return false;
  if(s.decide(310000,true,1900,500,0,230,32)!=0) return false;
  return s.decide(315000,true,2300,0,0,230,32)==10;
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
  if(s.decide(10000,true,2300,0,0,0,32)!=0) return false;
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
static_assert(failures(),"missing telemetry, voltage and grid fallback stop");
static_assert(wraparound(),"millis overflow must preserve five-minute window");

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
