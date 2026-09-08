#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <String.h>

/*************************  Constante ****************************/
#define VERSION "v3.1-rs485-lilygo"
// Identifiants HTTP Basic (fenêtre native du navigateur), à personnaliser.
#define WEB_USER "admin"
#define WEB_PASSWORD "vetronic"
// Réseau essayé au démarrage ; AutoConnect reste disponible en secours.
// Copier secrets.example.h vers secrets.h pour une connexion preconfiguree.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID ""
#endif
#ifndef DEFAULT_WIFI_PASSWORD
#define DEFAULT_WIFI_PASSWORD ""
#endif
#define ATTENTE_REQUETE 1
#define READ_EVSE 2
#define ENVOI_REQ 3
#define UPDATE_EVSE 4
#define UPDATE_JEEDOM 5
#define END 3
#define BUFFERSIZE 16384

/*************************  COM Port 2 ****************************/
#define UART_BAUD2 115200         // Baudrate UART2
#define SERIAL_PARAM2 SERIAL_8N1  // Data/Parity/Stop UART2
#define SERIAL2_RXPIN 32          // WB-01 via MAX3232 : RX sur connecteur libre
#define SERIAL2_TXPIN 33          // WB-01 via MAX3232 : TX (IO4 = LED LILYGO)

// LILYGO T-CAN485 / XY_32_CAN+RS485 V1.0 : UART1 réservé au Deye.
// MAX13487 à direction automatique, commandes fixes selon l'exemple LILYGO.
#define DEYE_RS485_RX 21
#define DEYE_RS485_TX 22
#define DEYE_RS485_CALLBACK 17
#define DEYE_RS485_ENABLE 19
#define DEYE_RS485_POWER 16

// LED RGB WS2812B intégrée à la LILYGO T-CAN485.
#define LILYGO_LED_PIN 4
/**********************  Port d'écoute TCP ************************/
#define SERIAL2_TCP_PORT 9200  // Wifi Port UART2

/*********************  Timeout port COM2 *************************/
const long timeoutTime = 100;  // Tempo d'attente reception COM Vetronic en millisecond

/*************  Gestion de la sauvegarde en EEPROM  ****************/
// identifiant pour retrouver les données en EEPROM
#define PARAM_ID "VETRONIC"

// Paramètres de Jeedom
typedef struct
{
  char id[sizeof(PARAM_ID)];
  char  ip_jeedom[16];
  char  api_key_jeedom[255];  
  uint16_t id_code_status;
  uint16_t id_txt_status;
  uint16_t id_tension;
  uint16_t id_courant;
  uint16_t id_courant_max;
  unsigned long freq_update_evse;
} param_t;

/*************  Etat de la borne  ****************/
typedef struct
{
  uint16_t code_status;
  char txt_status[40];
  uint16_t tension;
  uint16_t courant;
  uint16_t courant_max;
} evse_state_t;

/****************  Messages de debug  ******************/
extern bool debug ;

#endif // !_CONFIG_H_
