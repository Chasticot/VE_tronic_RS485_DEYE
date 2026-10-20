#pragma once
#include <ArduinoJson.h>
#include <base64.h>
#include "solar_logic.h"
#include "solar_measurements.h"
#include "battery_soc_guard.h"
#include "deye_solarman.h"
#include "lilygo_led.h"
#include "pilotage_page.h"
#include "wb_protocol.h"
#include "manual_current_limit.h"
#include "history_protocol.h"
#include "history_store.h"
#include "temperature_sensor.h"
#include "history_page.h"

DeyeReader deye;
SolarLogic solar;
DeyeLossTimeout deyeLoss;
SolarMeasurementGuard solarMeasurements;
BatterySocGuard batterySocGuard;
String controlMode="solar", controlMessage="Initialisation du solaire dynamique", csrfToken;
bool solarStartupPending=true, wbReconnectPending=false;
int limitA=32, targetA=-1, nativeLimitA=6;
bool loadIncludesEV=true, meterConfirmed=false;
bool batterySocEnabled=false;
int batterySocStop=30, batterySocResume=35;
uint32_t lastControl=0, wbSampleAt=0;
bool wbSampleValid=false;
String wbReadError="Borne pas encore interrogée", lastCurrentReply, lastStateReply, lastValuesReply;
int lastRequestedA=-1;
bool currentConfirmed=false;
bool currentDelivered=false;
String currentReadback;
int calculationVolts=0;
String pendingWifiSsid, pendingWifiPassword;
uint32_t wifiConnectAt=0, firmwareRestartAt=0;
bool firmwareUploadAllowed=false, firmwareUploadSuccess=false;
String firmwareUploadError;
uint32_t lilygoLedStartedAt=0;
LilygoLedState lilygoLedLastState=LILYGO_LED_FAULT;
bool lilygoLedEnabled=true, lilygoLedFaultBlink=false, lilygoLedFaultVisible=true;
uint32_t lilygoLedLastBlinkAt=0, lilygoLedLastColor=0xffffffff;
uint32_t lilygoLedColors[4]={0x000030,0x003000,0x280030,0x380000};

HistoryStore historyStore;
HistoryTemperatureSensor historyTemperature;
uint32_t historyBootId=0, historyLastSample=0, historyLastMinute=0, historyLastSupply=0;
uint32_t historyLastZeroRequestAt=0;
int historySupplyMin=0, historySupplyMax=0;
String historySupplyRaw;
bool historySupplyValid=false, historySupplyWarning=false, historyTemperatureWarning=false;
bool historyOperation=false;
unsigned historyPreviousState=99;
bool historyPreviousWbValid=false, historyPreviousDeyeValid=false;
bool historyPreviousSocBlocked=false, historyPreviousMeasurementBlocked=false;
String historyPreviousMode, historyPreviousControlAlert;

bool historyTimeValid() { return rtc.getEpoch()>1609459200LL; }
String historyDate() { return historyTimeValid()?rtc.getTime("%Y-%m-%d"):"sans-date"; }
String historyTimestamp() {
  if(historyTimeValid()) return rtc.getTime("%Y-%m-%d %H:%M:%S");
  return "sans-date +"+String(millis()/1000U)+"s";
}
String historyCsvText(String value) {
  value.replace(';',','); value.replace('\r',' '); value.replace('\n',' ');
  if(value.length()>160) value=value.substring(0,160);
  return value;
}
String historyByteText(uint64_t bytes) {
  if(bytes>=1073741824ULL) return String(double(bytes)/1073741824.0,1)+" Go";
  if(bytes>=1048576ULL) return String(double(bytes)/1048576.0,1)+" Mo";
  if(bytes>=1024ULL) return String(double(bytes)/1024.0,1)+" Ko";
  return String(uint32_t(bytes))+" o";
}
void historyEvent(const char *level,const char *kind,const String &message) {
  if(!historyStore.ready() || historyOperation) return;
  String line; line.reserve(320);
  line=historyTimestamp()+" | "+String(level);
  while(line.length()<34) line+=' ';
  line+=" | "+String(kind)+" | "+historyCsvText(message);
  line+=" | état="+String(evse_state.code_status)+", cible="+String(targetA)+" A, U="+String(evse_state.tension)+" V, I="+String(evse_state.courant/1000.0f,1)+" A";
  if(historySupplyValid) line+=", Uintervalle="+String(historySupplyMin)+".."+String(historySupplyMax)+" V";
  if(historyTemperature.valid()) line+=", température="+String(historyTemperature.value(),1)+" °C";
  historyStore.appendEvent(historyDate(),line);
}

String historySampleLine() {
  bool wbFresh=wbSampleValid && uint32_t(millis()-wbSampleAt)<=10000U;
  bool deyeFresh=deye.sample.valid && uint32_t(millis()-deye.sample.at)<=15000U;
  const SolarPower power(deye.sample.pv,deye.sample.load,loadIncludesEV,
                         wbFresh,evse_state.code_status,evse_state.courant,calculationVolts);
  String line; line.reserve(600);
  line=historyTimestamp()+";"+String((unsigned long)(historyTimeValid()?rtc.getEpoch():0))+";"+String(millis())+";"+String(historyBootId,HEX);
  line+=";"+String(evse_state.code_status)+";"+String(wbFresh?1:0)+";"+String(evse_state.tension)+";"+String(calculationVolts);
  line+=";"+(historySupplyValid?String(historySupplyMin):"")+";"+(historySupplyValid?String(historySupplyMax):"");
  line+=";"+String(evse_state.courant/1000.0f,3)+";"+String(evse_state.courant_max/1000.0f,3)+";"+String(targetA)+";"+String(lastRequestedA)+";"+String(currentConfirmed?1:0);
  line+=";"+String(deyeFresh?1:0)+";"+String(deye.sample.pv)+";"+String(deye.sample.load)+";"+String(deye.sample.grid)+";"+String(deye.sample.battery)+";"+String(deye.sample.soc);
  line+=";"+String(power.evW)+";"+String(power.houseW)+";"+String(power.surplusW)+";"+historyCsvText(controlMode);
  line+=";"+String(batterySocGuard.blocked?1:0)+";"+String(solarMeasurements.pending?1:0)+";"+String(solarMeasurements.blocked?1:0);
  line+=";"+String(solar.bridging?(300000U-min(uint32_t(300000),uint32_t(millis()-solar.deficitSince)))/1000U:0U)+";"+String(solar.probing?1:0);
  line+=";"+(historyTemperature.valid()?String(historyTemperature.value(),2):"")+";"+String(historyTemperature.valid()?1:0);
  line+=";"+historyCsvText(wbReadError)+";"+historyCsvText(deye.error);
  return line;
}

void historyPrune() {
  if(!historyStore.ready() || !historyTimeValid()) return;
  time_t cutoff=time_t(rtc.getEpoch()-int64_t(HISTORY_RETENTION_DAYS)*86400LL);
  struct tm value; localtime_r(&cutoff,&value);
  char date[11]; strftime(date,sizeof(date),"%Y-%m-%d",&value);
  historyStore.removeOlderThan("/history/data",date,".csv");
  historyStore.removeOlderThan("/history/minute",date,".csv");
  historyStore.removeOlderThan("/history/events",date,".log");
}

void historyBegin() {
  historyBootId=esp_random();
  historyTemperature.begin();
  historyLastZeroRequestAt=millis()-16001U;
  if(!historyStore.begin()) { Serial.println("Historique SD indisponible : "+historyStore.error()); return; }
  historyPreviousState=evse_state.code_status;
  historyPreviousWbValid=wbSampleValid;
  historyPreviousDeyeValid=deye.sample.valid;
  historyPreviousMode=controlMode;
  historyEvent("INFO","DEMARRAGE_ESP32","Firmware "+String(VERSION)+", raison reset="+String(int(esp_reset_reason()))+", boot="+String(historyBootId,HEX));
  historyPrune();
}

