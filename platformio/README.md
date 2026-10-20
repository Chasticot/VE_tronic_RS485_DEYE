# VETRONIC RS485 — migration PlatformIO

Ce projet PlatformIO est une copie des sources Arduino locales sous
`C:\Users\lagie\Documents\Arduino\ve_tronic_rs485` n'ont pas ete modifiees.

Deux environnements sont disponibles :

- `vetronic_rs485_sd` : variante recente avec historique sur carte SD
  (environnement par defaut) ;
- `vetronic_rs485` : variante sans l'historique SD.

La carte detectee dans les anciennes compilations est un ESP32 generique
(`esp32dev`) avec Arduino-ESP32 2.0.17. Les versions existantes d'AutoConnect,
PageBuilder, ArduinoJson et ESP32Time sont embarquees dans `lib/`.

## Utilisation dans VS Code

1. Ouvrir ce dossier, celui qui contient `platformio.ini`.
2. Dans **PlatformIO > Project Tasks**, choisir `vetronic_rs485_sd` ou
   `vetronic_rs485`.
3. Cliquer sur **Build** pour compiler.
4. Brancher la carte, puis cliquer sur **Upload** pour televerser.
5. Ouvrir **Monitor** pour le port serie a 115200 bauds.

La sonde DS18B20 est desactivee dans la configuration SD actuelle. Si elle est
activee plus tard, il faudra ajouter OneWire et DallasTemperature dans `lib/` ou
`lib_deps`.

Les deux environnements ont ete compiles avec succes. Voir `BUILD_RESULTS.md`
pour l'occupation memoire et les adaptations de compatibilite.

## Maintien de la charge solaire à 6 A

En cours de charge, un surplus inférieur à `6 × tension de calcul` ou une
décharge batterie supérieure à 300 W déclenche un maintien à **6 A pendant
5 minutes maximum**. L'appoint peut venir de la batterie ou du réseau : une
importation réseau ne provoque plus de coupure immédiate pendant ce délai.
La réserve de calcul de 200 W ne déclenche pas à elle seule le timer.

Si le surplus redevient suffisant et que la décharge batterie est retombée à
150 W ou moins, le timer est annulé et la régulation normale reprend. Sinon,
l'arrêt est demandé au premier cycle de régulation après les 300 secondes.
Le redémarrage attend ensuite au moins 1 800 W de surplus et une décharge
batterie à 150 W ou moins. Les protections SOC et les défauts de mesures ou
de liaison borne restent prioritaires ; une perte Deye ne prolonge pas un
timer de déficit déjà engagé.

La même règle est appliquée au projet voisin `VETRONIC-ESP32-OTA`.
Les tests C++ de `tests/solar_logic_test.cpp` vérifient notamment l'importation
réseau, la réduction systématique à 6 A, l'échéance, la reprise et le
débordement du compteur de temps.

## Reconnexion Wi-Fi automatique

Après une coupure du routeur, les deux variantes relancent une connexion toutes
les 30 secondes, sans limite de durée. Chaque tentative est asynchrone pour
laisser fonctionner le pilotage RS485, ses temporisations et l'historique SD.
Le point d'accès direct reste actif ; aucun redémarrage de la carte n'est requis.
Le canal Wi-Fi est recherché à nouveau à chaque essai pour accepter un changement
de canal du routeur. Un réseau choisi depuis la page Wi-Fi devient la nouvelle
cible des tentatives. Si le routeur est absent au démarrage, la surveillance
utilise la configuration station, à défaut les identifiants préconfigurés,
puis le premier réseau mémorisé si aucun identifiant n'est préconfiguré.

Les tests `tests/run_wifi_reconnect_test.ps1` simulent une coupure de dix minutes,
le retour du réseau, un changement manuel de réseau, un démarrage sans routeur,
les identifiants de longueur maximale et le débordement de `millis()`.
Le script accepte le chemin du compilateur Zig via `-Zig`.

Avant un Upload, verifier l'environnement et la carte branches. Un Build ne
modifie pas le materiel.
