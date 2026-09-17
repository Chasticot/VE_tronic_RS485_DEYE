/////////////////////////////////////////////////////////////////////////////////////////////////////
// Connexion de la Borne VETRONIC WB-01 par son UART via un ESP32
// Le firmware de la borne doit être 1.5C ou supérieure
//
// Utilise le Port Série Com2 du ESP32 : voir la configuration des pin dans config.h
// Utilise le Port TCP spécifié dan config.h pour le retour au format XML
// Utilise le Port 80 pour la gestion du WiFi et la mise à jour du firmware de l'ESP 32 par OTA
//
// Le retour en XML se fait par simple appel http en ajoutant la commande de la borne à la fin
// Appels de requête = http://IP-WIFI:PORT_TCP/commande-Vetronic
// Si il n'y a pas de commande spécifiée, le module réalise une interrogation générique de la borne
//
// Auteur: Vincent ROBERT avec l'aide de Christian Dingean (cdlog2@hotmail.com)
// vrelectronique@gmail.com
// Version 2.2 du 07/05/2024
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <AutoConnect.h>
#include <AutoConnectCredential.h>
#include <WiFiClient.h>
#include <ESP32Time.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp32-hal-rgb-led.h>

/****************  Active les messages de debug  ******************/
bool debug = false;

// Port série COM2
HardwareSerial Serial_two(2);
HardwareSerial *COM = &Serial_two;

// Déclarations des variables globales
char buffer_tcp[BUFFERSIZE];
uint16_t cpt_buffer_tcp = 0;
char buffer_com[BUFFERSIZE];
uint16_t cpt_buffer_com = 0;
uint8_t step_tcp = ATTENTE_REQUETE;
unsigned long currentTime = 0;
unsigned long previousTime = 0;
unsigned long timer = 0;
unsigned long timer_display = 0;
evse_state_t evse_state;
evse_state_t previous_evse_state;
const char *fw_ver = VERSION;

// Portail AutoConnect
WebServer server;
AutoConnect portal(server);
AutoConnectConfig config;
param_t params;
String str_mem_commande = "help";

// Server TCP pour les requêtes en XML
WiFiServer server_2(SERIAL2_TCP_PORT);
WiFiServer *server_tcp = &server_2;
WiFiClient TCPClient;

// HTTP Client pour envoi de requête
WiFiClient clientHttp;
HTTPClient httpClient;

// RTC
ESP32Time rtc(0);
const char *ntpServer = "fr.pool.ntp.org";
struct tm timeinfo;
int previous_hour = 0;

