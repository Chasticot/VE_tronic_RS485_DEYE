#pragma once
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "solar_logic.h"

SOLAR_CONSTEXPR bool wbSpace(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
SOLAR_CONSTEXPR const char *wbFind(const char *s,const char *needle) {
  for(;*s;++s) {
    unsigned i=0; while(needle[i] && s[i] && s[i]==needle[i]) ++i;
    if(!needle[i]) return s;
  }
  return nullptr;
}
SOLAR_CONSTEXPR bool wbUnsigned(const char *&s,unsigned &out) {
  while(wbSpace(*s)) ++s;
  if(*s<'0' || *s>'9') return false;
  out=0;
  while(*s>='0' && *s<='9') {
    if(out>63000) return false;
    out=out*10+unsigned(*s++-'0');
  }
  return *s==0 || wbSpace(*s);
}

struct WBTelemetry {
  unsigned state=99, milliamps=0, volts=0, limitMilliamps=0;
};
SOLAR_CONSTEXPR bool wbParseValues(const char *reply, unsigned &ma, unsigned &v, unsigned &cap) {
  const char *p=wbFind(reply,"$OK ");
  if(!p) return false;
  p+=4;
  return wbUnsigned(p,ma) && wbUnsigned(p,v) && wbUnsigned(p,cap) && ma<=63000 && cap<=63000;
}
SOLAR_CONSTEXPR bool wbParseTelemetry(const char *state, const char *values, WBTelemetry &out) {
  const char *p=wbFind(state,"EVSE state :");
  if(!p) return false;
  p+=12;
  return wbUnsigned(p,out.state) && out.state<=2 &&
         wbParseValues(values,out.milliamps,out.volts,out.limitMilliamps);
}
SOLAR_CONSTEXPR unsigned wbCalculationVolts(const WBTelemetry &value) {
  if(value.volts>=180 && value.volts<=260) return value.volts;
  // La tension WB-01 ne sert qu'aux calculs de puissance. Une mesure absente
  // ou aberrante ne doit jamais invalider l'état de la borne ni bloquer une
  // commande : utiliser alors la référence monophasée de 230 V.
  return 230;
}
// Some WB-01 firmwares return only the exact command echo and shell prompt.
// This confirms a completed serial exchange, not that the requested limit is applied.
SOLAR_CONSTEXPR bool wbCurrentShellReply(const char *reply, int amps) {
  while(wbSpace(*reply)) ++reply;
  if(wbFind(reply,"$SC ")!=reply) return false;
  reply+=4;
  bool negative=*reply=='-'; if(negative) ++reply;
  unsigned value=0;
  if(!wbUnsigned(reply,value) || value>63 || (negative?-int(value):int(value))!=amps) return false;
  if(*reply!='\r' && *reply!='\n') return false;
  while(wbSpace(*reply)) ++reply;
  if(wbFind(reply,"wallbox$")!=reply) return false;
  reply+=8;
  while(wbSpace(*reply)) ++reply;
  return *reply==0;
}