void historyTrackEvents(bool wbFresh,bool deyeFresh) {
  if(historyPreviousState!=evse_state.code_status) {
    const char *stop=historyStopKind(historyPreviousState,evse_state.code_status,targetA,millis(),historyLastZeroRequestAt);
    if(stop) historyEvent(strcmp(stop,"ARRET_NON_COMMANDE")==0?"CRITIQUE":"INFO",stop,
                         "Transition "+String(historyPreviousState)+" → "+String(evse_state.code_status));
    else if(evse_state.code_status==2) historyEvent("INFO","CHARGE_DEMARREE","Transition vers véhicule en charge");
    else historyEvent("INFO","ETAT_WB01","Transition "+String(historyPreviousState)+" → "+String(evse_state.code_status));
    historyPreviousState=evse_state.code_status;
  }
  if(historyPreviousWbValid!=wbFresh) {
    historyEvent(wbFresh?"INFO":"CRITIQUE",wbFresh?"LIAISON_WB01_RETABLIE":"PERTE_LIAISON_WB01",wbFresh?"Télémétrie WB-01 de nouveau valide":wbReadError);
    historyPreviousWbValid=wbFresh;
  }
  if(historyPreviousDeyeValid!=deyeFresh) {
    historyEvent(deyeFresh?"INFO":"AVERTISSEMENT",deyeFresh?"LIAISON_DEYE_RETABLIE":"PERTE_LIAISON_DEYE",deyeFresh?"Mesures Deye de nouveau valides":deye.error);
    historyPreviousDeyeValid=deyeFresh;
  }
  if(historyPreviousMode!=controlMode) {
    historyEvent("INFO","MODE_CHANGE","Mode "+historyPreviousMode+" → "+controlMode);
    historyPreviousMode=controlMode;
  }
  if(historyPreviousSocBlocked!=batterySocGuard.blocked) {
    historyEvent(batterySocGuard.blocked?"AVERTISSEMENT":"INFO",batterySocGuard.blocked?"ARRET_SOC":"REPRISE_SOC","SOC batterie="+String(deye.sample.soc)+" %");
    historyPreviousSocBlocked=batterySocGuard.blocked;
  }
  if(historyPreviousMeasurementBlocked!=solarMeasurements.blocked) {
    historyEvent(solarMeasurements.blocked?"CRITIQUE":"INFO",solarMeasurements.blocked?"MESURES_INCOHERENTES":"MESURES_REVALIDEES","Consommation maison="+String(solarMeasurements.faultHouseW)+" W");
    historyPreviousMeasurementBlocked=solarMeasurements.blocked;
  }
  String alert;
  if(controlMessage.indexOf("Défaut")>=0 || controlMessage.indexOf("Arrêt")>=0 || controlMessage.indexOf("bloqu")>=0) alert=controlMessage;
  if(alert.length() && alert!=historyPreviousControlAlert) historyEvent("AVERTISSEMENT","REGULATION",alert);
  historyPreviousControlAlert=alert;
}

void historyTick() {
  historyTemperature.tick();
  if(!historyStore.ready() || historyOperation) return;
  bool wbFresh=wbSampleValid && uint32_t(millis()-wbSampleAt)<=10000U;
  bool deyeFresh=deye.sample.valid && uint32_t(millis()-deye.sample.at)<=15000U;
  historyTrackEvents(wbFresh,deyeFresh);

  if(historyTemperature.valid()) {
    float t=historyTemperature.value();
    if(!historyTemperatureWarning && t>=HISTORY_TEMP_WARNING_C) {
      historyTemperatureWarning=true; historyEvent("CRITIQUE","TEMPERATURE_HAUTE","Seuil d'alerte atteint");
    } else if(historyTemperatureWarning && t<=HISTORY_TEMP_CLEAR_C) {
      historyTemperatureWarning=false; historyEvent("INFO","TEMPERATURE_REVENUE","Température revenue sous le seuil de réarmement");
    }
  }

  uint32_t now=millis();
  if(evse_state.code_status==2 && step_tcp==ATTENTE_REQUETE && uint32_t(now-historyLastSupply)>=HISTORY_SUPPLY_INTERVAL) {
    historyLastSupply=now;
    historySupplyRaw=Read_Evse("info_supply");
    historySupplyValid=historyParseSupply(historySupplyRaw.c_str(),historySupplyMin,historySupplyMax);
    bool warning=historySupplyValid && (historySupplyMin<HISTORY_LOW_VOLTAGE || historySupplyMax>HISTORY_HIGH_VOLTAGE);
    if(warning && !historySupplyWarning) historyEvent("CRITIQUE","TENSION_HORS_PLAGE","Diagnostic info_supply : "+historyCsvText(historySupplyRaw));
    else if(!warning && historySupplyWarning) historyEvent("INFO","TENSION_REVENUE","Diagnostic de tension revenu dans la plage");
    historySupplyWarning=warning;
  }
  if(uint32_t(now-historyLastSample)>=HISTORY_SAMPLE_INTERVAL) {
    historyLastSample=now;
    String line=historySampleLine();
    historyStore.appendSample(historyDate(),line,false);
    if(uint32_t(now-historyLastMinute)>=HISTORY_MINUTE_INTERVAL) {
      historyLastMinute=now;
      historyStore.appendSample(historyDate(),line,true);
    }
  }
}

struct WBParameter { String name,value,unit,minimum,maximum,description; };
WBParameter wbParameters[64];
size_t wbCount=0;
String wbListError;

bool identifier(const String &s) {
  if(s.isEmpty() || s.length()>48) return false;
  for(size_t i=0;i<s.length();i++) if(!isalnum((unsigned char)s[i]) && s[i]!='_') return false;
  return true;
}
bool integerValue(const String &s, long &v) {
  if(s.isEmpty() || s.length()>11) return false;
  char *end; v=strtol(s.c_str(),&end,10); return *end==0;
}
bool registerValue(const String &s,uint16_t &v) {
  if(s.isEmpty() || s.length()>5) return false;
  for(size_t i=0;i<s.length();i++) if(!isdigit((unsigned char)s[i])) return false;
  unsigned long value=strtoul(s.c_str(),nullptr,10);
  if(value>UINT16_MAX) return false;
  v=uint16_t(value); return true;
}
bool scaleValue(const String &s,float &v) {
  if(s.isEmpty() || s.length()>12) return false;
  char *end; float value=strtof(s.c_str(),&end);
  if(*end || !isfinite(value) || value<0.001f || value>100.0f) return false;
  v=value; return true;
}
bool colorValue(const String &s,uint32_t &v) {
  if(s.length()!=7 || s[0]!='#') return false;
  for(size_t i=1;i<7;i++) if(!isxdigit((unsigned char)s[i])) return false;
  v=strtoul(s.substring(1).c_str(),nullptr,16); return true;
}
String colorText(uint32_t color) {
  char text[8]; snprintf(text,sizeof(text),"#%06lx",static_cast<unsigned long>(color&0xffffff)); return String(text);
}
bool safeValue(const String &s) {
  if(s.isEmpty() || s.length()>32) return false;
  for(size_t i=0;i<s.length();i++) if(!isdigit((unsigned char)s[i]) && s[i]!='-' && s[i]!='.' && s[i]!=':') return false;
  return true;
}
int wifiSignalPercent(int rssi) {
  if(rssi<=-100) return 0;
  if(rssi>=-50) return 100;
  return (rssi+100)*2;
}
const char *lilygoLedName(LilygoLedState state) {
  return state==LILYGO_LED_STARTUP?"bleu · démarrage / attente":
         state==LILYGO_LED_READY?"vert · communications prêtes":
         state==LILYGO_LED_CHARGING?"violet · véhicule en charge":"rouge · défaut de communication";
}
void lilygoLedWrite(uint32_t color) {
  neopixelWrite(LILYGO_LED_PIN,uint8_t(color>>16),uint8_t(color>>8),uint8_t(color));
}
void lilygoLedBegin() {
  lilygoLedStartedAt=millis();
  lilygoLedLastState=LILYGO_LED_STARTUP;
  lilygoLedLastColor=0xffffffff;
  lilygoLedWrite(lilygoLedColors[LILYGO_LED_STARTUP]);
}
void lilygoLedTick() {
  uint32_t now=millis();
  LilygoLedState next=lilygoLedSelect(now,lilygoLedStartedAt,
      deye.fullyConfigured(),deye.sample.valid,deye.sample.at,
      wbSampleValid,wbSampleAt,evse_state.code_status==2);
  if(next!=LILYGO_LED_FAULT || !lilygoLedFaultBlink) lilygoLedFaultVisible=true;
  else if(uint32_t(now-lilygoLedLastBlinkAt)>=500U) {
    lilygoLedLastBlinkAt=now; lilygoLedFaultVisible=!lilygoLedFaultVisible;
  }
  uint32_t color=lilygoLedEnabled && lilygoLedFaultVisible?lilygoLedColors[next]:0;
  if(next!=lilygoLedLastState || color!=lilygoLedLastColor) {
    lilygoLedLastState=next; lilygoLedLastColor=color;
    lilygoLedWrite(color);
  }
}
bool validWifiSsid(const String &ssid) {
  if(ssid.isEmpty() || ssid.length()>31) return false;
  for(size_t i=0;i<ssid.length();i++) if((uint8_t)ssid[i]<32) return false;
  return true;
}
void webMaintenanceTick() {
  if(wifiConnectAt && (int32_t)(millis()-wifiConnectAt)>=0) {
    wifiConnectAt=0;
    WiFi.disconnect(false,false);
    delay(100);
    WiFi.begin(pendingWifiSsid.c_str(),pendingWifiPassword.length()?pendingWifiPassword.c_str():nullptr);
    wifiReconnect.noteAttempt();
    pendingWifiSsid="";
    pendingWifiPassword="";
  }
  if(firmwareRestartAt && (int32_t)(millis()-firmwareRestartAt)>=0) ESP.restart();
}
bool webAuth(bool write=false, bool protectedSettings=true) {
  server.sendHeader("Cache-Control","no-store");
  if(protectedSettings && !server.authenticate(WEB_USER,WEB_PASSWORD)) {
    server.requestAuthentication(BASIC_AUTH,"VE-Tronic"); return false;
  }
  if(write && server.header("X-CSRF-Token")!=csrfToken) {
    server.send(403,"text/plain; charset=utf-8","Recharger la page (jeton absent)."); return false;
  }
  return true;
}
void jsonSend(JsonDocument &doc) {
  if(doc.overflowed()) { server.send(503,"text/plain","Mémoire JSON insuffisante"); return; }
  String body; serializeJson(doc,body); server.send(200,"application/json",body);
}

