#include "../VETRONIC_RS485/wb_protocol.h"
constexpr bool userStandby() {
  WBTelemetry t;
  if(!wbParseTelemetry("evse_state\r\nEVSE state : 1\r\nwallbox$",
      "$GG*B2\r\n$OK 100 0 0\r\nwallbox$",t)) return false;
  if(t.volts!=0 || t.milliamps!=100 || wbCalculationVolts(t)!=230) return false;
  SolarLogic s;
  return s.decide(0,true,2353,-1836,50,wbCalculationVolts(t),16)==9;
}
constexpr bool voltageCases() {
  WBTelemetry t;
  if(!wbParseTelemetry("EVSE state : 2\r\n","$OK 100 0 0\r\n",t) || wbCalculationVolts(t)!=230) return false;
  if(!wbParseTelemetry("EVSE state : 2\r\n","$OK 100 120 0\r\n",t) || wbCalculationVolts(t)!=230) return false;
  if(!wbParseTelemetry("EVSE state : 2\r\n","$OK 100 280 0\r\n",t) || wbCalculationVolts(t)!=230) return false;
  if(wbParseTelemetry("EVSE state : 1\r\n","$OK 63001 0 0\r\n",t)) return false;
  if(wbParseTelemetry("EVSE state : 4\r\n","$OK 0 230 0\r\n",t)) return false;
  if(wbParseTelemetry("EVSE state : 1\r\n","$OK 100 0\r\nwallbox$",t)) return false;
  return wbParseTelemetry("EVSE state : 2\r\n","$OK 10000 237 10000\r\n",t) && wbCalculationVolts(t)==237;
}
static_assert(userStandby(),"User fixture: connected at 0V must allow solar startup at 9A with reserve");
constexpr bool residualVoltageStartup() {
  for(unsigned state=0;state<2;++state) {
    for(unsigned volts=0;volts<180;++volts) {
      WBTelemetry t;
      // Exercise parsing below with fixed residual values; calculation across
      // the whole de-energized range must use the nominal reference.
      t.state=state; t.volts=volts;
      if(wbCalculationVolts(t)!=230) return false;
    }
  }
  WBTelemetry t;
  if(!wbParseTelemetry("EVSE state : 1\r\n","$OK 100 3 0\r\n",t)) return false;
  SolarLogic s;
  if(s.decide(0,true,2300,0,0,wbCalculationVolts(t),16)!=9) return false;
  if(!wbParseTelemetry("EVSE state : 1\r\n","$OK 100 120 0\r\n",t)) return false;
  SolarLogic probe;
  if(probe.decide(0,true,300,0,-300,wbCalculationVolts(t),16)!=0) return false;
  return probe.decide(120000,true,300,0,-300,wbCalculationVolts(t),16)==6;
}
static_assert(residualVoltageStartup(),"Residual outlet voltage must allow solar startup and probe");
static_assert(voltageCases(),"Voltage is calculation-only; reject invalid current, incomplete and fault telemetry");
static_assert(wbCurrentShellReply("$SC 0\r\nwallbox$",0),"WB-01 shell-only response");
static_assert(wbCurrentShellReply("$SC 10\r\nwallbox$",10),"Positive current echo");
static_assert(wbCurrentShellReply("$SC -1\r\nwallbox$",-1),"Release command echo");
static_assert(!wbCurrentShellReply("$SC 0\r\n",0),"Echo without prompt is incomplete");
static_assert(!wbCurrentShellReply("$SC 10\r\nwallbox$",0),"Wrong command echo");
static_assert(!wbCurrentShellReply("$SC 0\r\nError\r\nwallbox$",0),"Error is not success");
static_assert(!wbCurrentShellReply("wallbox$",0),"Prompt alone is not success");
