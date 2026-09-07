#pragma once
#include <WiFi.h>
#include <WiFiClient.h>
#include <HardwareSerial.h>
#include "config.h"
#include "deye_modbus.h"

// Adapted from the user's DEYE_LVGL project: Solarman V5, Modbus RTU slave 1.
// Only read function 03 is used; no inverter/battery settings are written.
struct DeyeSample {
  bool valid = false;
  uint32_t at = 0;
  int pv = 0, load = 0, battery = 0, grid = 0, soc = 0;
};
struct DeyeReader {
  WiFiClient socket;
  HardwareSerial rs485{1};
  DeyeModbus::Receiver receiver;
  DeyeSample sample;
  bool useRS485=false, rs485Started=false;
  uint32_t baud=9600, lastRxAt=0;
  uint8_t slave=1;
  String parity="8N1", error="Pas encore interrogé";
  String host;
  uint32_t serial = 0, started = 0, lastPoll = 0;
  float loadScale = 10, gridScale = 10;
  bool thirdMppt = true, waiting = false;
  uint8_t seq = 0, expectedSeq = 0, frame[256];
  size_t used = 0, wanted = 11;

  static uint16_t crc(const uint8_t *p, size_t n) {
    return DeyeModbus::crc(p,n);
  }
  static uint8_t checksum(const uint8_t *p, size_t n) {
    uint8_t sum=0; for(size_t i=1;i<n;i++) sum+=p[i]; return sum;
  }
  void cancel() { socket.stop(); waiting=false; sample.valid=false; }
  bool configured() const {
    return useRS485 ? slave>=1 && slave<=247 && DeyeModbus::validBaud(baud) &&
                     (parity=="8N1" || parity=="8E1" || parity=="8O1" || parity=="8N2")
                   : !host.isEmpty() && serial!=0;
  }
  void configure() {
    cancel(); receiver.reset(); used=0; wanted=11;
    if(rs485Started) { rs485.end(); rs485Started=false; }
    pinMode(DEYE_RS485_POWER,OUTPUT);
    pinMode(DEYE_RS485_CALLBACK,OUTPUT);
    pinMode(DEYE_RS485_ENABLE,OUTPUT);
    digitalWrite(DEYE_RS485_CALLBACK,HIGH);
    digitalWrite(DEYE_RS485_ENABLE,useRS485?HIGH:LOW);
    digitalWrite(DEYE_RS485_POWER,useRS485?HIGH:LOW);
    if(useRS485 && configured()) {
      uint32_t format=parity=="8E1"?SERIAL_8E1:parity=="8O1"?SERIAL_8O1:parity=="8N2"?SERIAL_8N2:SERIAL_8N1;
      rs485.setRxBufferSize(512);
      rs485.begin(baud,format,DEYE_RS485_RX,DEYE_RS485_TX);
      rs485Started=true;
    }
    lastPoll=millis(); lastRxAt=millis();
    error="En attente de mesures";
  }
  bool acceptRTU(const uint8_t *r,size_t n,uint8_t address) {
    DeyeModbus::Values v;
    if(!DeyeModbus::decode(r,n,address,thirdMppt,int(loadScale),int(gridScale),v)) return false;
    DeyeSample next;
    next.pv=v.pv; next.load=v.load; next.grid=v.grid;
    next.battery=v.battery; next.soc=v.soc;
    next.valid=true; next.at=millis(); sample=next; error=""; return true;
  }
  void tickRS485() {
    if(!rs485Started || !configured()) { cancel(); error="Configuration RS485 invalide"; return; }
    uint32_t now=millis();
    if(waiting && now-started>=1800) {
      cancel(); receiver.reset(); error="Délai RS485 dépassé (câblage, adresse, vitesse, parité ou CRC)";
    }
    // Lecture non bloquante, y compris lors d'un flux parasite permanent.
    for(size_t budget=0;budget<256 && rs485.available();budget++) {
      uint8_t b=rs485.read(); lastRxAt=millis();
      if(!waiting) continue; // Écarter les réponses tardives, jamais les réutiliser.
      auto result=receiver.feed(b,slave);
      if(result==DeyeModbus::Receiver::Exception) {
        error="Exception Modbus "+String(receiver.bytes[2]);
        cancel(); receiver.reset();
      } else if(result==DeyeModbus::Receiver::Data) {
        if(!acceptRTU(receiver.bytes,receiver.used,slave)) {
          cancel(); error="Mesures Deye incohérentes";
        } else waiting=false;
        receiver.reset();
      }
    }
    // Au moins 3,5 caractères de silence (11 bits/char), y compris à 1200 bauds.
    uint32_t quietMs=(38500U+baud-1)/baud+1;
    if(waiting || now-lastPoll<2500 || millis()-lastRxAt<quietMs || rs485.available()) return;
    lastPoll=now;
    uint8_t req[8]; DeyeModbus::request(slave,req);
    receiver.reset();
    if(rs485.write(req,sizeof(req))!=sizeof(req)) {
      cancel(); error="Échec émission RS485"; return;
    }
    // MAX13487 : direction automatique, aucune bascule manuelle de GPIO17/19.
    started=millis(); waiting=true;
  }
  bool decode(size_t n) {
    if(n < 26 || frame[0]!=0xa5 || frame[n-1]!=0x15 ||
       frame[n-2]!=checksum(frame,n-2) || frame[3]!=0x10 || frame[4]!=0x15 ||
       frame[5]!=expectedSeq || frame[11]!=2) return false;
    for(int i=0;i<4;i++) if(frame[7+i] != uint8_t(serial>>(8*i))) return false;
    // Response payload: 14-byte V5 prefix, then RTU. Some loggers add zero padding.
    const size_t offset=25, len=DeyeModbus::responseSize;
    if(offset+len>n-2) return false;
    return acceptRTU(frame+offset,len,1);
  }
  void tick() {
    if(useRS485) { tickRS485(); return; }
    uint32_t now=millis();
    if(WiFi.status()!=WL_CONNECTED || !configured()) { cancel(); error="Wi-Fi ou configuration LSW indisponible"; return; }
    if(waiting) {
      while(socket.available() && used<wanted) {
        frame[used++]=socket.read();
        if(used==11) {
          wanted=13+frame[1]+(uint16_t(frame[2])<<8);
          if(frame[0]!=0xa5 || wanted<26 || wanted>sizeof(frame)) { cancel(); return; }
        }
      }
      if(used==wanted) {
        if(decode(used)) { socket.stop(); waiting=false; }
        else { used=0; wanted=11; } // Ignore heartbeats/unrelated frames, within deadline.
      } else if(now-started>=1800 || !socket.connected()) cancel();
      // Also bound streams of unrelated complete frames.
      if(waiting && now-started>=1800) cancel();
      if(!waiting && !sample.valid) error="Réponse LSW absente ou invalide";
      return;
    }
    if(now-lastPoll<2500) return;
    lastPoll=now;
    if(!socket.connect(host.c_str(),8899,300)) { sample.valid=false; error="Connexion LSW impossible"; return; }
    uint8_t req[36]={0xa5,23,0,0x10,0x45};
    expectedSeq=seq++; req[5]=expectedSeq;
    for(int i=0;i<4;i++) req[7+i]=uint8_t(serial>>(8*i));
    req[11]=2;
    uint8_t rtu[8]; DeyeModbus::request(1,rtu);
    memcpy(req+26,rtu,8); req[34]=checksum(req,34); req[35]=0x15;
    if(socket.write(req,sizeof(req))!=sizeof(req)) { cancel(); return; }
    used=0; wanted=11; started=millis(); waiting=true;
  }
};
