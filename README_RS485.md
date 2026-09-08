# WB-01 + Deye 12K-SG02LP1-EU sur LILYGO T-CAN485

Cette version `v3.1-rs485-lilygo` ajoute dans **Paramètres → Onduleur Deye → Liaison avec le Deye** le choix entre **Wi-Fi / logger Solarman LSW** et **RS485 / câble direct**. La sélection s'applique dès l'enregistrement et reste mémorisée après redémarrage. Les anciennes configurations restent en Wi-Fi. Les coordonnées LSW sont conservées lors d'un passage en RS485.

Le Wi-Fi de l'ESP32 sert toujours à la page web, à Jeedom et aux mises à jour. La lecture Deye par RS485 ne dépend ni du réseau ni du logger. Sans Wi-Fi au démarrage, après les délais de connexion et de portail, le programme poursuit son fonctionnement avec le point d'accès AutoConnect `VETRONIC_ESP32_OTA`, adresse `172.0.0.1`. Comme dans le projet fourni, un redémarrage après pilotage géré revient en **arrêt**.

## Carte et liaison WB-01

Profil Arduino : **ESP32 Dev Module**, `esp32:esp32:esp32`, flash 4 Mo, PSRAM désactivée, partitions par défaut. Cette carte est un ESP32 classique. Le binaire de cette version utilise le nouveau câblage même en mode Wi-Fi.

| Fonction | Broche LILYGO | Raccordement |
| --- | --- | --- |
| Réception WB-01, UART2 | GPIO32 | Sortie logique RX du MAX3232 (R1OUT) |
| Émission WB-01, UART2 | GPIO33 | Entrée logique TX du MAX3232 (T1IN) |
| Masse logique WB-01 | GND du connecteur GPIO | GND du MAX3232 |
| Alimentation MAX3232 | 3,3 V vérifié | VCC de l'adaptateur compatible 3,3 V |
| Réception Deye, UART1 | GPIO21 | Déjà reliée au transceiver embarqué |
| Émission Deye, UART1 | GPIO22 | Déjà reliée au transceiver embarqué |
| Commandes RS485 | GPIO17 et GPIO19 | Activées par le firmware en RS485 |
| Alimentation transceiver | GPIO16 | Activée par le firmware en RS485 |

La WB-01 conserve sa liaison **RS232 à 115200 bauds, 8N1**, l'adaptateur MAX3232 et le câble croisé existant. Ses signaux RS232 ne se raccordent pas directement aux GPIO ni au bornier RS485. Déplacer les anciens fils **RX15 → RX32** et **TX4 → TX33** : GPIO4 commande la LED WS2812 et GPIO15 appartient au lecteur SD sur cette carte. Ne pas déduire la tension d'une broche simplement marquée « VDD » de la photo.

La commande du MAX13487 suit l'exemple constructeur LILYGO : GPIO16/17/19 à l'état haut, direction automatique. Les noms EN/SE sur l'image diffèrent de ceux de l'exemple ; il ne faut pas ajouter une commande DE/RE de module MAX485 générique.

## Câble Deye

Utiliser le **port RJ45 RS485 de supervision** du SUN-12K-SG02LP1-EU. Le port BMS est destiné à la batterie, le port Meter au compteur et les ports Parallel à la mise en parallèle.

| Bornier RS485 de la LILYGO | RJ45 du port RS485 Deye |
| --- | --- |
| RS_A | Broche 2 : RS485 A |
| RS_B | Broche 1 : RS485 B |

Utiliser une paire torsadée et repérer les numéros de contacts du connecteur RJ45, sans se fier à une vue miroir. Ne pas ajouter d'autres liaisons RJ45 par supposition. L'annexe I du manuel Deye de mai 2026 donne les broches 3 à 6 non connectées pour ce port ; la documentation du câble SolarAssistant décrit une autre variante avec GND en broche 3. **Ne raccorder le GND RS485 à la broche 3 que si la documentation de votre révision matérielle la confirme.** Le brochage A2/B1 est commun aux deux sources. Vérifier le port et la révision avant raccordement, hors tension selon la procédure constructeur.