// ========== setCurrent avec double tentative ==========
bool setCurrent(int amps) {
  int previousTarget=targetA;
  lastRequestedA=amps;
  if(amps==0) historyLastZeroRequestAt=millis();
  currentConfirmed=false;
  currentDelivered=false;
  currentReadback="";
  String answer = Read_Evse("$SC "+String(amps));
  if (answer.indexOf("Erreur : réponse série incomplète") >= 0 || answer.indexOf("Pas de reponse du Vetronic") >= 0) {
    // Deuxième essai après 200 ms pour laisser la ligne se stabiliser
    delay(200);
    answer = Read_Evse("$SC "+String(amps));
    if (answer.indexOf("Erreur : réponse série incomplète") >= 0 || answer.indexOf("Pas de reponse du Vetronic") >= 0) {
      lastCurrentReply=answer;
      controlMessage = "Défaut de communication série : "+answer;
      return false;
    }
  }
  lastCurrentReply=answer;
  String lower=answer; lower.toLowerCase();
  bool acknowledged=answer.indexOf("$OK")>=0;
  if((!acknowledged && !wbCurrentShellReply(answer.c_str(),amps)) || lower.indexOf("error")>=0 || lower.indexOf("erreur")>=0 || lower.indexOf("unknown")>=0) {
    controlMessage="Consigne $SC "+String(amps)+" non confirmée : "+answer;
    return false;
  }
  currentDelivered=true;
  // Read back the effective limit; it can be lower because of WB-01 protections,
  // or zero while the vehicle is disconnected. Never bypass these protections.
  currentReadback=Read_Evse("$GG*B2");
  unsigned ma=0,v=0,cap=0;
  currentConfirmed=amps>=0 && wbParseValues(currentReadback.c_str(),ma,v,cap) && cap==unsigned(amps)*1000U;
  targetA = amps;
  if(previousTarget!=amps)
    historyEvent("INFO","CONSIGNE","Consigne "+String(previousTarget)+" A → "+String(amps)+" A"+(currentConfirmed?" (confirmée)":" (non confirmée par relecture)"));
  return true;
}

// ========== Lecture de la borne avec double tentative et conservation de validité ==========
static bool refreshWBOnce() {
  String state = Read_Evse("evse_state"), values = Read_Evse("$GG*B2");
  lastStateReply=state; lastValuesReply=values;
  int p = state.indexOf("EVSE state :"), q = values.indexOf("$OK ");
  unsigned int code=99, ma=0, v=0, cap=0;
  WBTelemetry telemetry;
  bool ok=wbParseTelemetry(state.c_str(),values.c_str(),telemetry);
  code=telemetry.state; ma=telemetry.milliamps; v=telemetry.volts; cap=telemetry.limitMilliamps;
  if(ok) {
    wbReadError="";
    calculationVolts=wbCalculationVolts(telemetry);
    evse_state.code_status = code;
    evse_state.courant = ma;
    evse_state.tension = v;
    evse_state.courant_max = cap;
    snprintf(evse_state.txt_status,sizeof(evse_state.txt_status),"%s",
             code==2?"Véhicule en charge":code==1?"Câble connecté":"Câble déconnecté");
    wbSampleAt = millis();
    wbSampleValid = true;
  } else {
    if(p<0) wbReadError="Réponse evse_state absente ou non reconnue";
    else if(q<0) wbReadError="Réponse $GG*B2 absente ou non reconnue";
    else if(sscanf(values.c_str()+q,"$OK %u %u %u",&ma,&v,&cap)!=3) wbReadError="Format $GG*B2 invalide : trois valeurs attendues";
    else if(v<180 || v>260) wbReadError="Tension WB-01 hors plage : "+String(v)+" V (180 à 260 V attendus)";
    else if(code>2) wbReadError="État WB-01 non exploitable : "+String(code);
    else wbReadError="Courant WB-01 hors plage (0 à 63000 mA attendus)";
  }
  if(!ok && p>=0 && q>=0) { wbSampleValid=false; calculationVolts=0; }
  return ok;
}

bool refreshWB() {
  if (refreshWBOnce()) return true;
  // Deuxième tentative après un court délai
  delay(200);
  return refreshWBOnce();
}

