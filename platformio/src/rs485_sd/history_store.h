#pragma once

#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "ff.h"

class HistoryStore {
  SPIClass spi{HSPI};
  String lastErrorText="Carte non initialisée";
  uint32_t lastWriteMs=0;
  bool mounted=false;

  bool ensureDirectory(const char *path) {
    if (SD.exists(path)) return true;
    if (SD.mkdir(path)) return true;
    lastErrorText="Création impossible : "+String(path);
    return false;
  }

  bool removeTree(const String &path, bool removeRoot) {
    File root=SD.open(path);
    if (!root) return !SD.exists(path);
    if (!root.isDirectory()) {
      root.close();
      return SD.remove(path);
    }
    for (File entry=root.openNextFile(); entry; entry=root.openNextFile()) {
      String name=entry.name(); int slash=name.lastIndexOf('/'); if(slash>=0) name=name.substring(slash+1);
      String child=path+"/"+name;
      bool directory=entry.isDirectory();
      entry.close();
      if (directory) {
        if (!removeTree(child,true)) { root.close(); return false; }
      } else if (!SD.remove(child)) { root.close(); return false; }
    }
    root.close();
    return !removeRoot || SD.rmdir(path);
  }

  bool appendLine(const String &path, const String &header, const String &line) {
    if (!mounted) return false;
    bool fresh=!SD.exists(path);
    File file=SD.open(path,FILE_APPEND);
    if (!file) { lastErrorText="Ouverture impossible : "+path; return false; }
    bool ok=true;
    if (fresh && header.length()) ok=file.println(header)>0;
    if (ok) ok=file.println(line)>0;
    file.flush();
    file.close();
    if (!ok) { lastErrorText="Écriture incomplète : "+path; return false; }
    lastWriteMs=millis();
    lastErrorText="";
    return true;
  }

public:
  static const char *sampleHeader() {
    return "timestamp;epoch;uptime_ms;boot_id;state;wb_valid;volts_raw;volts_calc;supply_min_v;supply_max_v;current_a;limit_a;target_a;requested_a;current_confirmed;deye_valid;pv_w;load_w;grid_w;battery_w;soc;ev_w;house_w;surplus_w;mode;soc_blocked;measurement_pending;measurement_blocked;bridge_seconds;probing;temperature_c;temperature_valid;wb_error;deye_error";
  }

  bool begin() {
    pinMode(SD_CS,OUTPUT);
    digitalWrite(SD_CS,HIGH);
    spi.begin(SD_SCLK,SD_MISO,SD_MOSI,SD_CS);
    if (!SD.begin(SD_CS,spi,HISTORY_SD_FREQUENCY,"/sd",8,false)) {
      mounted=false;
      lastErrorText="Carte absente, illisible ou non formatée en FAT";
      return false;
    }
    mounted=true;
    if (!ensureDirectory("/history") || !ensureDirectory("/history/data") ||
        !ensureDirectory("/history/minute") || !ensureDirectory("/history/events")) {
      mounted=false;
      return false;
    }
    lastErrorText="";
    return true;
  }

  bool ready() const { return mounted; }
  const String &error() const { return lastErrorText; }
  uint32_t lastWriteAt() const { return lastWriteMs; }
  uint64_t totalBytes() const { return mounted?SD.totalBytes():0; }
  uint64_t usedBytes() const { return mounted?SD.usedBytes():0; }
  uint64_t cardBytes() const { return mounted?SD.cardSize():0; }

  bool appendSample(const String &date, const String &line, bool minute=false) {
    String directory=minute?"/history/minute/":"/history/data/";
    return appendLine(directory+date+".csv",sampleHeader(),line);
  }

  bool appendEvent(const String &date, const String &line) {
    return appendLine("/history/events/"+date+".log","",line);
  }

  bool clearHistory() {
    if (!mounted) return false;
    if (!removeTree("/history",true)) {
      lastErrorText="Suppression de l'historique impossible";
      return false;
    }
    return ensureDirectory("/history") && ensureDirectory("/history/data") &&
           ensureDirectory("/history/minute") && ensureDirectory("/history/events");
  }