## Configuration et premier essai

1. Installer le firmware sur la LILYGO par USB lors de la première utilisation, puis ouvrir `/parametres` (identifiants web définis dans `config.h`).
2. Choisir **RS485**. Valeurs initiales proposées : **9600 bauds, 8N1, adresse Modbus 1**. Régler l'adresse selon le « Modbus SN » de votre Deye ; vitesse et format doivent correspondre à son interface. L'IP et le numéro LSW ne sont pas demandés dans ce mode.
3. Enregistrer en mode Arrêt ou Borne/Jeedom. Le changement invalide les anciennes mesures et attend une nouvelle réponse. Aucune bascule automatique vers l'autre liaison n'est effectuée.
4. Vérifier le diagnostic de `/pilotage` : liaison choisie, paramètres série et erreur éventuelle. Comparer PV, consommation, réseau, batterie et SOC à l'écran Deye avant d'autoriser le solaire. Le bloc **169–195**, le troisième MPPT et les facteurs 1/10 sont repris du projet existant, sans prétendre déduire une nouvelle cartographie du seul nom du modèle.
5. Contrôler la lecture de la WB-01 et les commandes marche/arrêt. Tester ensuite une perte RS485 et le retour des mesures. La temporisation Deye existante de cinq minutes et la priorité de la temporisation batterie sont conservées.

Pour revenir au Wi-Fi, choisir **Wi-Fi · logger Solarman LSW**, vérifier l'IP et le numéro de série du **logger**, puis enregistrer. La connexion utilise TCP 8899 et l'esclave 1, comme auparavant.

## Vérification logicielle

La LED WS2812B intégrée à la LILYGO (GPIO4) affiche une couleur fixe : **bleu** pendant les 20 premières secondes ou l'attente de configuration Deye, **vert** lorsque les communications Deye et WB-01 sont valides, **violet** quand le véhicule est en charge, et **rouge** si une de ces communications expire après la phase de démarrage. Les délais de validité sont 15 s pour le Deye et 10 s pour la WB-01. La LED utilise le pilote intégré au cœur ESP32 ; aucune bibliothèque supplémentaire n'est requise.

Lecture Modbus FC03 uniquement, un bloc toutes les 2,5 s, réception non bloquante, délai de réponse de 1,8 s. Contrôle adresse, fonction, longueur, CRC et cohérence des mesures. Les réponses tardives hors requête sont écartées ; les trames fragmentées, un écho local et les données parasites sont gérés dans un tampon borné. La validité expire également après 15 s sans nouvelle mesure.

Exécuter `tests/run.ps1` et `node tests/web_access_test.js`. Les assertions C++14 évaluent les algorithmes de production à la compilation : régulation, protocole WB-01, CRC et décodage Deye, erreurs et reprise après bruit. Le test JavaScript vérifie les pages, les droits d'accès attendus et la restauration/bascule des champs Wi-Fi/RS485. Ces tests et une compilation ne remplacent pas un essai sur le matériel réel.

Le projet reçu applique déjà une référence de calcul de 230 V quand la tension WB-01 est hors plage. Ce comportement est conservé ; le test hérité qui attendait encore un rejet a été actualisé pour correspondre au code fourni.

## Sources matérielles

- [Broches officielles LILYGO](https://github.com/Xinyuan-LilyGO/T-CAN485/blob/arduino-esp32-libs_v3.0.1/lib/Mylibrary/pin_config.h) et [initialisation RS485 du constructeur](https://github.com/Xinyuan-LilyGO/T-CAN485/blob/arduino-esp32-libs_v3.0.1/examples/RS485_WS2812B/RS485_WS2812B.ino).
- [Manuel Deye SUN-(5–12)K-SG02LP1-EU, mai 2026](https://www.deyeinverter.com/deyeinverter/2026/05/19/BManualSUN-5-12K-SG02LP1-EU-AM320260519en.pdf), présentation des ports et annexe I, page imprimée 48.
- [Câble de supervision SolarAssistant pour SG02LP1](https://solar-assistant.io/help/inverters/deye/SG02LP1/rs485?locale=en), brochage et variante GND.
