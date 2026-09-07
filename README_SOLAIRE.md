# Pilotage web WB-01 + Deye Wi-Fi / RS485

**Version LILYGO T-CAN485 : consulter d'abord [README_RS485.md](README_RS485.md) pour le choix de liaison, le nouveau câblage WB-01 RX32/TX33 et le raccordement Deye 12K-SG02LP1-EU.**

Le sketch est `VETRONIC_ESP32_OTA/VETRONIC_ESP32_OTA.ino`. Il conserve AutoConnect comme secours de connexion, Jeedom, la console série et les réponses XML sur le port 9200. Les pages **Pilotage**, **Paramètres**, **Jeedom** et **Réseau & mise à jour** utilisent toutes la même interface VE‑Tronic.

## Première utilisation

1. Modifier `WEB_USER` et `WEB_PASSWORD` dans `config.h` avant compilation. Valeurs actuelles : `admin` / `vetronic`. L'authentification HTTP Basic affiche la fenêtre native du navigateur, sans certificat. Elle protège les pages **/parametres**, **/jeedom** et **/wifi**, ainsi que leurs API de modification. Le pilotage **/pilotage**, les mesures, marche/arrêt et les lectures/commandes de charge XML restent accessibles sans mot de passe. Les commandes arbitraires de configuration via la console/XML exigent également les identifiants pour éviter de contourner la protection. HTTP ne chiffre pas les identifiants : utiliser le réseau local de confiance, sans redirection de port Internet.
2. Compiler pour votre ESP32 avec les bibliothèques existantes : AutoConnect, PageBuilder, ArduinoJson 6, ESP32Time. Le développement utilise le cœur Espressif **2.0.17** et le profil `esp32:esp32:esp32`.
3. Charger le firmware adapté à la LILYGO par USB ou par OTA. Le nouveau brochage WB-01 est RX 32 / TX 33, UART 115200, avec adaptateur RS232 MAX3232 et câble croisé. Le RS485 embarqué est réservé au Deye, voir la notice dédiée.
4. Au démarrage, le réseau éventuellement défini dans `secrets.h` est essayé ; sinon le portail Wi-Fi de secours permet la configuration. Ouvrir **/wifi** pour choisir un réseau, voir les réseaux mémorisés ou installer un fichier firmware `.bin`. Le bandeau de toutes les pages affiche le SSID actif et la qualité du signal en pourcentage (conversion de -100 à -50 dBm). AutoConnect reste disponible uniquement comme secours si aucun réseau connu n'est accessible. Ouvrir **/parametres** depuis le lien « Modifier les paramètres ». Renseigner l'adresse IP locale et le **numéro de série du logger LSW**, pas celui de l'onduleur. Le port est 8899 et l'esclave Modbus 1.
5. Régler le courant maximal conformément à l'installation. La consigne est aussi plafonnée par les valeurs `max_current…` lues sur la WB-01 ; si plusieurs limites sont annoncées, la plus basse est retenue par prudence. Aucune consigne dynamique n'est envoyée si ces limites ne sont pas disponibles.
6. Comparer les puissances affichées avec le Deye. Les facteurs consommation/réseau sont initialisés à **10**, comme dans `update_dashboard_from_data()` du projet source. Ils sont réglables à 1 ou 10. Activer le troisième MPPT seulement si le registre 188 correspond bien à une entrée PV de votre modèle.
7. Indiquer si la consommation Deye inclut la borne. Dans ce cas, vérifier la **mesure réelle** du courant VE retournée par `$GG*B2` : elle nécessite le tore prévu par la WB-01. Une consigne de courant n'est pas une mesure. Cocher la validation des mesures pour autoriser le solaire. Cette version gère la charge **monophasée** ; elle refuse le démarrage géré si `network_type` indique du triphasé.
8. Dans **/parametres**, utiliser « Préparer le pilotage » si les modes natifs de la borne doivent changer. Revenir sur **/pilotage** pour choisir Solaire dynamique, Lancer immédiatement ou Arrêter sans authentification. Le mot de passe système **WB-01** est nécessaire à la préparation et pour éditer les paramètres protégés. Il n'est pas sauvegardé dans l'ESP32.

En mode RS485, les réglages du logger de l'étape 4 sont remplacés par l'adresse Modbus, la vitesse et la parité décrites dans la notice RS485.

## Régulation

