#pragma once

#include <stddef.h>
#include <stdint.h>

// Parse les valeurs de tension plausibles presentes dans la reponse
// "info_supply". Les firmwares WB-01 n'utilisent pas tous le meme libelle ;
// on conserve donc le minimum et le maximum des nombres compris entre 80 V
// et 300 V, sans dependre du texte traduit par la borne.
inline bool historyParseSupply(const char *text, int &minimum, int &maximum) {
  if (!text) return false;
  bool found=false;
  minimum=1000;
  maximum=0;
  for (const char *p=text; *p;) {
    if (*p<'0' || *p>'9') { ++p; continue; }
    unsigned value=0;
    while (*p>='0' && *p<='9') {
      if (value<100000U) value=value*10U+unsigned(*p-'0');
      ++p;
    }
    if (value>=80U && value<=300U) {
      if (int(value)<minimum) minimum=int(value);
      if (int(value)>maximum) maximum=int(value);
      found=true;
    }
  }
  if (!found) minimum=maximum=0;
  return found;
}

constexpr const char *historyStopKind(unsigned previousState, unsigned state,
                                      int targetA, uint32_t now,
                                      uint32_t lastZeroRequestAt) {
  return previousState!=2U || state==2U ? nullptr :
         (targetA==0 || uint32_t(now-lastZeroRequestAt)<=15000U ?
          "ARRET_COMMANDE_ESP32" : "ARRET_NON_COMMANDE");
}
