#include "../VETRONIC_ESP32_OTA/deye_modbus.h"
using namespace DeyeModbus;

constexpr RegisterMap kMap=defaults();
constexpr bool kThird=true;
constexpr size_t kFrame=frameSize(kMap,kThird);
constexpr void seal(uint8_t *r,size_t n) { uint16_t c=crc(r,n-2); r[n-2]=c; r[n-1]=c>>8; }
constexpr void put(uint8_t *r,const RegisterMap &m,bool third,uint16_t address,uint16_t value) {
  size_t i=3+size_t(address-minAddress(m,third))*2; r[i]=value>>8; r[i+1]=value;
}
constexpr void sampleFrame(uint8_t *r,uint8_t slave=1,const RegisterMap &m=kMap,bool third=kThird) {
  r[0]=slave; r[1]=3; r[2]=dataBytes(m,third);
  put(r,m,third,m.grid,uint16_t(-25)); put(r,m,third,m.load,123); put(r,m,third,m.soc,75);
  put(r,m,third,m.pv1,1000); put(r,m,third,m.pv2,2000);
  if(third) put(r,m,third,m.pv3,3000);
  put(r,m,third,m.battery,uint16_t(-500)); seal(r,frameSize(m,third));
}
constexpr bool knownCRC() {
  uint8_t r[]={1,3,0,0,0,10,0xc5,0xcd};
  if(!validCrc(r,8)) return false;
  uint8_t req[8]={}; request(7,kMap,kThird,req);
  return req[0]==7 && req[1]==3 && req[2]==0 && req[3]==169 && req[4]==0 && req[5]==22 && validCrc(req,8);
}
constexpr bool measurements() {
  uint8_t r[kFrame]={}; sampleFrame(r); RawValues v;
  if(!decode(r,kFrame,1,kMap,true,v) || v.pv1!=1000 || v.pv2!=2000 || v.pv3!=3000 || v.load!=123 || v.grid!=-25 || v.battery!=-500 || v.soc!=75) return false;
  if(!decode(r,frameSize(kMap,false),1,kMap,false,v)) return false;
  if(v.pv3!=0 || v.pv1!=1000 || v.load!=123) return false;
  if(decode(r,kFrame-1,1,kMap,true,v) || decode(r,kFrame,2,kMap,true,v)) return false;
  r[7]^=1; return !decode(r,kFrame,1,kMap,true,v);
}
constexpr bool configurableMap() {
  RegisterMap m; m.grid=310; m.load=302; m.soc=306; m.pv1=300; m.pv2=301; m.pv3=305; m.battery=309;
  if(!validMap(m,true) || minAddress(m,true)!=300 || registerCount(m,true)!=11) return false;
  uint8_t r[frameSize(m,true)]={}; sampleFrame(r,4,m,true); RawValues v;
  if(!decode(r,frameSize(m,true),4,m,true,v) || v.grid!=-25 || v.battery!=-500 || v.pv3!=3000) return false;
  uint8_t req[8]={}; request(4,m,true,req);
  if(req[2]!=1 || req[3]!=44 || req[4]!=0 || req[5]!=11 || !validCrc(req,8)) return false;
  m.battery=500; return !validMap(m,true);
}
constexpr bool fragmentationAndNoise() {
  Receiver rx; uint8_t r[kFrame]={}, req[8]={}; sampleFrame(r,7); request(7,kMap,kThird,req);
  for(int i=0;i<8;i++) if(rx.feed(req[i],7,dataBytes(kMap,kThird))!=Receiver::Pending) return false;
  for(int i=0;i<200;i++) if(rx.feed(0xff,7,dataBytes(kMap,kThird))!=Receiver::Pending) return false;
  for(size_t i=0;i<kFrame;i++) if(rx.feed(r[i],7,dataBytes(kMap,kThird))!=(i==kFrame-1?Receiver::Data:Receiver::Pending)) return false;
  RawValues v; return decode(rx.bytes,rx.used,7,kMap,kThird,v) && v.pv1==1000;
}
constexpr bool recoveryAndExceptions() {
  Receiver rx; uint8_t r[kFrame]={}; sampleFrame(r,2);
  for(size_t i=0;i<kFrame;i++) if(rx.feed(r[i],1,dataBytes(kMap,kThird))!=Receiver::Pending) return false;
  sampleFrame(r); r[kFrame-2]^=1;
  for(size_t i=0;i<kFrame;i++) if(rx.feed(r[i],1,dataBytes(kMap,kThird))!=Receiver::Pending) return false;
  r[kFrame-2]^=1;
  for(size_t i=0;i<kFrame;i++) if(rx.feed(r[i],1,dataBytes(kMap,kThird))!=(i==kFrame-1?Receiver::Data:Receiver::Pending)) return false;
  rx.reset(); uint8_t ex[]={1,0x83,2,0xc0,0xf1};
  for(int i=0;i<5;i++) if(rx.feed(ex[i],1,dataBytes(kMap,kThird))!=(i==4?Receiver::Exception:Receiver::Pending)) return false;
  return true;
}
static_assert(knownCRC(),"Requete/CRC Modbus incorrect");
static_assert(measurements(),"Decodage des mesures Modbus incorrect");
static_assert(configurableMap(),"Carte de registres configurable incorrecte");
static_assert(fragmentationAndNoise(),"Fragments, echo local ou bruit incorrects");
static_assert(recoveryAndExceptions(),"Reprise Modbus ou exceptions incorrectes");
static_assert(validBaud(9600) && validBaud(115200) && !validBaud(0) && !validBaud(12345),"Vitesses serie");