- Lecture du bloc Deye 169–195 toutes les 2,5 secondes. Une lecture porte sur un bloc cohérent : PV1 186, PV2 187, PV3 188, consommation 178, réseau 169, batterie 190 et SOC 184. Les puissances réseau/batterie sont signées ; positif = import/décharge.
- Toutes les 5 secondes, nouvelle lecture WB-01 puis calcul : `maison = consommation Deye - puissance VE mesurée` si la borne est incluse, sinon `maison = consommation Deye`. `surplus = PV - maison`.
- Démarrage si surplus ≥ **1 800 W**. Consigne entière `surplus / tension mesurée`, arrondie vers le bas, entre 6 A et la limite autorisée. À 230 V, 1 800 W donne 7 A.
- En cours de charge, un surplus inférieur à `6 × tension` ou une décharge batterie supérieure à 100 W ouvre une fenêtre de **300 secondes**. Sous le minimum solaire, la consigne descend à 6 A. À expiration, consigne 0 A, avec attente d'un surplus ≥ 1 800 W et de la fin de la décharge batterie avant redémarrage.
- Un retour à un surplus suffisant pour couvrir 6 A, sans décharge batterie, remet la temporisation à zéro. Il s'agit de cinq minutes **par baisse continue**, pas d'un budget quotidien.
- Si le réseau fournit plus de 200 W alors que le surplus est inférieur au minimum de charge, la charge est coupée : pas de bascule volontaire sur le réseau lorsque la batterie ne prend pas le relais. Ces seuils de mesure évitent de réagir au bruit de quelques watts.
- Perte de connexion Deye ou mesures Deye anciennes de plus de 15 s : en mode solaire, conserver la dernière consigne pendant **300 secondes à partir de la détection de perte**, puis demander l’arrêt au prochain cycle. Un compte à rebours apparaît sur la page. Les tentatives de reconnexion ne prolongent pas le délai. Aucune charge ne démarre sur des données périmées. Au retour de mesures valides, le délai est remis à zéro et la régulation reprend ; après un arrêt, les conditions normales de redémarrage solaire s’appliquent. Pendant le maintien, la charge peut consommer sur la batterie ou le réseau puisqu’il n’y a plus de mesure Deye exploitable. Une temporisation d’appoint batterie déjà engagée garde son échéance initiale et peut donc provoquer un arrêt plus tôt.
- Une réponse WB-01 invalide, une tension incohérente ou, avec des mesures Deye fraîches, une consommation maison calculée négative provoquent toujours une demande d’arrêt au prochain cycle. Le bouton Arrêter reste immédiat, sans délai Deye. Les réponses série sont bornées à deux secondes ; un défaut ou une requête web peut donc décaler un cycle. Il ne s’agit pas d’une boucle temps réel stricte.
- Un état Wi-Fi transitoire ne provoque plus de redémarrage automatique de l’ESP32 : AutoConnect gère la reconnexion et la temporisation peut continuer. Après un véritable redémarrage, la règle de retour en mode arrêt reste inchangée.

La connexion Deye est **en lecture seule** : la décharge batterie reste soumise au SOC minimum, aux horaires et aux autorisations déjà configurés dans l'onduleur. Le programme limite la durée pendant laquelle la recharge VE tolère cet appoint ; il ne peut pas interdire à la batterie d'alimenter les autres appareils de la maison. Une panne physique de l'ESP32 ou de la liaison série peut empêcher la transmission d'un arrêt ; les protections matérielles de la borne restent indispensables.

## Paramètres de la borne

La commande `list` fournit les noms exacts, valeurs, unités, bornes et descriptions du firmware installé. La page affiche ces lignes et permet une édition individuelle par `passwd`, `set nom valeur`, puis relecture. Elle ne suppose pas qu'un menu V1.5L existe sur un firmware V1.5C.

Selon firmware : courants primaire/secondaire, mode de charge, deux plages horaires et durées, autorisation d'arrêt, réveil, contacteur clef, délestage, abonnement, calibration du tore, solaire natif et sa plage, courant de base/temporisation, réseau, température maximale, correction horloge, langue et mot de passe système si exposé. La date/heure dispose d'une ligne dédiée via la commande `date`. Les formats horaires sont ceux renvoyés par la borne (par exemple `hh:mm`). Un paramètre non exposé par l'interface série ne peut pas être inventé : il reste accessible sur la borne, et la console historique permet `help`, `info` et les diagnostics.

Le bouton **Préparer le pilotage**, sur la page protégée, règle `charge_mode=0` et `solar_mode=0` si nécessaire. Les boutons publics de démarrage ne modifient pas ces paramètres et renvoient vers la préparation si nécessaire. Ces changements sont persistants dans la WB-01. Le bouton « Rendre la main » libère la consigne avec `$SC -1` ; **il ne restaure pas automatiquement ces deux modes**. Les remettre à leur valeur souhaitée dans le tableau pour retrouver les horaires ou le solaire natif. Le verrouillage par clef n'est pas désactivé.

Arrêter annule la régulation et maintient une consigne nulle. En pilotage web, les commandes arbitraires de l'ancienne console et du port XML sont refusées pour éviter deux régulateurs concurrents ; la lecture XML sans commande reste disponible. Pour reprendre ces commandes, rendre la main. Les lectures et commandes de courant Jeedom ne nécessitent plus d'authentification. Seules les commandes arbitraires de configuration restent protégées (identifiants identiques à la page Paramètres).

