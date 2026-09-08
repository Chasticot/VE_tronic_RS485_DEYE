#include "../VETRONIC_ESP32_OTA/battery_soc_guard.h"
constexpr bool hysteresis() {
  BatterySocGuard guard;
  if(guard.update(false,true,10,30,35)) return false;
  if(!guard.update(true,true,30,30,35)) return false;
  if(!guard.update(true,true,34,30,35)) return false;
  if(!guard.update(true,false,90,30,35)) return false;
  if(guard.update(true,true,35,30,35)) return false;
  if(guard.update(true,true,31,30,35)) return false;
  if(!guard.update(true,true,29,30,35)) return false;
  return !guard.update(false,false,0,30,35);
}
static_assert(hysteresis(),"Verrou SOC : arret bas, reprise haute et perte telemetrie");
