# Resultats de compilation PlatformIO

## Maintien à 6 A — 11 octobre 2026

La temporisation solaire autorise désormais l'appoint réseau ou batterie
pendant 300 secondes, avec une consigne systématiquement réduite à 6 A.
Les versions portent le suffixe `-6a-5min`.

| Environnement | Etat | RAM | Flash |
|---|---:|---:|---:|
| `vetronic_rs485` | SUCCESS | 90 588 / 327 680 (27,6 %) | 1 212 269 / 1 310 720 (92,5 %) |
| `vetronic_rs485_sd` | SUCCESS | 90 724 / 327 680 (27,7 %) | 1 246 905 / 1 310 720 (95,1 %) |
| `vetronic_esp32_ota` (projet voisin, ESP32-S3) | SUCCESS | 88 400 / 327 680 (27,0 %) | 1 158 493 / 1 966 080 (58,9 %) |

Les dix assertions de `tests/solar_logic_test.cpp` passent sur les trois
en-têtes de production. Les suites existantes de logique solaire, de
cohérence des mesures et de protection SOC passent également sur chaque
variante ; l'ancien test d'arrêt immédiat sur import réseau a été adapté
en mémoire pour attendre 6 A. Les fichiers Arduino n'ont pas été modifiés.

Vérifications : import réseau au-delà de 200 W, déficit batterie avec
surplus calculé élevé, échéance exacte à 300 secondes, absence de
prolongation lors d'un passage batterie → réseau ou d'une perte Deye,
reprise au retour du surplus, hystérésis et débordement de `millis()`.

Les trois firmwares ont été générés dans leurs dossiers `.pio/build/`.
Aucun téléversement ni essai avec une voiture n'a été effectué.
Les projets PlatformIO restent issus de la migration de septembre : cette
modification ne rapatrie pas les autres correctifs Arduino du 10 octobre.

## Compilation initiale — 28 septembre 2026

Verification effectuee le 28 septembre 2026 avec PlatformIO Core 6.2.0,
Espressif32 6.10.0, Arduino-ESP32 2.0.17 et la carte `esp32dev`.

| Environnement | Etat | RAM | Flash |
|---|---:|---:|---:|
| `vetronic_rs485_sd` | SUCCESS | 90 724 / 327 680 (27,7 %) | 1 246 749 / 1 310 720 (95,1 %) |
| `vetronic_rs485` | SUCCESS | 90 588 / 327 680 (27,6 %) | 1 212 097 / 1 310 720 (92,5 %) |

La marge flash est faible, surtout pour la variante SD. Ne pas ajouter de grosse
bibliotheque sans verifier le pourcentage apres Build.

Un avertissement herite de la source Arduino reste present dans `fonctions.h`
pour la chaine `<!DOCTYPE \\ HTML>`. Il n'empeche ni la compilation ni l'edition
des binaires et n'a pas ete modifie afin de respecter la source metier.

## Verification du 20 octobre 2026 : reconnexion Wi-Fi

Les deux environnements ont ete recompiles avec succes apres ajout de la
surveillance Wi-Fi commune dans `include/wifi_reconnect.h`.

| Environnement | Etat | Taille du firmware.bin |
|---|---|---:|
| `vetronic_rs485_sd` | SUCCESS | 1 254 432 octets |
| `vetronic_rs485` | SUCCESS | 1 219 808 octets |

Pour la variante SD, PlatformIO indique 90 828 octets de RAM (27,7 %) et
1 247 849 octets de flash (95,2 %).

Les tests de `tests/run_wifi_reconnect_test.ps1` ont tous reussi : coupure
simulee de dix minutes avec relances toutes les 30 secondes, retour du reseau,
changement manuel d'identifiants, demarrage sans routeur avec identifiants
preconfigures ou memorises, absence d'identifiants, longueurs maximales et
debordement du compteur de temps.

Les tests utilisent un pilote Wi-Fi simule. Aucun televersement ni essai de
coupure du routeur sur la carte physique n'a ete effectue.

## Adaptations PlatformIO effectuees

- fichiers `.ino` copies sous le nom `main.cpp` ;
- ajout explicite de `SPIFFS.h` dans les deux copies de `main.cpp` pour le
  detecteur de dependances PlatformIO ;
- declaration locale `depends=SPIFFS` pour l'ancienne bibliotheque PageBuilder ;
- script `scripts/framework_deps.py` pour exposer proprement les en-tetes FS et
  SPIFFS a PageBuilder, sans chemin absolu propre a ce PC.

Le dossier `.pio/` est regenerable et est supprime de la livraison apres les
tests pour garder le projet leger.