  // Formate le volume FAT déjà monté. Le pointeur FATFS est conservé puis
  // remonté, ce qui évite le défaut de SD.end()/SD.begin() du coeur ESP32 2.0.17.
  bool formatFat() {
    if (!mounted) {
      // Le pilote 2.0.17 sait créer un volume lorsqu'aucun FAT valide n'est
      // présent. Cela permet notamment de récupérer une carte exFAT ou vierge.
      if (!SD.begin(SD_CS,spi,HISTORY_SD_FREQUENCY,"/sd",8,true)) {
        lastErrorText="Carte absente ou formatage initial impossible";
        return false;
      }
      mounted=true;
      if (!ensureDirectory("/history") || !ensureDirectory("/history/data") ||
          !ensureDirectory("/history/minute") || !ensureDirectory("/history/events")) return false;
      File test=SD.open("/history/.write-test",FILE_WRITE);
      if (!test || test.print("ok")!=2) {
        if (test) test.close();
        lastErrorText="Formatage terminé mais test d'écriture impossible";
        return false;
      }
      test.close(); SD.remove("/history/.write-test");
      lastWriteMs=millis(); lastErrorText="";
      return true;
    }
    FATFS *filesystem=nullptr;
    DWORD freeClusters=0;
    char drive[3]={0,':',0};
    bool found=false;
    for (int i=0;i<FF_VOLUMES;i++) {
      drive[0]=char('0'+i);
      if (f_getfree(drive,&freeClusters,&filesystem)==FR_OK && filesystem) { found=true; break; }
    }
    if (!found) { lastErrorText="Volume FAT monté introuvable"; return false; }
    mounted=false;
    if (f_mount(nullptr,drive,0)!=FR_OK) { lastErrorText="Démontage FAT impossible"; return false; }
    BYTE *work=(BYTE*)malloc(FF_MAX_SS);
    if (!work) {
      f_mount(filesystem,drive,1);
      mounted=true;
      lastErrorText="Mémoire insuffisante pour formater";
      return false;
    }
    FRESULT result=f_mkfs(drive,FM_FAT|FM_FAT32,0,work,FF_MAX_SS);
    free(work);
    FRESULT mountResult=f_mount(filesystem,drive,1);
    mounted=result==FR_OK && mountResult==FR_OK;
    if (!mounted) { lastErrorText="Échec du formatage ou du remontage FAT"; return false; }
    if (!ensureDirectory("/history") || !ensureDirectory("/history/data") ||
        !ensureDirectory("/history/minute") || !ensureDirectory("/history/events")) return false;
    File test=SD.open("/history/.write-test",FILE_WRITE);
    if (!test || test.print("ok")!=2) {
      if (test) test.close();
      lastErrorText="Carte formatée mais test d'écriture impossible";
      return false;
    }
    test.close();
    SD.remove("/history/.write-test");
    lastWriteMs=millis();
    lastErrorText="";
    return true;
  }

  bool validDate(const String &date) const {
    if (date.length()!=10 || date[4]!='-' || date[7]!='-') return false;
    for (int i=0;i<10;i++) if (i!=4 && i!=7 && !isDigit(date[i])) return false;
    int year=date.substring(0,4).toInt(),month=date.substring(5,7).toInt(),day=date.substring(8,10).toInt();
    return year>=2020 && year<=2099 && month>=1 && month<=12 && day>=1 && day<=31;
  }

  String filePath(const String &kind,const String &date) const {
    if (!validDate(date)) return "";
    if (kind=="data") return "/history/data/"+date+".csv";
    if (kind=="minute") return "/history/minute/"+date+".csv";
    if (kind=="events") return "/history/events/"+date+".log";
    return "";
  }

  bool removeOlderThan(const String &directory,const String &cutoff,const char *extension) {
    if (!mounted || !validDate(cutoff)) return false;
    File root=SD.open(directory);
    if (!root || !root.isDirectory()) { if(root) root.close(); return false; }
    for (File entry=root.openNextFile();entry;entry=root.openNextFile()) {
      String name=entry.name();
      int slash=name.lastIndexOf('/'); if(slash>=0) name=name.substring(slash+1);
      bool directoryEntry=entry.isDirectory();
      entry.close();
      if (directoryEntry || !name.endsWith(extension)) continue;
      String date=name.substring(0,10);
      if (validDate(date) && date<cutoff) SD.remove(String(directory)+"/"+name);
    }
    root.close();
    return true;
  }
};
