#pragma once
#include <stddef.h>
#include <stdint.h>

// Lecture Modbus FC03 uniquement. Les adresses restent configurables sans
// permettre d'écrire une valeur dans l'onduleur.
#if __cplusplus >= 201402L
#define DEYE_CONSTEXPR constexpr
#else
#define DEYE_CONSTEXPR inline
#endif

namespace DeyeModbus {
constexpr size_t maxResponseSize=255; // 5 octets Modbus + 125 registres x 2

struct RegisterMap {
  uint16_t grid=169, load=178, soc=184, pv1=186, pv2=187, pv3=188, battery=190;
};
DEYE_CONSTEXPR RegisterMap defaults() { return RegisterMap{}; }
DEYE_CONSTEXPR uint16_t minAddress(const RegisterMap &m,bool thirdMppt) {
  uint16_t lo=m.grid<m.load?m.grid:m.load;
  lo=lo<m.soc?lo:m.soc; lo=lo<m.pv1?lo:m.pv1; lo=lo<m.pv2?lo:m.pv2;
  lo=lo<m.battery?lo:m.battery;
  return thirdMppt && m.pv3<lo?m.pv3:lo;
}
DEYE_CONSTEXPR uint16_t maxAddress(const RegisterMap &m,bool thirdMppt) {
  uint16_t hi=m.grid>m.load?m.grid:m.load;
  hi=hi>m.soc?hi:m.soc; hi=hi>m.pv1?hi:m.pv1; hi=hi>m.pv2?hi:m.pv2;
  hi=hi>m.battery?hi:m.battery;
  return thirdMppt && m.pv3>hi?m.pv3:hi;
}
DEYE_CONSTEXPR uint16_t registerCount(const RegisterMap &m,bool thirdMppt) {
  return uint16_t(maxAddress(m,thirdMppt)-minAddress(m,thirdMppt)+1);
}
DEYE_CONSTEXPR bool validMap(const RegisterMap &m,bool thirdMppt) {
  // Modbus FC03 autorise au plus 125 registres par lecture.
  return registerCount(m,thirdMppt)>=1 && registerCount(m,thirdMppt)<=125;
}
DEYE_CONSTEXPR uint8_t dataBytes(const RegisterMap &m,bool thirdMppt) {
  return uint8_t(registerCount(m,thirdMppt)*2);
}
DEYE_CONSTEXPR size_t frameSize(const RegisterMap &m,bool thirdMppt) {
  return size_t(dataBytes(m,thirdMppt))+5;
}
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
DEYE_CONSTEXPR void request(uint8_t slave,const RegisterMap &m,bool thirdMppt,uint8_t *r) {
  uint16_t start=minAddress(m,thirdMppt), count=registerCount(m,thirdMppt);
  r[0]=slave; r[1]=3; r[2]=start>>8; r[3]=start; r[4]=count>>8; r[5]=count;
  uint16_t c=crc(r,6); r[6]=c; r[7]=c>>8;
}
DEYE_CONSTEXPR bool validBaud(uint32_t baud) {
  return baud==1200 || baud==2400 || baud==4800 || baud==9600 ||
         baud==19200 || baud==38400 || baud==57600 || baud==115200;
}
DEYE_CONSTEXPR uint16_t reg(const uint8_t *r,uint16_t start,uint16_t address) {
  size_t i=3+size_t(address-start)*2;
  return uint16_t(r[i])<<8|r[i+1];
}
struct RawValues { uint16_t pv1=0,pv2=0,pv3=0,soc=0; int16_t load=0,battery=0,grid=0; };
DEYE_CONSTEXPR bool decode(const uint8_t *r,size_t n,uint8_t slave,const RegisterMap &m,
                           bool thirdMppt,RawValues &out) {
  size_t want=frameSize(m,thirdMppt);
  if(!validMap(m,thirdMppt) || n!=want || r[0]!=slave || r[1]!=3 ||
     r[2]!=dataBytes(m,thirdMppt) || !validCrc(r,n)) return false;
  uint16_t start=minAddress(m,thirdMppt);
  RawValues v;
  v.grid=int16_t(reg(r,start,m.grid)); v.load=int16_t(reg(r,start,m.load));
  v.soc=reg(r,start,m.soc); v.pv1=reg(r,start,m.pv1); v.pv2=reg(r,start,m.pv2);
  v.pv3=thirdMppt?reg(r,start,m.pv3):0; v.battery=int16_t(reg(r,start,m.battery));
  out=v; return true;
}

// Fenêtre glissante bornée : trames fragmentées, bruit, écho local et erreurs
// CRC sont écartés sans attendre une nouvelle requête.
struct Receiver {
  uint8_t bytes[maxResponseSize]={};
  size_t used=0;
  enum Result { Pending, Data, Exception };
  DEYE_CONSTEXPR void reset() { used=0; }
  DEYE_CONSTEXPR void discardFirst() {
    for(size_t i=1;i<used;i++) bytes[i-1]=bytes[i];
    --used;
  }
  DEYE_CONSTEXPR Result feed(uint8_t b,uint8_t slave,uint8_t expectedBytes) {
    if(used==maxResponseSize) discardFirst();
    bytes[used++]=b;
    while(used) {
      if(bytes[0]!=slave) { discardFirst(); continue; }
      if(used<2) return Pending;
      if(bytes[1]!=3 && bytes[1]!=0x83) { discardFirst(); continue; }
      if(used<3) return Pending;
      if(bytes[1]==3 && bytes[2]!=expectedBytes) { discardFirst(); continue; }
      size_t wanted=bytes[1]==3?size_t(expectedBytes)+5:5;
      if(used<wanted) return Pending;
      if(validCrc(bytes,wanted)) return bytes[1]==3?Data:Exception;
      discardFirst();
    }
    return Pending;
  }
};
}