bool loadWBParameters() {
  String raw=Read_Evse("list"); wbCount=0; wbListError="";
  for(int start=0;start<(int)raw.length();) {
    int end=raw.indexOf('\n',start); if(end<0) end=raw.length();
    String line=raw.substring(start,end); start=end+1;
    String fields[6]; int from=0; bool complete=true;
    for(int i=0;i<5;i++) {
      int sep=line.indexOf(';',from); if(sep<0) { complete=false; break; }
      fields[i]=line.substring(from,sep); fields[i].trim(); from=sep+1;
    }
    if(!complete || !identifier(fields[0]) || fields[0]=="name") continue;
    fields[5]=line.substring(from); fields[5].trim();
    if(wbCount>=64) { wbListError="Trop de paramètres : liste incomplète"; wbCount=0; return false; }
    wbParameters[wbCount++]={fields[0],fields[1],fields[2],fields[3],fields[4],fields[5]};
  }
  if(raw.length()>=BUFFERSIZE-1 || !wbCount) { wbListError="Liste absente/incomplète : vérifier la liaison et le firmware WB-01"; wbCount=0; return false; }
  return true;
}
String unlockWB(const String &pin) {
  if(pin.isEmpty()) return "";
  if(pin.length()!=6) return "Mot de passe WB-01 : six chiffres requis";
  for(size_t i=0;i<6;i++) if(!isdigit((unsigned char)pin[i])) return "Mot de passe WB-01 invalide";
  String r=Read_Evse("passwd "+pin); String lower=r; lower.toLowerCase();
  return lower.indexOf("password ok")>=0 ? "" : "Déverrouillage refusé : "+r;
}
String writeWB(const String &name,const String &value) {
  String r=Read_Evse("set "+name+" "+value);
  if(r.indexOf("parameter '"+name+"' set to ")<0) return "Modification non confirmée : "+r;
  return "";
}
String prepareWB(const String &pin, bool allowChanges=false) {
  if(!loadWBParameters()) return wbListError;
  bool needsUnlock=false, foundCharge=false, foundLimit=false;
  nativeLimitA=63;
  for(size_t i=0;i<wbCount;i++) {
    auto &p=wbParameters[i];
    if(p.name=="charge_mode") foundCharge=true;
    if(p.name=="network_type" && p.value!="0") return "Ce mode de pilotage est prévu pour une borne monophasée";
    long current;
    if(p.name.startsWith("max_current") && integerValue(p.value,current) && current>=6 && current<=63) {
      nativeLimitA=min(nativeLimitA,int(current)); foundLimit=true;
    }
    if((p.name=="charge_mode" || p.name=="solar_mode") && p.value!="0") needsUnlock=true;
  }
  if(!foundCharge) return "Paramètre charge_mode non disponible";
  if(!foundLimit) return "Courant maximal WB-01 non disponible : démarrage refusé";
  if(needsUnlock) {
    if(!allowChanges) return "Ouvrir Paramètres et utiliser Préparer le pilotage pour configurer les modes WB-01.";
    String e=unlockWB(pin); if(!e.isEmpty()) return e;
    bool changed=false;
    for(size_t i=0;i<wbCount;i++) {
      auto &p=wbParameters[i];
      if((p.name=="charge_mode" || p.name=="solar_mode") && p.value!="0") {
        e=writeWB(p.name,"0"); if(!e.isEmpty()) return e;
        changed=true;
      }
    }
    if(changed) {
      Read_Evse("reset");
      delay(4000);
      bool backUp=false;
      for(int attempt=0; attempt<8 && !backUp; attempt++) {
        String v=Read_Evse("version");
        if(v.length()>0 && v.indexOf("Erreur")<0 && v.indexOf("Pas de reponse")<0) backUp=true;
        else delay(500);
      }
      if(!backUp) return "La borne n'a pas répondu après redémarrage (mode probablement appliqué, réessayer dans quelques instants).";
      if(!loadWBParameters()) return wbListError;
    }
  }
  return "";
}
void saveControlConfig() {
  Preferences p; p.begin("solar-web",false);
  p.putString("host",deye.host); p.putUInt("serial",deye.serial); p.putInt("limit",limitA);
  p.putBool("rs485",deye.useRS485); p.putUInt("baud",deye.baud);
  p.putUChar("slave",deye.slave); p.putString("parity",deye.parity);
  p.putUShort("regGrid",deye.registers.grid); p.putUShort("regLoad",deye.registers.load);
  p.putUShort("regSoc",deye.registers.soc); p.putUShort("regPv1",deye.registers.pv1);
  p.putUShort("regPv2",deye.registers.pv2); p.putUShort("regPv3",deye.registers.pv3);
  p.putUShort("regBat",deye.registers.battery);
  p.putFloat("pv1Scale",deye.pv1Scale); p.putFloat("pv2Scale",deye.pv2Scale); p.putFloat("pv3Scale",deye.pv3Scale);
  p.putFloat("loadScale",deye.loadScale); p.putFloat("gridScale",deye.gridScale);
  p.putFloat("batScale",deye.batteryScale); p.putFloat("socScale",deye.socScale);
  p.putBool("socGuard",batterySocEnabled); p.putInt("socStop",batterySocStop); p.putInt("socResume",batterySocResume);
  p.putBool("includes",loadIncludesEV); p.putBool("meter",meterConfirmed);
  p.putBool("pv3",deye.thirdMppt);
  p.end();
}
void saveLedConfig() {
  Preferences p; p.begin("solar-web",false);
  p.putBool("ledOn",lilygoLedEnabled); p.putBool("ledBlink",lilygoLedFaultBlink);
  p.putUInt("ledStart",lilygoLedColors[LILYGO_LED_STARTUP]);
  p.putUInt("ledReady",lilygoLedColors[LILYGO_LED_READY]);
  p.putUInt("ledCharge",lilygoLedColors[LILYGO_LED_CHARGING]);
  p.putUInt("ledFault",lilygoLedColors[LILYGO_LED_FAULT]);
  p.end();
}
void rememberMode() {
  Preferences p; p.begin("solar-web",false); p.putBool("managed",controlMode!="legacy"); p.end();
}

void saveSolarMeasurementFault() {
  Preferences p; p.begin("solar-web",false);
  p.putBool("measureFault",solarMeasurements.blocked);
  p.putInt("faultHouseW",solarMeasurements.faultHouseW); p.end();
}

// Point d'entrée unique pour les boutons web et les commandes Jeedom du port 9200.
// Garder les vérifications ici évite qu'une commande distante ne contourne les
// protections appliquées depuis la page Pilotage.
String applyControlMode(String mode, int manualAmps=0) {
  mode.trim();
  mode.toLowerCase();
  if(mode!="stop" && mode!="manual" && mode!="solar" && mode!="legacy") return "Mode invalide";
  if(mode=="solar" && (!meterConfirmed || !deye.fullyConfigured()))
    return "Configurer Deye et confirmer les mesures avant activation";

  solarStartupPending=false;
  controlMode="stop"; solar.reset(); deyeLoss.reset(); batterySocGuard.reset(); rememberMode();
  if(!setCurrent(0)) return controlMessage;
  if(mode=="manual" || mode=="solar") {
    String e=prepareWB("");
    if(!e.isEmpty()) return e;
  }
  if(mode=="manual") {
    const int manualLimit=wb_manual_current_limit(limitA);
    if(manualAmps==0) manualAmps=manualLimit; // Compatibilité Jeedom historique.
    if(manualAmps<6 || manualAmps>manualLimit)
      return "Intensité manuelle invalide (6 à "+String(manualLimit)+" A)";
    if(!setCurrent(manualAmps)) return controlMessage;
  }
  if(mode=="legacy" && !setCurrent(-1)) return controlMessage;
  if(mode=="solar") { solarMeasurements.reset(); saveSolarMeasurementFault(); }
  controlMode=mode;
  controlMessage="Mode demandé : "+mode;
  rememberMode();
  lastControl=millis()-5000;
  return "";
}

// Syntaxe destinée aux appels historiques Jeedom : /$MODE%20solar
// (l'espace peut également être envoyé tel quel par les anciennes commandes).
bool parseJeedomModeCommand(String command, String &mode) {
  command.trim();
  if(!command.startsWith("$MODE ")) return false;
  mode=command.substring(6);
  mode.trim();
  mode.toLowerCase();
  return true;
}

