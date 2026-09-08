#pragma once
#include <ArduinoJson.h>
#include <base64.h>
#include "solar_logic.h"
#include "deye_solarman.h"
#include "lilygo_led.h"
#include "pilotage_page.h"
#include "wb_protocol.h"

DeyeReader deye;
SolarLogic solar;
DeyeLossTimeout deyeLoss;
String controlMode="legacy", controlMessage="Pilotage historique", csrfToken;
int limitA=32, targetA=-1, nativeLimitA=6;
bool loadIncludesEV=true, meterConfirmed=false;
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
void lilygoLedWrite(LilygoLedState state) {
  // Intensité volontairement modérée pour une LED visible sans éblouir.
  uint8_t red=0,green=0,blue=0;
  switch(state) {
    case LILYGO_LED_STARTUP:  blue=48; break;             // Bleu : démarrage / attente
    case LILYGO_LED_READY:    green=48; break;            // Vert : Deye + WB-01 joignables
    case LILYGO_LED_CHARGING: red=40; blue=48; break;     // Violet : véhicule en charge
    case LILYGO_LED_FAULT:    red=56; break;              // Rouge : défaut de communication
  }
  neopixelWrite(LILYGO_LED_PIN,red,green,blue);
}
void lilygoLedBegin() {
  lilygoLedStartedAt=millis();
  lilygoLedLastState=LILYGO_LED_STARTUP;
  lilygoLedWrite(lilygoLedLastState);
}
void lilygoLedTick() {
  LilygoLedState next=lilygoLedSelect(millis(),lilygoLedStartedAt,
      deye.configured(),deye.sample.valid,deye.sample.at,
      wbSampleValid,wbSampleAt,evse_state.code_status==2);
  if(next!=lilygoLedLastState) {
    lilygoLedLastState=next;
    lilygoLedWrite(next);
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
  lastRequestedA=amps;
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
  p.putBool("includes",loadIncludesEV); p.putBool("meter",meterConfirmed);
  p.putBool("pv3",deye.thirdMppt); p.putFloat("loadScale",deye.loadScale); p.putFloat("gridScale",deye.gridScale);
  p.end();
}
void rememberMode() {
  Preferences p; p.begin("solar-web",false); p.putBool("managed",controlMode!="legacy"); p.end();
}

// Point d'entrée unique pour les boutons web et les commandes Jeedom du port 9200.
// Garder les vérifications ici évite qu'une commande distante ne contourne les
// protections appliquées depuis la page Pilotage.
String applyControlMode(String mode) {
  mode.trim();
  mode.toLowerCase();
  if(mode!="stop" && mode!="manual" && mode!="solar" && mode!="legacy") return "Mode invalide";
  if(mode=="solar" && (!meterConfirmed || !deye.configured()))
    return "Configurer Deye et confirmer les mesures avant activation";

  controlMode="stop"; solar.reset(); deyeLoss.reset(); rememberMode();
  if(!setCurrent(0)) return controlMessage;
  if(mode=="manual" || mode=="solar") {
    String e=prepareWB("");
    if(!e.isEmpty()) return e;
  }
  if(mode=="manual" && !setCurrent(min(limitA,nativeLimitA))) return controlMessage;
  if(mode=="legacy" && !setCurrent(-1)) return controlMessage;
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
  else deyeLoss.reset();

  if((uint32_t)(millis()-lastControl)<5000 || step_tcp!=ATTENTE_REQUETE) return;
  lastControl=millis();

  // Lecture de la borne (avec double tentative)
  refreshWB();

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
  int evW = wbFresh && evse_state.code_status==2 ? int(evse_state.courant)*calculationVolts/1000 : 0;
  int house = deye.sample.load - (loadIncludesEV?evW:0);
  bool fresh = wbFresh && deyeFresh && meterConfirmed && house>=0;
  int surplus = deye.sample.pv - house;
  int amps;

  if(!deyeFresh) {
    amps = deyeLoss.current(millis(), wbFresh && meterConfirmed,
                            solar.running, targetA,
                            solar.bridging, solar.deficitSince);
    if(amps==0) {
      solar.decide(millis(), false, 0, 0, 0, calculationVolts, min(limitA,nativeLimitA));
    }
  } else {
    amps = solar.decide(millis(), fresh, surplus, deye.sample.battery,
                        deye.sample.grid, calculationVolts, min(limitA,nativeLimitA));
  }

  if(!setCurrent(amps)) {
    solar.reset();
    setCurrent(0);
    controlMessage="Défaut liaison borne : arrêt demandé";
  } else if(!deyeFresh) {
    controlMessage = amps>0 ? "Connexion Deye perdue : dernière consigne maintenue, arrêt dans "+String((deyeLoss.remaining(millis())+999)/1000)+" s"
                            : "Deye indisponible : charge arrêtée, attente du retour des mesures";
  } else {
    if(!wbFresh) controlMessage="Arrêt solaire : "+wbReadError;
    else if(!meterConfirmed) controlMessage="Arrêt solaire : validation des mesures non cochée dans Paramètres";
    else if(house<0) controlMessage="Arrêt solaire : consommation maison calculée négative ("+String(house)+" W), vérifier les mesures et l'inclusion du VE";
    else if(solar.probing) controlMessage="Test de déblocage de la production solaire (courant minimal, 2 min)";
    else if(amps==0) controlMessage="Attente solaire : surplus "+String(surplus)+" W / seuil 1800 W"+(solar.recoveryRequired?" ; attente de fin de décharge batterie":"");
    else if(solar.bridging) controlMessage="Appoint batterie (maximum 5 minutes)";
    else controlMessage="Charge solaire dynamique";
  }
}

void beginControl() {
  Preferences p; p.begin("solar-web",true);
  deye.host=p.getString("host",""); deye.serial=p.getUInt("serial",0);
  // Anciennes configurations : Wi-Fi conservé si la nouvelle clé est absente.
  deye.useRS485=p.getBool("rs485",false); deye.baud=p.getUInt("baud",9600);
  deye.slave=p.getUChar("slave",1); deye.parity=p.getString("parity","8N1");
  limitA=constrain(p.getInt("limit",32),6,63);
  loadIncludesEV=p.getBool("includes",true); meterConfirmed=p.getBool("meter",false);
  deye.thirdMppt=p.getBool("pv3",true); deye.loadScale=p.getFloat("loadScale",10); deye.gridScale=p.getFloat("gridScale",10);
  if(p.getBool("managed",false)) controlMode="stop";
  p.end();
  lilygoLedStartedAt=millis();
  deye.configure();
  if(controlMode=="stop") setCurrent(0);
  csrfToken=String(esp_random(),HEX)+String(esp_random(),HEX);
  const char *headers[]={"X-CSRF-Token"}; server.collectHeaders(headers,1);
  server.on("/pilotage",HTTP_GET,[](){ if(webAuth(false,false)) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
  server.on("/parametres",HTTP_GET,[](){ if(webAuth()) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
  server.on("/",HTTP_GET,[](){ if(webAuth(false,false)) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
  // Jeedom expose une clé API : protégée comme /parametres (l'ancienne page ne l'était pas).
  server.on("/jeedom",HTTP_GET,[](){ if(webAuth()) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
  server.on("/wifi",HTTP_GET,[](){ if(webAuth()) server.send_P(200,"text/html; charset=utf-8",PILOTAGE_HTML); });
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
    JsonArray saved=d.createNestedArray("saved");
    AutoConnectCredential credentials;
    station_config_t credential;
    uint8_t entries=min((uint8_t)10,credentials.entries());
    for(uint8_t i=0;i<entries;i++) {
      memset(&credential,0,sizeof(credential));
      if(credentials.load(i,&credential)) saved.add((const char*)credential.ssid);
    }
    JsonArray networks=d.createNestedArray("networks");
    int found=WiFi.scanNetworks();
    if(found>0) for(int i=0;i<found && i<12;i++) {
      String ssid=WiFi.SSID(i);
      if(ssid.isEmpty()) continue;
      JsonObject network=networks.createNestedObject();
      int signal=WiFi.RSSI(i);
      network["ssid"]=ssid; network["rssi"]=signal; network["percent"]=wifiSignalPercent(signal);
      network["secured"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    jsonSend(d);
  });
  server.on("/api/wifi",HTTP_POST,[](){
    if(!webAuth(true)) return;
    String action=server.arg("action"),ssid=server.arg("ssid");
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
    d["surplusW"]=deye.sample.pv-deye.sample.load+(loadIncludesEV?int(evse_state.courant)*calculationVolts/1000:0);
    d["state"]=evse_state.code_status; d["volts"]=evse_state.tension; d["amps"]=evse_state.courant/1000.0;
    d["deyeValid"]=deye.sample.valid && millis()-deye.sample.at<=15000; // harmonisé avec deyeFresh (controlTick)
    d["deyeTimeoutSeconds"]=deyeLoss.remaining(millis())/1000;
    d["pv"]=deye.sample.pv; d["load"]=deye.sample.load; d["battery"]=deye.sample.battery; d["grid"]=deye.sample.grid; d["soc"]=deye.sample.soc;
    d["bridgeSeconds"]=solar.bridging ? (300000U-min(uint32_t(300000),uint32_t(millis()-solar.deficitSince)))/1000 : 0;
    d["probing"]=solar.probing;
    d["host"]=deye.host; d["serial"]=String(deye.serial); d["limit"]=limitA;
    d["transport"]=deye.useRS485?"rs485":"wifi";
    d["baud"]=deye.baud; d["slave"]=deye.slave; d["parity"]=deye.parity;
    d["deyeError"]=deye.error;
    d["includes"]=loadIncludesEV; d["meter"]=meterConfirmed; d["pv3"]=deye.thirdMppt;
    d["loadScale"]=deye.loadScale; d["gridScale"]=deye.gridScale;
    d["tcpPort"]=SERIAL2_TCP_PORT; d["lastCommand"]=str_mem_commande; jsonSend(d);
  });
  server.on("/api/mode",HTTP_POST,[](){
    if(!webAuth(true,false)) return;
    String mode=server.arg("mode");
    String error=applyControlMode(mode);
    if(!error.isEmpty()) { server.send(409,"text/plain; charset=utf-8",error); return; }
    server.send(200,"text/plain; charset=utf-8","Mode appliqué. Vérifier l'état réel du véhicule.");
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
    String ls=server.arg("loadScale"),gs=server.arg("gridScale");
    if((ls!="1" && ls!="10") || (gs!="1" && gs!="10")) { server.send(400,"text/plain","Facteur invalide"); return; }
    deye.cancel(); deye.host=host; deye.serial=serial; limitA=limit;
    deye.useRS485=rs; deye.baud=baud; deye.slave=slave; deye.parity=parity;
    loadIncludesEV=server.arg("includes")=="1"; meterConfirmed=server.arg("meter")=="1";
    deye.thirdMppt=server.arg("pv3")=="1"; deye.loadScale=ls.toInt(); deye.gridScale=gs.toInt();
    deye.configure(); solar.reset(); deyeLoss.reset();
    saveControlConfig(); server.send(200,"text/plain; charset=utf-8","Configuration enregistrée · liaison Deye "+String(rs?"RS485":"Wi-Fi")+" active");
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
