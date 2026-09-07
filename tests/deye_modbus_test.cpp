#include "../VETRONIC_ESP32_OTA/deye_modbus.h"
using namespace DeyeModbus;
constexpr void seal(uint8_t *r,size_t n) { uint16_t c=crc(r,n-2); r[n-2]=c; r[n-1]=c>>8; }
constexpr void put(uint8_t *r,int a,uint16_t v) { int i=3+2*(a-169); r[i]=v>>8; r[i+1]=v; }
constexpr void sampleFrame(uint8_t *r,uint8_t slave=1) {
  r[0]=slave; r[1]=3; r[2]=54;
  put(r,169,uint16_t(-25)); put(r,178,123); put(r,184,75);
  put(r,186,1000); put(r,187,2000); put(r,188,3000); put(r,190,uint16_t(-500));
  seal(r,responseSize);
}
constexpr bool knownCRC() {
  // Vecteur public Modbus FC03 et CRC standard, indépendant du générateur.
  uint8_t r[]={1,3,0,0,0,10,0xc5,0xcd};
  if(!validCrc(r,8)) return false;
  uint8_t req[8]={}; request(7,req);
  return req[0]==7 && req[1]==3 && req[2]==0 && req[3]==169 && req[4]==0 && req[5]==27 && validCrc(req,8);
}
constexpr bool measurements() {
  uint8_t r[responseSize]={}; sampleFrame(r); Values v;
  if(!decode(r,responseSize,1,true,10,10,v) || v.pv!=6000 || v.load!=1230 || v.grid!=-250 || v.battery!=-500 || v.soc!=75) return false;
  if(!decode(r,responseSize,1,false,1,1,v) || v.pv!=3000 || v.load!=123 || v.grid!=-25) return false;
  if(decode(r,responseSize-1,1,true,10,10,v) || decode(r,responseSize,2,true,10,10,v)) return false;
  r[7]^=1; if(decode(r,responseSize,1,true,10,10,v)) return false; r[7]^=1;
  put(r,184,101); seal(r,responseSize);
  return !decode(r,responseSize,1,true,10,10,v) && v.soc==75;
}
constexpr bool fragmentationAndNoise() {
  Receiver rx; uint8_t r[responseSize]={}, req[8]={}; sampleFrame(r,7); request(7,req);
  for(int i=0;i<8;i++) if(rx.feed(req[i],7)!=Receiver::Pending) return false;
  for(int i=0;i<200;i++) if(rx.feed(0xff,7)!=Receiver::Pending) return false;
  for(size_t i=0;i<responseSize;i++) {
    auto result=rx.feed(r[i],7);
    if(result!=(i==responseSize-1?Receiver::Data:Receiver::Pending)) return false;
  }
  Values v; return decode(rx.bytes,rx.used,7,true,10,10,v) && v.pv==6000;
}
constexpr bool recovery() {
  Receiver rx; uint8_t r[responseSize]={}; sampleFrame(r,2);
  for(size_t i=0;i<responseSize;i++) if(rx.feed(r[i],1)!=Receiver::Pending) return false;
  sampleFrame(r); r[58]^=1;
  for(size_t i=0;i<responseSize;i++) if(rx.feed(r[i],1)!=Receiver::Pending) return false;
  r[58]^=1;
  for(size_t i=0;i<responseSize;i++) {
    auto result=rx.feed(r[i],1);
    if(result!=(i==responseSize-1?Receiver::Data:Receiver::Pending)) return false;
  }
  rx.reset(); return rx.used==0;
}
constexpr bool exceptions() {
  Receiver rx; uint8_t ex[]={1,0x83,2,0xc0,0xf1};
  for(size_t i=0;i<5;i++) if(rx.feed(ex[i],1)!=(i==4?Receiver::Exception:Receiver::Pending)) return false;
  rx.reset(); ex[4]^=1;
  for(size_t i=0;i<5;i++) if(rx.feed(ex[i],1)!=Receiver::Pending) return false;
  return true;
}
constexpr bool embeddedException() {
  Receiver rx; uint8_t r[responseSize]={}; sampleFrame(r);
  // CRC d'exception valide dans les données : ne doit pas interrompre FC03.
  r[10]=1; r[11]=0x83; r[12]=2; r[13]=0xc0; r[14]=0xf1; seal(r,responseSize);
  rx.feed(0xff,1); rx.feed(0xaa,1);
  for(size_t i=0;i<responseSize;i++) if(rx.feed(r[i],1)!=(i==responseSize-1?Receiver::Data:Receiver::Pending)) return false;
  return true;
}
static_assert(knownCRC(),"Requete/CRC Modbus incorrect");
static_assert(measurements(),"Mesures, signes, facteurs, validation, atomicite");
static_assert(fragmentationAndNoise(),"Fragments, echo local, bruit et borne du tampon");
static_assert(recovery(),"Mauvais esclave/CRC puis reprise");
static_assert(exceptions(),"Exceptions Modbus avec controle CRC");
static_assert(embeddedException(),"Ne pas confondre des mesures et une exception apres du bruit");
static_assert(validBaud(9600) && validBaud(115200) && !validBaud(0) && !validBaud(12345),"Vitesses serie");