// ========== controlTick modifié pour la tolérance ==========
void controlTick() {
  deye.tick();
  bool deyeFresh = deye.sample.valid && (millis() - deye.sample.at <= 15000); // 15s au lieu de 7.5s

  if(controlMode=="solar") deyeLoss.update(millis(),deyeFresh);
  else { deyeLoss.reset(); batterySocGuard.reset(); }

  if((uint32_t)(millis()-lastControl)<5000 || step_tcp!=ATTENTE_REQUETE) return;
  lastControl=millis();

  // Lecture de la borne (avec double tentative)
  bool wbResponding=refreshWB();
  if(!wbResponding && (!wbSampleValid || uint32_t(millis()-wbSampleAt)>10000U))
    wbReconnectPending=true;
  if(wbResponding && wbReconnectPending) {
    wbReconnectPending=false;
    solarStartupPending=true;
    controlMode="solar";
    solar.reset(); deyeLoss.reset(); batterySocGuard.reset();
  }

  // Une WB-01 qui redémarre retrouve le solaire sans demander une seconde
  // action Web. Une sélection explicite de mode annule cette reprise.
  if(solarStartupPending) {
    if(!setCurrent(0)) return;
    if(!meterConfirmed || !deye.fullyConfigured()) {
      controlMessage="Solaire par défaut : configurer Deye et confirmer les mesures";
      return;
    }
    if(!wbResponding) return;
    String error=prepareWB("");
    if(!error.isEmpty()) {
      controlMessage="Solaire par défaut en attente : "+error;
      return;
    }
    solarStartupPending=false;
    solar.reset(); deyeLoss.reset(); batterySocGuard.reset();
    controlMessage="Solaire dynamique prêt : attente de mesures fraîches";
    return;
  }

  // Fraîcheur de la borne : 10 secondes de validité (au lieu de zéro)
  bool wbFresh = wbSampleValid && (millis() - wbSampleAt <= 10000);

  if(controlMode=="legacy") return;

  if(controlMode=="stop") {
    if(targetA!=0 || !wbFresh || evse_state.courant_max!=0) setCurrent(0);
    if(currentConfirmed) controlMessage="Mode arrêt : sélectionner Solaire dynamique sur Pilotage pour activer la régulation";
    return;
  }
  if(controlMode=="manual") return;

  // No vehicle power is drawn with a de-energized outlet. The 230 V reference
  // is only for calculating a start command, never presented as a measurement.
  // Serial reads can take several seconds; recheck Deye age afterwards.
  deyeFresh = deye.sample.valid && uint32_t(millis()-deye.sample.at)<=15000U;
  deyeLoss.update(millis(),deyeFresh);
  bool batteryBlocked=batterySocGuard.update(batterySocEnabled,deyeFresh,deye.sample.soc,
                                             batterySocStop,batterySocResume);
  const SolarPower power(deye.sample.pv,deye.sample.load,loadIncludesEV,
                         wbFresh,evse_state.code_status,evse_state.courant,calculationVolts);
  bool fresh = wbFresh && deyeFresh && meterConfirmed;
  bool wasBlocked=solarMeasurements.blocked;
  solarMeasurements.update(millis(),fresh,power.houseW);
  if(solarMeasurements.blocked!=wasBlocked) saveSolarMeasurementFault();
  int surplus = power.surplusW;
  int amps;

  if(batteryBlocked || solarMeasurements.blocked) {
    solar.reset();
    amps=0;
  } else if(solarMeasurements.pending) {
    amps=solarMeasurements.holdCurrent(millis(),wbFresh && meterConfirmed,
             solar.running || solar.probing,targetA,solar.bridging,solar.deficitSince,
             solar.probing,solar.probeStarted);
    amps=min(amps,wb_solar_current_limit(limitA));
    if(amps==0) solar.decide(millis(),false,0,0,0,calculationVolts,wb_solar_current_limit(limitA));
  } else if(!deyeFresh) {
    amps = deyeLoss.current(millis(), wbFresh && meterConfirmed,
                            solar.running, targetA,
                            solar.bridging, solar.deficitSince);
    if(amps==0) {
      solar.decide(millis(), false, 0, 0, 0, calculationVolts, wb_solar_current_limit(limitA));
    }
  } else {
    amps = solar.decide(millis(), fresh, surplus, deye.sample.battery,
                        deye.sample.grid, calculationVolts, wb_solar_current_limit(limitA));
  }

  if(!setCurrent(amps)) {
    solar.reset();
    setCurrent(0);
    controlMessage="Défaut liaison borne : arrêt demandé";
  } else if(batteryBlocked) {
    controlMessage="Charge solaire bloquée : batterie Deye à "+String(deye.sample.soc)+" % ; reprise à "+String(batterySocResume)+" %";
  } else if(solarMeasurements.blocked) {
    controlMessage="Solaire verrouillé : consommation maison incohérente ("+String(solarMeasurements.faultHouseW)+" W). Vérifier les mesures et l'inclusion du VE, puis réactiver Solaire dynamique";
  } else if(solarMeasurements.pending) {
    controlMessage=amps>0 ? "Synchronisation des mesures Deye / VE : consigne maintenue, maximum 15 s"
                          : "Attente de mesures Deye / VE cohérentes : charge arrêtée";
  } else if(!deyeFresh) {
    controlMessage = amps>0 ? "Connexion Deye perdue : dernière consigne maintenue, arrêt dans "+String((deyeLoss.remaining(millis())+999)/1000)+" s"
                            : "Deye indisponible : charge arrêtée, attente du retour des mesures";
  } else {
    if(!wbFresh) controlMessage="Arrêt solaire : "+wbReadError;
    else if(!meterConfirmed) controlMessage="Arrêt solaire : validation des mesures non cochée dans Paramètres";
    else if(solar.probing) controlMessage="Test de déblocage de la production solaire (courant minimal, 2 min)";
    else if(amps==0) controlMessage="Attente solaire : surplus "+String(surplus)+" W / seuil 1800 W"+(solar.recoveryRequired?" ; attente de fin de décharge batterie":"");
    else if(solar.bridging) controlMessage="Maintien à 6 A : appoint batterie ou réseau (maximum 5 minutes)";
    else controlMessage="Charge solaire dynamique";
  }
}

