#pragma once
#include <stddef.h>
#include <stdint.h>

// Bloc de mesures du projet Deye existant. Lecture seule, fonction 03.
#if __cplusplus >= 201402L
#define DEYE_CONSTEXPR constexpr
#else
#define DEYE_CONSTEXPR inline
#endif
namespace DeyeModbus {
constexpr size_t responseSize = 59;
DEYE_CONSTEXPR uint16_t crc(const uint8_t *p, size_t n) {
  uint16_t c=0xffff;
  for(size_t i=0;i<n;i++) {
    c^=p[i];
    for(int b=0;b<8;b++) c=c&1 ? (c>>1)^0xa001 : c>>1;
  }
  return c;
}
DEYE_CONSTEXPR bool validCrc(const uint8_t *p,size_t n) {
  return n>=4 && crc(p,n-2)==(p[n-2]|uint16_t(p[n-1])<<8);
}
DEYE_CONSTEXPR void request(uint8_t slave,uint8_t *r) {
  r[0]=slave; r[1]=3; r[2]=0; r[3]=169; r[4]=0; r[5]=27;
  uint16_t c=crc(r,6); r[6]=c; r[7]=c>>8;
}
DEYE_CONSTEXPR bool validBaud(uint32_t baud) {
  return baud==1200 || baud==2400 || baud==4800 || baud==9600 ||
         baud==19200 || baud==38400 || baud==57600 || baud==115200;
}
struct Values { int pv=0, load=0, battery=0, grid=0, soc=0; };
DEYE_CONSTEXPR uint16_t reg(const uint8_t *r,int address) {
  int i=3+2*(address-169); return uint16_t(r[i])<<8|r[i+1];
}
DEYE_CONSTEXPR bool decode(const uint8_t *r,size_t n,uint8_t slave,bool pv3,
                      int loadScale,int gridScale,Values &out) {
  if(n!=responseSize || r[0]!=slave || r[1]!=3 || r[2]!=54 || !validCrc(r,n)) return false;
  if((loadScale!=1 && loadScale!=10) || (gridScale!=1 && gridScale!=10)) return false;
  Values v;
  v.pv=reg(r,186)+reg(r,187)+(pv3?reg(r,188):0);
  v.load=int16_t(reg(r,178))*loadScale; v.grid=int16_t(reg(r,169))*gridScale;
  v.battery=int16_t(reg(r,190)); v.soc=reg(r,184);
  if(v.pv>60000 || v.load<0 || v.load>60000 || v.soc>100) return false;
  out=v; return true;
}
// Fenêtre glissante bornée : accepte les réponses fragmentées et retrouve une
// trame après du bruit, un écho de requête, un mauvais CRC ou un autre esclave.
struct Receiver {
  uint8_t bytes[responseSize]={};
  size_t used=0;
  enum Result { Pending, Data, Exception };
  DEYE_CONSTEXPR void reset() { used=0; }
  DEYE_CONSTEXPR void discardFirst() {
    for(size_t i=1;i<used;i++) bytes[i-1]=bytes[i];
    --used;
  }
  DEYE_CONSTEXPR Result feed(uint8_t b,uint8_t slave) {
    if(used==responseSize) discardFirst();
    bytes[used++]=b;
    while(used) {
      if(bytes[0]!=slave) { discardFirst(); continue; }
      if(used<2) return Pending;
      if(bytes[1]!=3 && bytes[1]!=0x83) { discardFirst(); continue; }
      if(used<3) return Pending;
      if(bytes[1]==3 && bytes[2]!=54) { discardFirst(); continue; }
      size_t wanted=bytes[1]==3?responseSize:5;
      if(used<wanted) return Pending;
      // Tester une trame entière avant de chercher un nouveau début : une
      // séquence ressemblant à une exception peut être une mesure légitime.
      if(validCrc(bytes,wanted)) return bytes[1]==3?Data:Exception;
      discardFirst();
    }
    return Pending;
  }
};
}
