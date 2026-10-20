# VE_tronic_RS485_DEYE

Pilotage solaire d'une borne **VE-Tronic WB-01** avec un onduleur **Deye SUN-12K-SG02LP1-EU**, sur une **LILYGO T-CAN485 / ESP32**.

La page web permet de choisir la connexion au Deye : **Wi-Fi via un logger Solarman LSW** ou **RS485 direct**. Le choix et les paramètres sont mémorisés. Le Wi-Fi reste disponible pour la configuration web et Jeedom. Un point d'accès direct permanent permet aussi de joindre la carte à `http://192.168.4.1`.

## Version actuelle — 20 octobre 2026

Le projet [PlatformIO](platformio/README.md) contient la version actuelle, avec
les variantes **sans SD** et **avec historique micro-SD**. La variante SD est
sélectionnée par défaut. Ouvrir le dossier `platformio/` dans VS Code avec
PlatformIO, puis lancer **Build** et **Upload** pour l'environnement souhaité.
Les bibliothèques utilisées pour les compilations sont incluses dans `platformio/lib/`.

Cette version ajoute une reconnexion Wi-Fi toutes les **30 secondes**, sans
limite de durée après une coupure du routeur. Les tentatives ne bloquent pas
le pilotage RS485 et le point d'accès direct reste actif. Le choix d'un autre
réseau depuis la page Wi-Fi devient la nouvelle cible de reconnexion.

Les deux variantes ont été compilées et les tests simulant notamment une
coupure de dix minutes passent. Les détails et limites des vérifications
figurent dans [les résultats de compilation](platformio/BUILD_RESULTS.md).
Les essais sur la carte après une coupure réelle du routeur restent à faire.

Les dossiers Arduino décrits ci-dessous correspondent à la version antérieure.
Pour la correction de reconnexion et la variante SD, utiliser `platformio/`.

## Démarrage Arduino (version antérieure)

1. Lire la [notice RS485 et câblage LILYGO](README_RS485.md).
2. Ouvrir `VETRONIC_RS485/VETRONIC_RS485.ino` dans Arduino IDE.
3. Choisir **ESP32 Dev Module**, cœur Espressif **2.0.17**, flash 4 Mo, partitions par défaut, PSRAM désactivée. Installer AutoConnect, PageBuilder, ArduinoJson 6 et ESP32Time.
4. Facultatif : copier `secrets.example.h` vers `secrets.h` dans le dossier du sketch et renseigner le Wi-Fi et, si souhaité, le mot de passe du point d'accès direct. Ce fichier reste local. Le point d'accès `VE-Tronic-Direct` reste disponible à `192.168.4.1`, même avec une connexion à la box.
5. Personnaliser les identifiants web `WEB_USER` / `WEB_PASSWORD` dans `config.h`, compiler et téléverser par USB sur la carte neuve.
6. Ouvrir `/parametres`, configurer la liaison Deye et comparer les mesures à l'onduleur avant d'activer le pilotage solaire.

**Câblage WB-01 : RX32 / TX33, via MAX3232, 115200 bauds.** Le transceiver RS485 embarqué est réservé au Deye. Voir la notice avant raccordement.

## Contenu

- [Notice LILYGO et RS485](README_RS485.md)
- [Fonctionnement du pilotage solaire et WB-01](README_SOLAIRE.md)
- [Instructions firmware et OTA](firmware/LIRE_AVANT_OTA.md)
- `VETRONIC_RS485/` : sketch et interface web
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