void beginControl() {
  Preferences p; p.begin("solar-web",true);
  deye.host=p.getString("host",""); deye.serial=p.getUInt("serial",0);
  // Anciennes configurations : Wi-Fi conservé si la nouvelle clé est absente.
  deye.useRS485=p.getBool("rs485",false); deye.baud=p.getUInt("baud",9600);
  deye.slave=p.getUChar("slave",1); deye.parity=p.getString("parity","8N1");
  deye.registers.grid=p.getUShort("regGrid",169); deye.registers.load=p.getUShort("regLoad",178);
  deye.registers.soc=p.getUShort("regSoc",184); deye.registers.pv1=p.getUShort("regPv1",186);
  deye.registers.pv2=p.getUShort("regPv2",187); deye.registers.pv3=p.getUShort("regPv3",188);
  deye.registers.battery=p.getUShort("regBat",190);
  limitA=constrain(p.getInt("limit",32),6,63);
  loadIncludesEV=p.getBool("includes",true); meterConfirmed=p.getBool("meter",false);
  solarMeasurements.blocked=p.getBool("measureFault",false);
  solarMeasurements.faultHouseW=p.getInt("faultHouseW",0);
  deye.thirdMppt=p.getBool("pv3",true); deye.loadScale=p.getFloat("loadScale",10); deye.gridScale=p.getFloat("gridScale",10);
  deye.pv1Scale=p.getFloat("pv1Scale",1); deye.pv2Scale=p.getFloat("pv2Scale",1); deye.pv3Scale=p.getFloat("pv3Scale",1);
  deye.batteryScale=p.getFloat("batScale",1); deye.socScale=p.getFloat("socScale",1);
  batterySocEnabled=p.getBool("socGuard",false); batterySocStop=constrain(p.getInt("socStop",30),0,95);
  batterySocResume=constrain(p.getInt("socResume",35),5,100);
  if(batterySocResume<batterySocStop+5) batterySocResume=min(100,batterySocStop+5);
  lilygoLedEnabled=p.getBool("ledOn",true); lilygoLedFaultBlink=p.getBool("ledBlink",false);
  lilygoLedColors[LILYGO_LED_STARTUP]=p.getUInt("ledStart",0x000030);
  lilygoLedColors[LILYGO_LED_READY]=p.getUInt("ledReady",0x003000);
  lilygoLedColors[LILYGO_LED_CHARGING]=p.getUInt("ledCharge",0x280030);
  lilygoLedColors[LILYGO_LED_FAULT]=p.getUInt("ledFault",0x380000);
  controlMode="solar";
  solarStartupPending=true;
  p.end();
  lilygoLedStartedAt=millis();
  deye.configure();
  setCurrent(0);
  csrfToken=String(esp_random(),HEX)+String(esp_random(),HEX);
  const char *headers[]={"X-CSRF-Token"}; server.collectHeaders(headers,1);
  server.on("/pilotage",HTTP_GET,[](){ if(webAuth(false,false)) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
  server.on("/parametres",HTTP_GET,[](){ if(webAuth()) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
  server.on("/",HTTP_GET,[](){ if(webAuth(false,false)) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
  // Jeedom expose une clé API : protégée comme /parametres (l'ancienne page ne l'était pas).
  server.on("/jeedom",HTTP_GET,[](){ if(webAuth()) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
  server.on("/wifi",HTTP_GET,[](){ if(webAuth()) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
  server.on("/historique",HTTP_GET,[](){ if(webAuth(false,false)) server.send_P(200,"text/html; charset=utf-8",HISTORY_HTML); });
  server.on("/api/history/status",HTTP_GET,[](){
    if(!webAuth(false,false)) return;
    DynamicJsonDocument d(1024);
    d["ready"]=historyStore.ready(); d["error"]=historyStore.error();
    d["cardBytes"]=historyStore.cardBytes(); d["totalBytes"]=historyStore.totalBytes(); d["usedBytes"]=historyStore.usedBytes();
    d["cardBytesText"]=historyStore.ready()?historyByteText(historyStore.cardBytes()):"—";
    d["lastWriteSeconds"]=historyStore.lastWriteAt()?int(uint32_t(millis()-historyStore.lastWriteAt())/1000U):-1;
    d["temperatureValid"]=historyTemperature.valid();
    d["temperatureC"]=historyTemperature.valid()?historyTemperature.value():0;
    d["temperatureStatus"]=historyTemperature.status();
    d["supplyValid"]=historySupplyValid; d["supplyMinV"]=historySupplyMin; d["supplyMaxV"]=historySupplyMax;
    jsonSend(d);
  });
  server.on("/api/history/file",HTTP_GET,[](){
    if(!webAuth(false,false)) return;
    if(!historyStore.ready()) { server.send(503,"text/plain; charset=utf-8",historyStore.error()); return; }
    String kind=server.arg("kind"),date=server.arg("date"),path=historyStore.filePath(kind,date);
    if(path.isEmpty()) { server.send(400,"text/plain; charset=utf-8","Type ou date invalide"); return; }
    File file=SD.open(path,FILE_READ);
    if(!file) { server.send(404,"text/plain; charset=utf-8","Fichier absent"); return; }
    server.sendHeader("Content-Disposition","inline; filename=\""+date+(kind=="events"?".log":".csv")+"\"");
    server.streamFile(file,kind=="events"?"text/plain; charset=utf-8":"text/csv; charset=utf-8");
    file.close();
  });
  server.on("/api/history/clear",HTTP_POST,[](){
    if(!webAuth(true)) return;
    if(server.arg("confirm")!="EFFACER") { server.send(400,"text/plain; charset=utf-8","Confirmation EFFACER requise"); return; }
    if(evse_state.code_status==2) { server.send(409,"text/plain; charset=utf-8","Effacement refusé pendant une charge"); return; }
    historyOperation=true; bool ok=historyStore.clearHistory(); historyOperation=false;
    if(!ok) { server.send(500,"text/plain; charset=utf-8",historyStore.error()); return; }
    historyEvent("INFO","HISTORIQUE_EFFACE","Historique effacé depuis l'interface web");
    server.send(200,"text/plain; charset=utf-8","Historique effacé. Un nouveau journal a été créé.");
  });
  server.on("/api/history/format",HTTP_POST,[](){
    if(!webAuth(true)) return;
    if(server.arg("confirm")!="FORMATER") { server.send(400,"text/plain; charset=utf-8","Confirmation FORMATER requise"); return; }
    if(evse_state.code_status==2) { server.send(409,"text/plain; charset=utf-8","Formatage refusé pendant une charge"); return; }
    historyOperation=true; bool ok=historyStore.formatFat(); historyOperation=false;
    if(!ok) { server.send(500,"text/plain; charset=utf-8",historyStore.error()); return; }
    historyEvent("INFO","CARTE_FORMATEE","Carte SD formatée depuis l'interface web");
    server.send(200,"text/plain; charset=utf-8","Carte formatée en FAT, remontée et testée.");
  });
  server.on("/api/command",HTTP_POST,[](){
    if(!webAuth(true,false)) return; // CSRF requis ; auth conditionnelle selon la commande ci-dessous.
    String cmd=server.arg("commande");
    if(!legacyPublicCommand(cmd) && !server.authenticate(WEB_USER,WEB_PASSWORD)) {
      server.requestAuthentication(BASIC_AUTH,"VE-Tronic"); return;
    }
    if(controlMode!="legacy") { server.send(409,"text/plain; charset=utf-8","Commande suspendue : rendre la main à la borne / Jeedom depuis Pilotage."); return; }
    str_mem_commande=cmd;
    server.send(200,"text/plain; charset=utf-8",Read_Evse(cmd));
  });
  server.on("/api/jeedom",HTTP_GET,[](){
    if(!webAuth()) return;
    DynamicJsonDocument d(512);
    d["token"]=csrfToken;
    d["ip_jeedom"]=params.ip_jeedom; d["api_key_jeedom"]=params.api_key_jeedom;
    d["id_code_status"]=params.id_code_status; d["id_txt_status"]=params.id_txt_status;
    d["id_tension"]=params.id_tension; d["id_courant"]=params.id_courant; d["id_courant_max"]=params.id_courant_max;
    d["freq_update_evse"]=params.freq_update_evse;
    jsonSend(d);
  });
  server.on("/api/jeedom",HTTP_POST,[](){
    if(!webAuth(true)) return;
    memset(&params.ip_jeedom,'\0',sizeof(param_t::ip_jeedom));
    strncpy(params.ip_jeedom,server.arg("ip_jeedom").c_str(),sizeof(param_t::ip_jeedom)-1);
    memset(&params.api_key_jeedom,'\0',sizeof(param_t::api_key_jeedom));
    strncpy(params.api_key_jeedom,server.arg("api_key_jeedom").c_str(),sizeof(param_t::api_key_jeedom)-1);
    params.id_code_status=server.arg("id_code_status").toInt();
    params.id_txt_status=server.arg("id_txt_status").toInt();
    params.id_tension=server.arg("id_tension").toInt();
    params.id_courant=server.arg("id_courant").toInt();
    params.id_courant_max=server.arg("id_courant_max").toInt();
    params.freq_update_evse=server.arg("freq_update_evse").toInt();
    putParams(params);
    server.send(200,"text/plain; charset=utf-8","Paramètres Jeedom enregistrés.");
  });
  server.on("/api/wifi",HTTP_GET,[](){
    if(!webAuth()) return;
    DynamicJsonDocument d(6144);
    bool connected=WiFi.status()==WL_CONNECTED;
    int rssi=connected?WiFi.RSSI():0;
    d["connected"]=connected; d["ssid"]=connected?WiFi.SSID():"";
    d["rssi"]=rssi; d["percent"]=connected?wifiSignalPercent(rssi):0;
    d["ip"]=connected?WiFi.localIP().toString():"";
    d["apSsid"]=DIRECT_AP_SSID; d["apIp"]=WiFi.softAPIP().toString();
    d["apStations"]=WiFi.softAPgetStationNum();
    JsonArray saved=d.createNestedArray("saved");
    AutoConnectCredential credentials;
    station_config_t credential;
    uint8_t entries=min((uint8_t)10,credentials.entries());
    for(uint8_t i=0;i<entries;i++) {
      memset(&credential,0,sizeof(credential));
      if(credentials.load(i,&credential)) saved.add((const char*)credential.ssid);
    }
    jsonSend(d);
  });
  server.on("/api/wifi",HTTP_POST,[](){
    if(!webAuth(true)) return;
    String action=server.arg("action"),ssid=server.arg("ssid");
    if(action=="scan") {
      DynamicJsonDocument d(4096);
      JsonArray networks=d.createNestedArray("networks");
      int found=WiFi.scanNetworks();
      if(found>0) for(int i=0;i<found && i<12;i++) {
        String name=WiFi.SSID(i);
        if(name.isEmpty()) continue;
        JsonObject network=networks.createNestedObject();
        int signal=WiFi.RSSI(i);
        network["ssid"]=name; network["rssi"]=signal; network["percent"]=wifiSignalPercent(signal);
        network["secured"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
      }
      WiFi.scanDelete();
      jsonSend(d); return;
    }
    if(!validWifiSsid(ssid)) { server.send(400,"text/plain; charset=utf-8","SSID invalide (1 à 31 caractères imprimables)."); return; }
    AutoConnectCredential credentials;
    if(action=="delete") {
      if(!credentials.del(ssid.c_str())) { server.send(404,"text/plain; charset=utf-8","Réseau mémorisé introuvable."); return; }
      server.send(200,"text/plain; charset=utf-8","Réseau mémorisé supprimé."); return;
    }
    String password=server.arg("password");
    if(action!="connect" || password.length()>63) { server.send(400,"text/plain; charset=utf-8","Demande Wi-Fi invalide."); return; }
    station_config_t credential;
    memset(&credential,0,sizeof(credential));
    ssid.toCharArray((char*)credential.ssid,sizeof(credential.ssid));
    password.toCharArray((char*)credential.password,sizeof(credential.password));
    if(!credentials.save(&credential)) { server.send(500,"text/plain; charset=utf-8","Impossible de mémoriser le réseau."); return; }
    pendingWifiSsid=ssid; pendingWifiPassword=password; wifiConnectAt=millis()+700;
    server.send(200,"text/plain; charset=utf-8","Réseau mémorisé. Connexion en cours…");
  });
  server.on("/api/update",HTTP_POST,[](){
    bool ok=firmwareUploadAllowed && firmwareUploadSuccess;
    firmwareUploadAllowed=false;
    if(!ok) { server.send(500,"text/plain; charset=utf-8",firmwareUploadError.length()?firmwareUploadError:"Mise à jour interrompue."); return; }
    firmwareRestartAt=millis()+800;
    server.send(200,"text/plain; charset=utf-8","Mise à jour installée. Redémarrage de la borne…");
  },[](){
    HTTPUpload &upload=server.upload();
    if(upload.status==UPLOAD_FILE_START) {
      firmwareUploadAllowed=server.authenticate(WEB_USER,WEB_PASSWORD) && server.header("X-CSRF-Token")==csrfToken;
      firmwareUploadSuccess=false; firmwareUploadError="";
      if(!firmwareUploadAllowed) { firmwareUploadError="Accès refusé."; return; }
      if(!Update.begin(UPDATE_SIZE_UNKNOWN)) firmwareUploadError="Espace firmware insuffisant.";
    } else if(upload.status==UPLOAD_FILE_WRITE) {
      if(firmwareUploadAllowed && !firmwareUploadError.length() && Update.write(upload.buf,upload.currentSize)!=upload.currentSize)
        firmwareUploadError="Écriture du firmware impossible.";
    } else if(upload.status==UPLOAD_FILE_END) {
      if(firmwareUploadAllowed && !firmwareUploadError.length() && Update.end(true)) firmwareUploadSuccess=true;
      else if(!firmwareUploadError.length()) firmwareUploadError="Firmware invalide ou incomplet.";
    } else if(upload.status==UPLOAD_FILE_ABORTED) {
      Update.abort(); firmwareUploadError="Envoi du firmware annulé.";
    }
  });
  server.on("/api/status",HTTP_GET,[](){
    if(!webAuth(false,false)) return;
    DynamicJsonDocument d(6144);
    d["token"]=csrfToken; d["mode"]=controlMode; d["message"]=controlMessage;
    bool wifiConnected=WiFi.status()==WL_CONNECTED;
    int wifiRssi=wifiConnected?WiFi.RSSI():0;
    d["wifiConnected"]=wifiConnected; d["wifiSsid"]=wifiConnected?WiFi.SSID():"";
    d["wifiRssi"]=wifiRssi; d["wifiPercent"]=wifiConnected?wifiSignalPercent(wifiRssi):0;
    d["apSsid"]=DIRECT_AP_SSID; d["apIp"]=WiFi.softAPIP().toString();
    d["apStations"]=WiFi.softAPgetStationNum();
    d["targetA"]=targetA; d["wbValid"]=wbSampleValid && millis()-wbSampleAt<10000;
    d["requestedA"]=lastRequestedA; d["currentConfirmed"]=currentConfirmed;
    d["currentDelivered"]=currentDelivered; d["currentReadback"]=currentReadback.substring(0,512);
    d["calculationVolts"]=calculationVolts;
    d["voltageReference"]=wbSampleValid && calculationVolts==230 && (evse_state.tension<180 || evse_state.tension>260);
    d["wbReadError"]=wbReadError;
    d["currentReply"]=lastCurrentReply.substring(0,512);
    d["stateReply"]=lastStateReply.substring(0,512);
    d["valuesReply"]=lastValuesReply.substring(0,512);
    d["nativeLimitA"]=nativeLimitA;
    d["manualLimitA"]=wb_manual_current_limit(limitA);
    d["solarLimitA"]=wb_solar_current_limit(limitA);
    const SolarPower power(deye.sample.pv,deye.sample.load,loadIncludesEV,
                           wbSampleValid && uint32_t(millis()-wbSampleAt)<=10000U,
                           evse_state.code_status,evse_state.courant,calculationVolts);
    d["surplusW"]=power.surplusW; d["houseW"]=power.houseW; d["evPowerW"]=power.evW;
    d["measurementPending"]=solarMeasurements.pending;
    d["measurementBlocked"]=solarMeasurements.blocked;
    d["measurementFaultHouseW"]=solarMeasurements.faultHouseW;
    d["state"]=evse_state.code_status; d["volts"]=evse_state.tension; d["amps"]=evse_state.courant/1000.0;
    uint32_t deyeAgeMs=deye.sample.valid ? uint32_t(millis()-deye.sample.at) : 0;
    bool deyeValid=deye.sample.valid && deyeAgeMs<=15000;
    d["deyeValid"]=deyeValid;
    d["deyeAgeSeconds"]=deye.sample.valid ? int((deyeAgeMs+999)/1000) : -1;
    d["deyeUsingLastGood"]=deyeValid && !deye.error.isEmpty();
    d["deyeTimeoutSeconds"]=deyeLoss.remaining(millis())/1000;
    d["pv"]=deye.sample.pv; d["load"]=deye.sample.load; d["battery"]=deye.sample.battery; d["grid"]=deye.sample.grid; d["soc"]=deye.sample.soc;
    d["bridgeSeconds"]=solar.bridging ? (300000U-min(uint32_t(300000),uint32_t(millis()-solar.deficitSince)))/1000 : 0;
    d["probing"]=solar.probing;
    d["host"]=deye.host; d["serial"]=String(deye.serial); d["limit"]=limitA;
    d["transport"]=deye.useRS485?"rs485":"wifi";
    d["baud"]=deye.baud; d["slave"]=deye.slave; d["parity"]=deye.parity;
    d["deyeError"]=deye.error;
    d["includes"]=loadIncludesEV; d["meter"]=meterConfirmed; d["pv3"]=deye.thirdMppt;
    d["regGrid"]=deye.registers.grid; d["regLoad"]=deye.registers.load; d["regSoc"]=deye.registers.soc;
    d["regPv1"]=deye.registers.pv1; d["regPv2"]=deye.registers.pv2; d["regPv3"]=deye.registers.pv3; d["regBat"]=deye.registers.battery;
    d["pv1Scale"]=deye.pv1Scale; d["pv2Scale"]=deye.pv2Scale; d["pv3Scale"]=deye.pv3Scale;
    d["loadScale"]=deye.loadScale; d["gridScale"]=deye.gridScale; d["batScale"]=deye.batteryScale; d["socScale"]=deye.socScale;
    d["socGuard"]=batterySocEnabled; d["socStop"]=batterySocStop; d["socResume"]=batterySocResume; d["socBlocked"]=batterySocGuard.blocked;
    d["socGuardApi"]=true;
    d["ledOn"]=lilygoLedEnabled; d["ledBlink"]=lilygoLedFaultBlink; d["ledState"]=lilygoLedName(lilygoLedLastState);
    d["ledStart"]=colorText(lilygoLedColors[LILYGO_LED_STARTUP]); d["ledReady"]=colorText(lilygoLedColors[LILYGO_LED_READY]);
    d["ledCharge"]=colorText(lilygoLedColors[LILYGO_LED_CHARGING]); d["ledFault"]=colorText(lilygoLedColors[LILYGO_LED_FAULT]);
    d["tcpPort"]=SERIAL2_TCP_PORT; d["lastCommand"]=str_mem_commande; jsonSend(d);
  });
  server.on("/api/mode",HTTP_POST,[](){
    if(!webAuth(true,false)) return;
    String mode=server.arg("mode");
    long amps=0;
    if(mode=="manual" && (!integerValue(server.arg("amps"),amps) || amps<6 || amps>63)) {
      server.send(400,"text/plain; charset=utf-8","Intensité manuelle invalide (6 à 63 A)."); return;
    }
    String error=applyControlMode(mode,int(amps));
    if(!error.isEmpty()) { server.send(409,"text/plain; charset=utf-8",error); return; }
    server.send(200,"text/plain; charset=utf-8","Mode appliqué. Vérifier l'état réel du véhicule.");
  });
  server.on("/api/soc-guard",HTTP_POST,[](){
    if(!webAuth(true,false)) return;
    long socStop,socResume;
    if(!integerValue(server.arg("socStop"),socStop) || !integerValue(server.arg("socResume"),socResume) ||
       socStop<0 || socStop>95 || socResume<5 || socResume>100 || socResume<socStop+5) {
      server.send(400,"text/plain; charset=utf-8","Seuils SOC invalides : reprise au moins 5 % au-dessus de l'arrêt"); return;
    }
    batterySocEnabled=server.arg("socGuard")=="1";
    batterySocStop=int(socStop); batterySocResume=int(socResume);
    batterySocGuard.reset();
    saveControlConfig();
    server.send(200,"text/plain; charset=utf-8","Protection SOC enregistrée sans modifier le mode de charge.");
  });
  server.on("/api/config",HTTP_POST,[](){
    if(!webAuth(true)) return;
    if(controlMode!="stop" && controlMode!="legacy") { server.send(409,"text/plain","Arrêter la charge avant configuration"); return; }
    String transport=server.hasArg("transport")?server.arg("transport"):(deye.useRS485?"rs485":"wifi");
    if(transport!="wifi" && transport!="rs485") { server.send(400,"text/plain","Liaison Deye invalide"); return; }
    bool rs=transport=="rs485";
    String host=deye.host, parity=deye.parity;
    uint32_t serial=deye.serial;
    long limit, baud=deye.baud, slave=deye.slave;
    if(!integerValue(server.arg("limit"),limit) || limit<6 || limit>63) { server.send(400,"text/plain","Courant invalide"); return; }
    if(rs) {
      parity=server.arg("parity");
      if(!integerValue(server.arg("baud"),baud) || baud<0 || !DeyeModbus::validBaud(baud) ||
         !integerValue(server.arg("slave"),slave) || slave<1 || slave>247 ||
         (parity!="8N1" && parity!="8E1" && parity!="8O1" && parity!="8N2")) {
        server.send(400,"text/plain","Adresse Modbus (1 à 247), vitesse ou parité invalide"); return;
      }
    } else {
      IPAddress ip; host=server.arg("host"); String sn=server.arg("serial");
      bool digits=sn.length()>0 && sn.length()<=10;
      for(size_t i=0;i<sn.length();i++) if(!isdigit((unsigned char)sn[i])) digits=false;
      unsigned long long value=strtoull(sn.c_str(),nullptr,10);
      if(!ip.fromString(host) || !digits || value==0 || value>UINT32_MAX) {
        server.send(400,"text/plain","IP ou numéro de série LSW invalide"); return;
      }
      serial=uint32_t(value);
    }
    DeyeModbus::RegisterMap map;
    if(!registerValue(server.arg("regGrid"),map.grid) || !registerValue(server.arg("regLoad"),map.load) ||
       !registerValue(server.arg("regSoc"),map.soc) || !registerValue(server.arg("regPv1"),map.pv1) ||
       !registerValue(server.arg("regPv2"),map.pv2) || !registerValue(server.arg("regPv3"),map.pv3) ||
       !registerValue(server.arg("regBat"),map.battery)) {
      server.send(400,"text/plain","Adresse de registre invalide (0 à 65535)"); return;
    }
    bool third=server.arg("pv3")=="1";
    if(!DeyeModbus::validMap(map,third)) {
      server.send(400,"text/plain","Les registres actifs doivent tenir dans un bloc Modbus de 125 registres maximum"); return;
    }
    float pv1Scale,pv2Scale,pv3Scale,loadScale,gridScale,batteryScale,socScale;
    if(!scaleValue(server.arg("pv1Scale"),pv1Scale) || !scaleValue(server.arg("pv2Scale"),pv2Scale) ||
       !scaleValue(server.arg("pv3Scale"),pv3Scale) || !scaleValue(server.arg("loadScale"),loadScale) ||
       !scaleValue(server.arg("gridScale"),gridScale) || !scaleValue(server.arg("batScale"),batteryScale) ||
       !scaleValue(server.arg("socScale"),socScale)) {
      server.send(400,"text/plain","Coefficient invalide (0,001 à 100)"); return;
    }
    long socStop,socResume;
    if(!integerValue(server.arg("socStop"),socStop) || !integerValue(server.arg("socResume"),socResume) ||
       socStop<0 || socStop>95 || socResume<5 || socResume>100 || socResume<socStop+5) {
      server.send(400,"text/plain","Seuils SOC invalides : reprise au moins 5 % au-dessus de l'arrêt"); return;
    }
    deye.cancel(); deye.host=host; deye.serial=serial; limitA=limit;
    deye.useRS485=rs; deye.baud=baud; deye.slave=slave; deye.parity=parity;
    loadIncludesEV=server.arg("includes")=="1"; meterConfirmed=server.arg("meter")=="1";
    deye.thirdMppt=third; deye.registers=map;
    deye.pv1Scale=pv1Scale; deye.pv2Scale=pv2Scale; deye.pv3Scale=pv3Scale;
    deye.loadScale=loadScale; deye.gridScale=gridScale; deye.batteryScale=batteryScale; deye.socScale=socScale;
    batterySocEnabled=server.arg("socGuard")=="1"; batterySocStop=socStop; batterySocResume=socResume;
    deye.configure(); solar.reset(); deyeLoss.reset(); batterySocGuard.reset();
    solarMeasurements.reset(); saveSolarMeasurementFault();
    saveControlConfig(); server.send(200,"text/plain; charset=utf-8","Configuration enregistrée · liaison Deye "+String(rs?"RS485":"Wi-Fi")+" active");
  });
  server.on("/api/led",HTTP_POST,[](){
    if(!webAuth(true)) return;
    uint32_t colors[4];
    if(!colorValue(server.arg("ledStart"),colors[LILYGO_LED_STARTUP]) ||
       !colorValue(server.arg("ledReady"),colors[LILYGO_LED_READY]) ||
       !colorValue(server.arg("ledCharge"),colors[LILYGO_LED_CHARGING]) ||
       !colorValue(server.arg("ledFault"),colors[LILYGO_LED_FAULT])) {
      server.send(400,"text/plain; charset=utf-8","Couleur invalide : format #RRGGBB attendu."); return;
    }
    for(uint8_t i=0;i<4;i++) lilygoLedColors[i]=colors[i];
    lilygoLedEnabled=server.arg("ledOn")=="1"; lilygoLedFaultBlink=server.arg("ledBlink")=="1";
    lilygoLedLastColor=0xffffffff; lilygoLedFaultVisible=true; lilygoLedLastBlinkAt=millis();
    saveLedConfig(); lilygoLedTick();
    server.send(200,"text/plain; charset=utf-8",lilygoLedEnabled?"Configuration LED enregistrée.":"LED désactivée.");
  });
  server.on("/api/prepare",HTTP_POST,[](){
    if(!webAuth(true)) return;
    controlMode="stop"; solar.reset(); deyeLoss.reset(); rememberMode();
    if(!setCurrent(0)) { server.send(502,"text/plain; charset=utf-8",controlMessage); return; }
    String error=prepareWB(server.arg("pin"),true);
    server.send(error.isEmpty()?200:409,"text/plain; charset=utf-8",error.isEmpty()?"Pilotage préparé. Revenir à Pilotage pour démarrer.":error);
  });
  server.on("/api/parameters",HTTP_GET,[](){
    if(!webAuth()) return;
    if(!loadWBParameters()) { server.send(502,"text/plain; charset=utf-8",wbListError); return; }
    DynamicJsonDocument d(24576); JsonArray rows=d.createNestedArray("parameters");
    for(size_t i=0;i<wbCount;i++) {
      auto &p=wbParameters[i]; JsonObject r=rows.createNestedObject();
      r["name"]=p.name; r["value"]=p.value; r["unit"]=p.unit; r["min"]=p.minimum; r["max"]=p.maximum; r["description"]=p.description;
    }
    d["date"]=Read_Evse("date"); jsonSend(d);
  });
  server.on("/api/parameter",HTTP_POST,[](){
    if(!webAuth(true)) return;
    if(controlMode!="stop" && controlMode!="legacy") { server.send(409,"text/plain","Arrêter avant de modifier les paramètres"); return; }
    String name=server.arg("name"), value=server.arg("value"),error;
    if(!identifier(name) || !safeValue(value)) { server.send(400,"text/plain","Valeur invalide"); return; }
    if(!loadWBParameters()) { server.send(502,"text/plain",wbListError); return; }
    WBParameter *p=nullptr; for(size_t i=0;i<wbCount;i++) if(wbParameters[i].name==name) p=&wbParameters[i];
    if(!p) { server.send(400,"text/plain","Paramètre non annoncé par la borne"); return; }
    long v,lo,hi;
    if(integerValue(p->minimum,lo) && integerValue(p->maximum,hi) && integerValue(value,v) && (v<lo || v>hi)) { server.send(400,"text/plain","Valeur hors limites"); return; }
    error=unlockWB(server.arg("pin")); if(error.isEmpty()) error=writeWB(name,value);
    String readback=error.isEmpty()?Read_Evse("get "+name):error;
    server.send(error.isEmpty()?200:502,"text/plain; charset=utf-8",readback);
  });
  server.on("/api/date",HTTP_POST,[](){
    if(!webAuth(true)) return;
    if(controlMode!="stop" && controlMode!="legacy") { server.send(409,"text/plain","Arrêter avant de régler l'heure"); return; }
    String s=server.arg("value"); int y,m,d,h,n; char tail;
    if(sscanf(s.c_str(),"%d-%d-%dT%d:%d%c",&y,&m,&d,&h,&n,&tail)!=5 || y<2012 || y>2038 || m<1 || m>12 || h<0 || h>23 || n<0 || n>59) { server.send(400,"text/plain","Date invalide (2012-2038)"); return; }
    int days[]={31,28,31,30,31,30,31,31,30,31,30,31}; if(y%4==0) days[1]=29;
    if(d<1 || d>days[m-1]) { server.send(400,"text/plain","Jour invalide"); return; }
    String r=Read_Evse("date "+String(y)+" "+String(m)+" "+String(d)+" "+String(h)+" "+String(n));
    server.send(r.indexOf("date set to")>=0?200:502,"text/plain; charset=utf-8",r);
  });
}
