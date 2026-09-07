# VE_tronic_RS485_DEYE

Pilotage solaire d'une borne **VE-Tronic WB-01** avec un onduleur **Deye SUN-12K-SG02LP1-EU**, sur une **LILYGO T-CAN485 / ESP32**.

La page web permet de choisir la connexion au Deye : **Wi-Fi via un logger Solarman LSW** ou **RS485 direct**. Le choix et les paramètres sont mémorisés. Le Wi-Fi reste disponible pour la configuration web et Jeedom.

## Démarrage

1. Lire la [notice RS485 et câblage LILYGO](README_RS485.md).
2. Ouvrir `VETRONIC_ESP32_OTA/VETRONIC_ESP32_OTA.ino` dans Arduino IDE.
3. Choisir **ESP32 Dev Module**, cœur Espressif **2.0.17**, flash 4 Mo, partitions par défaut, PSRAM désactivée. Installer AutoConnect, PageBuilder, ArduinoJson 6 et ESP32Time.
4. Facultatif : copier `secrets.example.h` vers `secrets.h` dans le dossier du sketch et renseigner le Wi-Fi. Ce fichier reste local. Sinon utiliser le portail de configuration Wi-Fi de secours.
5. Personnaliser les identifiants web `WEB_USER` / `WEB_PASSWORD` dans `config.h`, compiler et téléverser par USB sur la carte neuve.
6. Ouvrir `/parametres`, configurer la liaison Deye et comparer les mesures à l'onduleur avant d'activer le pilotage solaire.

**Câblage WB-01 : RX32 / TX33, via MAX3232, 115200 bauds.** Le transceiver RS485 embarqué est réservé au Deye. Voir la notice avant raccordement.

## Contenu

- [Notice LILYGO et RS485](README_RS485.md)
- [Fonctionnement du pilotage solaire et WB-01](README_SOLAIRE.md)
- [Instructions firmware et OTA](firmware/LIRE_AVANT_OTA.md)
- `VETRONIC_ESP32_OTA/` : sketch et interface web
- `tests/` : tests Modbus, WB-01, régulation et interface web

Les binaires locaux précompilés ne sont pas versionnés : ils peuvent embarquer des identifiants Wi-Fi. Recompiler à partir des sources pour produire son propre firmware. Les fichiers temporaires et caches de compilation sont également exclus.

## Tests

Sous Windows avec le cœur Arduino ESP32 installé :

```powershell
./tests/run.ps1
node tests/web_access_test.js
```

Les assertions C++14 évaluent les algorithmes de production à la compilation. Les tests JavaScript vérifient les pages et le choix Wi-Fi/RS485. Une compilation réussie ne remplace pas les essais sur la borne et l'onduleur.

## Origine

Basé sur le [projet vetronic-esp32-ota de Vincent Robert](https://github.com/Vince00731/vetronic-esp32-ota), avec adaptations de pilotage solaire Deye et prise en charge LILYGO RS485. Les mentions d'auteur du code d'origine sont conservées ; les références matérielles figurent dans les notices.
