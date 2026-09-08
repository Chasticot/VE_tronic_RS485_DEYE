## Publication GitHub

Les fichiers .bin décrits dans cette notice restent locaux et sont exclus de Git, car ils peuvent contenir les identifiants Wi-Fi personnels. Pour une nouvelle installation, recompiler le sketch ; le fichier secrets.h facultatif reste lui aussi local. Voir le README principal.

# Firmware LILYGO T-CAN485 : v3.1 RS485

Fichier actuel : **`VETRONIC_LILYGO_v3.1-rs485.bin`**.

- Binaire applicatif pour la **LILYGO T-CAN485 / ESP32 classique**, cœur Arduino 2.0.17, profil `esp32:esp32:esp32`, flash 4 Mo, partitions par défaut et PSRAM désactivée.
- Compilation finale réussie : programme 1 163 057 octets (88 % de la partition) ; variables globales 90 204 octets (27 % de la RAM).
- **Nouveau câblage WB-01 : RX32 / TX33 via MAX3232**, même si le Deye est utilisé en Wi-Fi. L'ancien RX15/TX4 ne convient pas à ce firmware.
- Dans `/parametres`, sélectionner Wi-Fi / LSW ou RS485. Valeurs RS485 initiales : 9600 bauds, 8N1, esclave 1, à faire correspondre au Deye. Le choix est mémorisé.
- La LED RGB intégrée indique bleu (démarrage/attente), vert (communications prêtes), violet (véhicule en charge) ou rouge (défaut de communication).
- Le réseau de démarrage et les identifiants web du projet sont conservés. Identifiants web actuels : `admin` / `vetronic`, personnalisables dans `config.h`.
- Pour une carte neuve, utiliser l'USB et le sketch avec le profil ci-dessus. Ce fichier applicatif seul n'est pas une image fusionnée à écrire à l'adresse 0. L'OTA est utilisable si un firmware compatible et son partitionnement sont déjà installés sur cette LILYGO.
- Ne pas utiliser ce binaire sur un ESP32-S3 ou une ancienne carte restée câblée RX15/TX4.
- Tests C++14 et JavaScript réussis. Aucun téléversement ni essai avec l'onduleur ou la borne réels n'a été effectué.

Voir [la notice RS485](../README_RS485.md) pour le câblage Deye SUN-12K-SG02LP1-EU, les variantes GND, la configuration et les essais.

## Ancien binaire conservé : v3.0 solaire

Les informations ci-dessous décrivent uniquement l'ancien fichier v3.0, conservé dans le dossier. Utiliser le nouveau fichier ci-dessus pour la LILYGO.

`VETRONIC_ESP32_OTA_v3.0-solaire.bin` est le binaire applicatif pour la mise à jour OTA existante (pas un binaire fusionné à écrire à l'adresse 0).

- Profil compilé : `esp32:esp32:esp32`, cœur 2.0.17, partitionnement par défaut.
- Flash application : 1 137 553 octets, 86 % de la partition.
- Variables globales : 89 476 octets, 27 % de la RAM.
- Identifiants inclus dans ce binaire : **admin / vetronic**.
- Personnaliser `WEB_USER` / `WEB_PASSWORD` dans `config.h` et recompiler pour utiliser d'autres identifiants.
- Après installation : `http://IP_ESP32/pilotage` sans mot de passe ; `http://IP_ESP32/parametres` pour les réglages protégés. Le réseau local dépend des valeurs utilisées à la compilation. Configurer IP et numéro de série LSW et vérifier les puissances avant activation solaire.
- Vérifier que le profil et le partitionnement conviennent à votre module avant téléversement. Aucun téléversement ni essai sur borne réelle n'a été réalisé.

La notice complète est dans `README_SOLAIRE.md` à la racine du projet.

Cette version maintient la dernière consigne solaire jusqu’à cinq minutes après la détection d’une perte Deye, puis demande l’arrêt. La temporisation batterie déjà en cours et les défauts de liaison WB-01 restent prioritaires.

Correction WB-01 : acceptation de la prise hors tension avant charge (référence initiale 230 V, mesure réelle ensuite), échange `$SC` avec écho et prompt, confirmation de limite séparée par `$GG*B2`. Tests reproduisant les réponses réelles de la borne validés.