// Fonctions nécessaires au fonctionnement du programme
#include "fonctions.h"
#include "pilotage.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialisation de l'ESP 32
/////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{

  // Port série de l'ESP sur chip USB
  Serial.begin(115200);
  Serial.setRxBufferSize(BUFFERSIZE);
  delay(500);
  lilygoLedBegin();

  // Port série COM vers la borne
  COM->begin(UART_BAUD2, SERIAL_PARAM2, SERIAL2_RXPIN, SERIAL2_TXPIN);
  COM->setRxBufferSize(BUFFERSIZE);
  COM->write('\r'); // Lance le prompt sur la borne VETRONIC
  delay(100);
  ClearBufferRxCom();
  Serial.println("VETRONIC_ESP32_OTA " + String(VERSION));

  // RTC
  // On configure le seveur NTP sur la timezone de Paris
  configTzTime("CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", ntpServer);

  // Configuration du portail web AutoConnect
  config.boundaryOffset = CREDENTIAL_OFFSET; // Déplace la sauvergarde des SSID Wifi après les paramètres de EEPROM
  // L'OTA est servie par /wifi afin de rester dans l'interface VE-Tronic.
  config.ota = AC_OTA_EXTRA;
  config.auth = AC_AUTH_NONE; // Authentification uniquement sur les paramètres.
  config.otaExtraCaption = fw_ver;
  config.apid = "VETRONIC_ESP32_OTA";    // SSID en mode AP
  config.apip = IPAddress(172, 0, 0, 1); // Adresse IP en mode AP
  config.autoReconnect = true;           // Reconnection automatique
  // Laisser tourner le régulateur RS485 même sans réseau Wi-Fi au démarrage.
  config.portalTimeout = 15000;
  config.retainPortal = true;
  config.menuItems = config.menuItems | AC_MENUITEM_DELETESSID;
  portal.config(config);

  // Déclaration des pages Web : toutes gérées par beginControl() (pilotage.h),
  // sur le même template HTML/CSS/JS que Pilotage et Paramètres.
  beginControl();

  // Démmarage du portail AutoConnect
  if (portal.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD, 15000))
  {
    Serial.println("Web server started, IP:" + WiFi.localIP().toString());
  }
  else
  {
    Serial.println("Wi-Fi indisponible : fonctionnement local et portail de secours actifs.");
  }

  // Récupération des valeurs en mémoire
  getParams(params);

  // Démmarage du server TCP
  Serial.println("Starting TCP server");
  server_tcp->begin(); // start TCP server
  server_tcp->setNoDelay(true);

  // Configuration des timeouts du client HTTP
  httpClient.setTimeout(200);
  httpClient.setConnectTimeout(500);

  // Intitialisation des variables globales
  evse_state.code_status = 13;
  strncpy(evse_state.txt_status, "Démarrage...", sizeof(evse_state_t::txt_status) - 1);
  evse_state.courant = 0;
  evse_state.courant_max = 0;
  evse_state.tension = 0;

  previous_evse_state.code_status = 13;
  strncpy(previous_evse_state.txt_status, "Démarrage...", sizeof(evse_state_t::txt_status) - 1);
  previous_evse_state.courant = 0;
  previous_evse_state.courant_max = 0;
  previous_evse_state.tension = 0;

  // Mise à jour des données Jeedom
  UpdateJeedom(String(params.id_code_status), String(evse_state.code_status));
  UpdateJeedom(String(params.id_txt_status), String(evse_state.txt_status));
  UpdateJeedom(String(params.id_tension), String(evse_state.tension));
  UpdateJeedom(String(params.id_courant), String(evse_state.courant));
  UpdateJeedom(String(params.id_courant_max), String(evse_state.courant_max));

  // Ne pas réinitialiser la WB-01 : cela peut libérer une ancienne consigne d'arrêt.
  if (controlMode == "stop") setCurrent(0);

  // Initialisation des machines à état
  step_tcp = ATTENTE_REQUETE;

  // Initilisation du timer de refresh
  timer = millis();
  timer_display = millis();

  // Mise à jour de l'heure
  if (getLocalTime(&timeinfo, 10))
  {
    if (debug)
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
    rtc.setTimeStruct(timeinfo);
  }
  else
  {
    // Echec d'obtention de l'heure
    if (debug)
      Serial.println(&timeinfo, "Echec d'obtention de l'heure");
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Boucle principale
/////////////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{
  static String str = "";
  static String str_com = "";
  static String str2_com = "";
  static char str_temp[40];
  static uint32_t tcpStarted = 0;

  // AutoConnect gère la reconnexion. Ne pas redémarrer sur un état Wi-Fi
  // transitoire : le régulateur doit conserver sa temporisation Deye de 5 min.
  // Gestion du portail AutoConnect
  portal.handleClient();
  webMaintenanceTick();
  controlTick();
  lilygoLedTick();

  // Mise à jour de l'heure par NTP toutes les heures
  if (previous_hour != rtc.getHour(true))
  {
    previous_hour = rtc.getHour(true);
    if (getLocalTime(&timeinfo, 10))
    {
      if (debug)
      {
        Serial.println(&timeinfo, "Mise à jour de l'heure");
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
      }
      // Mise à jour de l'heure dans la RTC locale
      rtc.setTimeStruct(timeinfo);
    }
    else
    {
      // Echec d'obtention de l'heure
      if (debug)
        Serial.println(&timeinfo, "Echec d'obtention de l'heure");
    }
  }

  // Si la fréquence de raffraichissement est trop faible, on corrige à 2sec mini
  if ((params.freq_update_evse < 2000) && (params.freq_update_evse != 0))
  {
    params.freq_update_evse = 2000;
    // Sauvegarde en mémoire
    putParams(params);
  }

  // Lecture de la borne réguliérement
  if ((millis() > timer + params.freq_update_evse) && (params.freq_update_evse != 0))
  {
    // Si une requête est en cours, on attend le prochain tour de boucle
    if (step_tcp == ATTENTE_REQUETE)
    {
      step_tcp = UPDATE_EVSE;
    }
  }

  // Mise à jour de l'écran de la borne réguliérement (1sec)
  if ((millis() > timer_display + 1000))
  {
    // Si une requête est en cours, on attend le prochain tour de boucle
    if (step_tcp == ATTENTE_REQUETE)
    {
      // Si véhicule en charge
      if (evse_state.code_status == 2)
      {
        sprintf(str_temp, "%dV | %.1f A | %d A", evse_state.tension, (float)((float)evse_state.courant / 1000), evse_state.courant_max / 1000);
        Write_Display_Evse(String(str_temp));
      }
      // Si la borne est connecté mais la borne pas activée
      else if ((evse_state.courant_max < 6000) && (evse_state.code_status == 1))
      {
        Write_Display_Evse(String("Charge indisponible"));
      }
      else
      {
        Write_Display_Evse(rtc.getTime("%d/%m/%Y %H:%M:%S"));
      }
      timer_display = millis();
    }
  }

  // Machine à état des requêtes TCP
  switch (step_tcp)
  {
  /////////////
  case ATTENTE_REQUETE:
    if (server_tcp->hasClient())
    {
      if (!TCPClient || !TCPClient.connected())
      {
        if (TCPClient)
          TCPClient.stop();
        TCPClient = server_tcp->available();
        cpt_buffer_tcp = 0;
        tcpStarted = millis();
        if (debug)
          Serial.println("New client for COM");
      }
    }
    if (TCPClient && TCPClient.available())
    { // find if Client available
      while (TCPClient.available() && cpt_buffer_tcp < BUFFERSIZE - 1)
      {
        buffer_tcp[cpt_buffer_tcp] = TCPClient.read(); // read char from Tcp client
        if (cpt_buffer_tcp < BUFFERSIZE - 1)
          cpt_buffer_tcp++;
      }
      buffer_tcp[cpt_buffer_tcp] = 0;
      if (cpt_buffer_tcp >= BUFFERSIZE - 1) {
        TCPClient.stop(); cpt_buffer_tcp = 0; break;
      }
      if (String(buffer_tcp).indexOf("\r\n\r\n") < 0) break;

      /////////////////////////
      // Test la Requete reçue
      /////////////////////////
      str = TestIfReqValide(String(buffer_tcp));
      if (str == "")
      {
        step_tcp = ATTENTE_REQUETE;
      }
      else
        step_tcp = READ_EVSE;
    }
    if (TCPClient && millis() - tcpStarted > 2000) {
      TCPClient.stop(); cpt_buffer_tcp = 0;
    }
    break;

  /////////////
  case READ_EVSE:
    str_com = "";
    // Commandes de pilotage ESP32 accessibles depuis Jeedom via le port 9200 :
    // /$MODE%20stop, /$MODE%20manual, /$MODE%20solar et /$MODE%20legacy.
    // Elles doivent rester disponibles même quand le régulateur solaire est actif.
    {
      String requestedMode;
      if (parseJeedomModeCommand(str, requestedMode)) {
        String error = applyControlMode(requestedMode);
        str_com = error.isEmpty() ? "$MODE OK " + controlMode : "$MODE ERROR " + error;
        step_tcp = ENVOI_REQ;
        break;
      }
    }
    if (controlMode != "legacy" && str != " ") {
      str_com = "Pilotage web actif : rendre la main à la borne / Jeedom pour envoyer une commande";
      step_tcp = ENVOI_REQ;
      break;
    }
    // Si il n'y a pas de commande dans la requête (lu comme un espace)
    if (str == " ")
    {
      str = "evse_state + $GG*B2"; // Alors on fait une double lecture des status
      str_com = Read_Evse("evse_state");
      str2_com = Read_Evse("$GG*B2");
      str_com = str_com + str2_com;
    }
    else // Sinon on fait une lecture classique
    {
      str_com = Read_Evse(str);
    }
    step_tcp = ENVOI_REQ;
    break;

  ////////////////
  case ENVOI_REQ:
    if (str_com.indexOf("Pas de reponse du Vetronic") != -1)
      str = DoRepXml(str, "444", "");
    else
      str = DoRepXml(str, "200", str_com);
    SendResponseHead(str); // renvoie la reponse COM2 to TCP WIFI
    TCPClient.stop();
    step_tcp = UPDATE_EVSE;
    break;

  ////////////////
  case UPDATE_EVSE:
    timer = millis();   // Mise à zéro du timer de raffraichissement
    UpdateEvseStatus(); // Interrogation de la borne
    step_tcp = UPDATE_JEEDOM;
    break;

  ////////////////
  case UPDATE_JEEDOM:
    // Envoi vers Jeedom uniquement en cas de changement
    // Si le code status a changé
    if (evse_state.code_status != previous_evse_state.code_status)
    {
      previous_evse_state.code_status = evse_state.code_status;
      UpdateJeedom(String(params.id_code_status), String(evse_state.code_status));
    }
    // Si le texte du status a changé
    if (strncmp(evse_state.txt_status, previous_evse_state.txt_status, sizeof(evse_state_t::txt_status) - 1) != 0)
    {
      strncpy(previous_evse_state.txt_status, evse_state.txt_status, sizeof(evse_state_t::txt_status) - 1);
      UpdateJeedom(String(params.id_txt_status), String(evse_state.txt_status));
    }
    // Si la tension a changé
    if (evse_state.tension != previous_evse_state.tension)
    {
      if (evse_state.tension < 5)
      {
        UpdateJeedom(String(params.id_tension), "0");
      }
      else
      {
        UpdateJeedom(String(params.id_tension), String(evse_state.tension));
      }
      previous_evse_state.tension = evse_state.tension;
    }
    // Si le courant a changé
    if (evse_state.courant != previous_evse_state.courant)
    {
      if (evse_state.courant <= 100)
      {
        UpdateJeedom(String(params.id_courant), "0");
      }
      else
      {
        UpdateJeedom(String(params.id_courant), String(evse_state.courant));
      }
      previous_evse_state.courant = evse_state.courant;
    }
    // Si le courant max a changé
    if (evse_state.courant_max != previous_evse_state.courant_max)
    {
      previous_evse_state.courant_max = evse_state.courant_max;
      UpdateJeedom(String(params.id_courant_max), String(evse_state.courant_max));
    }
    step_tcp = ATTENTE_REQUETE;
    break;
  }
}
