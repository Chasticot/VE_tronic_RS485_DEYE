#include "../VETRONIC_ESP32_OTA/lilygo_led.h"

constexpr bool colors() {
  // Bleu au démarrage ou tant que le Deye n'est pas configuré.
  if(lilygoLedSelect(19999,0,true,false,0,false,0,false)!=LILYGO_LED_STARTUP) return false;
  if(lilygoLedSelect(90000,0,false,false,0,false,0,false)!=LILYGO_LED_STARTUP) return false;
  // Rouge si l'une des communications se perd après la phase d'attente.
  if(lilygoLedSelect(21000,0,true,false,0,true,21000,false)!=LILYGO_LED_FAULT) return false;
  if(lilygoLedSelect(21000,0,true,true,21000,false,0,false)!=LILYGO_LED_FAULT) return false;
  if(lilygoLedSelect(36001,0,true,true,21000,true,21000,false)!=LILYGO_LED_FAULT) return false;
  if(lilygoLedSelect(31001,0,true,true,31001,true,21000,false)!=LILYGO_LED_FAULT) return false;
  // Vert si les deux liaisons sont fraîches ; violet durant une charge réelle.
  if(lilygoLedSelect(25000,0,true,true,25000,true,25000,false)!=LILYGO_LED_READY) return false;
  if(lilygoLedSelect(25000,0,true,true,25000,true,25000,true)!=LILYGO_LED_CHARGING) return false;
  // Les calculs restent corrects lorsque millis() déborde.
  return lilygoLedSelect(1000,0xffff0000U,true,true,1000,true,1000,true)==LILYGO_LED_CHARGING;
}
static_assert(colors(),"Etats LED LILYGO : bleu, vert, violet et rouge");