La configuration Deye est conservée dans un espace Preferences séparé de Jeedom/Wi-Fi. Après redémarrage d'un ESP32 qui pilotait la charge, le mode revient sur **arrêt**, sans reprise automatique ni remise à zéro de la borne. Une première installation conserve le pilotage historique jusqu'à sélection d'un mode.

## Vérification et références

Les tests `tests/solar_logic_test.cpp` utilisent directement le régulateur de production et évaluent à la compilation C++14 : seuil exact, adaptation/plafond, expiration des cinq minutes, attente avant redémarrage, retour du soleil, défauts de mesure/réseau et débordement de `millis()`.

La validation sur le matériel reste à faire : lecture des paramètres de votre firmware, test marche/arrêt avec votre véhicule, comparaison des puissances réelles, baisse solaire sur plus de cinq minutes et déconnexion du logger. Aucun téléversement ni essai de puissance n'a été effectué par cette modification.

Sources :

- [Documentation constructeur WB-01 V1.5L](https://www.ve-tronic.fr/store/download/wallbox_WB-01_V15L.pdf), sections 4–6 ; copie texte déjà présente dans `tmp/pdfs/WB01.txt`.
- [Projet de base de Vincent Robert](https://github.com/Vince00731/vetronic-esp32-ota), commandes `$SC`, `$GG*B2` et architecture AutoConnect/Jeedom.
- Chaînes du firmware constructeur V1.5C déjà présentes dans `tmp/firmware-strings.txt` : formats de `list`, `passwd`, `set`, `get` et `date`.
- [Implémentation Solarman V5 de référence](https://github.com/jmccrohan/pysolarmanv5/blob/master/pysolarmanv5/pysolarmanv5.py), vérification de l'enveloppe et offset RTU 25.
- Adaptation de votre `DEYE_LVGL_V3_2x2/DEYE_LVGL_UI_2x2_menu_avance_coef_Tempo_VE/deye_solarman.h` et `registres.h`, sans modifier ce projet source.

Test de séparation des pages et des appels API : `node tests/web_access_test.js`.

Les tests couvrent également le maintien de la dernière consigne sur perte Deye, l’expiration exacte à 300 secondes, les échecs répétés, la reconnexion, les défauts WB-01 prioritaires, la temporisation batterie préexistante et le débordement du compteur de temps.

## Diagnostic d'une consigne solaire bloquée à zéro

La production Deye seule ne valide pas les mesures WB-01. « Courant VE — » indique que la lecture `evse_state` / `$GG*B2` n'est pas valide ou a expiré. Le régulateur vérifie également la tension (180–260 V), les courants (0–63000 mA) et l'état (0–2).

La rubrique « Diagnostic de la régulation » du pilotage affiche le mode ESP32, le surplus calculé, la limite WB-01, la dernière demande `$SC` et les réponses brutes de la borne. Une réponse quelconque n'est plus considérée comme un succès : la confirmation `$OK` est requise pour afficher une consigne confirmée. Un format de réponse différent nécessite une adaptation à partir de la réponse réelle, sans remplacer arbitrairement une mesure manquante.

### Historique de la correction du démarrage avec prise véhicule hors tension

**État du code fourni pour cette adaptation :** la référence de 230 V est désormais utilisée pour toute tension WB-01 hors plage 180–260 V, sans invalider son état. Cette règle est conservée dans la version RS485 ; les anciens cas de rejet de tension décrits ci-dessous sont historiques. Le test WB-01 a été actualisé pour vérifier cette règle, ainsi que le rejet des états invalides, réponses incomplètes et courants excessifs.

La tension `$GG*B2` est celle appliquée sur la prise véhicule (voir le README du projet d’origine). Une réponse `EVSE state : 1` et `$OK 100 0 0` est donc acceptée comme une lecture de veille cohérente. À 0 V, uniquement hors charge (états 0/1 et courant ≤ 100 mA), le calcul initial utilise une **référence nominale de 230 V**, explicitement indiquée dans le diagnostic. La mesure brute reste affichée à 0 V et la puissance VE soustraite de la consommation maison vaut 0 W hors charge. Une fois en charge, la tension mesurée doit être entre 180 et 260 V ; 0 V en état 2, une tension intermédiaire anormale, un courant incohérent ou une réponse malformée restent invalides.

Le firmware WB-01 observé renvoie seulement l’écho `$SC n` et le prompt `wallbox$`. Cette réponse permet désormais de poursuivre le pilotage. Elle n’est pas présentée comme une preuve du courant appliqué : une lecture `$GG*B2` contrôle la troisième valeur. Une limite différente reste signalée « à vérifier », sans dépasser ni contourner les protections de la borne.

Régression reproduite dans `tests/wb_protocol_test.cpp` : les réponses exactes fournies par l’utilisateur et un surplus de 2 353 W permettent de calculer 10 A avec une limite configurée de 16 A ; un état en charge à 0 V reste refusé.
