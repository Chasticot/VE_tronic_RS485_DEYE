#include "../VETRONIC_ESP32_OTA/wb_protocol.h"
constexpr bool userStandby() {
  WBTelemetry t;
  if(!wbParseTelemetry("evse_state\r\nEVSE state : 1\r\nwallbox$",
      "$GG*B2\r\n$OK 100 0 0\r\nwallbox$",t)) return false;
  if(t.volts!=0 || t.milliamps!=100 || wbCalculationVolts(t)!=230) return false;
  SolarLogic s;
  return s.decide(0,true,2353,-1836,50,wbCalculationVolts(t),16)==10;
}
constexpr bool voltageCases() {
  WBTelemetry t;
  // Le projet fourni conserve désormais l'état WB-01 et calcule à 230 V
  // lorsque sa mesure de tension est absente/hors plage.
  if(!wbParseTelemetry("EVSE state : 2\r\n","$OK 100 0 0\r\n",t) || wbCalculationVolts(t)!=230) return false;
  if(!wbParseTelemetry("EVSE state : 1\r\n","$OK 100 120 0\r\n",t) || wbCalculationVolts(t)!=230) return false;
  if(!wbParseTelemetry("EVSE state : 1\r\n","$OK 9000 0 0\r\n",t) || wbCalculationVolts(t)!=230) return false;
  if(wbParseTelemetry("EVSE state : 1\r\n","$OK 64000 230 0\r\n",t)) return false;
  if(wbParseTelemetry("EVSE state : 4\r\n","$OK 0 230 0\r\n",t)) return false;
  if(wbParseTelemetry("EVSE state : 1\r\n","$OK 100 0\r\nwallbox$",t)) return false;
  return wbParseTelemetry("EVSE state : 2\r\n","$OK 10000 237 10000\r\n",t) && wbCalculationVolts(t)==237;
}
static_assert(userStandby(),"User fixture: connected at 0V must allow solar startup at 10A");
static_assert(voltageCases(),"Voltage fallback; reject excessive current, incomplete and fault telemetry");
static_assert(wbCurrentShellReply("$SC 0\r\nwallbox$",0),"WB-01 shell-only response");
static_assert(wbCurrentShellReply("$SC 10\r\nwallbox$",10),"Positive current echo");
static_assert(wbCurrentShellReply("$SC -1\r\nwallbox$",-1),"Release command echo");
static_assert(!wbCurrentShellReply("$SC 0\r\n",0),"Echo without prompt is incomplete");
static_assert(!wbCurrentShellReply("$SC 10\r\nwallbox$",0),"Wrong command echo");
static_assert(!wbCurrentShellReply("$SC 0\r\nError\r\nwallbox$",0),"Error is not success");
static_assert(!wbCurrentShellReply("wallbox$",0),"Prompt alone is not success");
