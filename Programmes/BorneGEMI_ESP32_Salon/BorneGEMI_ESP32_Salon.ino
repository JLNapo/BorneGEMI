/*
 * Gestion de la borne GEMI Salon
 * Cette partie de la borne execute les commandes et correspond avec GEMI pour celles qu'il ne peut traiter.
 *
 * Processeur ESP32 (DOIT ESP32 DEVKIT V1)
 * Amplificateur: MAX98357A (3w)
 * LED pilotées par : 74HC595 (8-bit serial - in / serial or parallel - out shift register with output latches 3 - state)
 * Touches sensitives
 *
 * Proc. IDE arduino  : DOIT ESP32 DEVKIT V1
 * Proc. RTC (heure)  : DS3231 (sauvegardé par batterie)
 * Mot de passe WEB   : jlN300855
 * Mot de passe MAJ   : jlN300855
 *
 *
 */

#include <WebServer.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <Update.h>
#include <WiFiUdp.h>
//#include <Wire.h>
#include "RTClib.h"
#include <Audio.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include "ESP8266FtpServer.h"
#include "OTApage.h"
#include "esp32_ping.h"

/* Necessaire au Réseau WiFi */
String SysDomo = "GEMI";                                                              // Nom donné au système domotique (pour moi GEMI => Gestion Electronique de Maison Individuelle)
String MyName = SysDomo + "_Salon";                                                   // Nom visible sur le réseau <= Si nécessaire la fonction ESP.getChipId() peut ajouter l'ID de la puce à la fin.
int nbReseauWiFi = 4;                                                                 // Nombre de réseau disponnibles dans le lieu
String ssid1 = "WiFi_1";                                                              // SSID (Nom du reseau wifi)
String pass1 = "MDP WiFi 1";                                                          // Password (Mot de passe wifi)
String ssid2 = "WiFi_2";                                                              // SSID (Nom du reseau wifi)
String pass2 = "MDP WiFi 2";                                                          // Password (Mot de passe wifi)
String ssid3 = "WiFi_3";                                                              // SSID (Nom du reseau wifi)
String pass3 = "MDP WiFi 3";                                                          // Password (Mot de passe wifi)
String ssid4 = "WiFi_4";                                                              // SSID (Nom du reseau wifi)
String pass4 = "MDP WiFi 4";                                                          // Password (Mot de passe wifi)
IPAddress ipAdresse(192, 168, 000, 000);                                              // IP locale de ce module (fourni par le serveur dns, normalement la Box)
IPAddress gateway(192, 168, 000, 000);                                                // L'IP de la box
IPAddress subnet(255, 255, 255, 0);                                                   // Mask réseau
IPAddress dns(192, 168, 000, 000);                                                    // L'IP de la box (ou d'un serveur NTP)
const uint32_t connectTimeoutMs = 10000;                                              // Tmeout de connexion WiFiMulti
String MyIP = "";                                                                     // Contient l'adresse IP avec séparateur (point) comme chaine
String MAC = "";                                                                      // Adresse MAC du module (peut servir pour éventuellement paramétrer la box)
String MySSID = "";
String MyPass = "";
String FTPuser = "Utilisateur FTP";                                                   // Utilisateur FTP
String FTPpass = "MDP FTP";                                                           // Mot de passe FTP
String OTApass = "MDP OTA";                                                           // Mot de passe OTA
char USR_WEB[] = "Utilisateur WEB";                                                   // Utilisateur pour le web (indispensable)
char PSS_WEB[] = "MDP WEB";                                                           // Mot de passe pour le web (indispensable)
bool Log = false;                                                                     // Enregistrer dans un fichier Log les différentes actions
byte LogMois = 0;                                                                     // Mois en cours pour les Log

String VoiceRss = F("http://api.voicerss.org/?key=b1b971dd1c88484799e674f32ffd4e66&hl=fr-fr&c=MP3&f=32khz_16bit_mono&src=");
String UID_Nano = F("AD-6A-DF-9D-1D-4F-34-72");
String Access_Key = F("uFvtzsB/s20inH3sEUxAbNIBucyfhdHxcu7ARpL580euQKg5kMQzzA==");

/************************************** CE QUI EST AU DESSUS DE CETTE LIGNE EST SPECIFIQUE A CHAQUE BORNE, AU DESSOUS tout EST COMMUN A TOUTES LES BORNES *******************************************************/

const char Version[] = "0.2.1.0";

/* I2S Connexion et boutons */
#define I2S_DOUT        25                                                            // I2S pour la connexion avec l'ampli MAX98357A
#define I2S_BCLK        26
#define I2S_LRC         27
#define Serial_RX       16                                                            // 16 et 17 servent au port série de communication avec le NANO 33 BLE Sense
#define Serial_TX       17
#define bgClock         13                                                            // Clock du 74HC595 (BarGraph)
#define bgLatch         12                                                            // Latch du 74HC595 (BarGraph)
#define bgData          14                                                            // Data  du 74HC595 (BarGraph)
#define btnPlus         19                                                            // Bouton "plus" volume (Touche sensitive capacitive)
#define btnMoins        18                                                            // Bouton "moins" volume (Touche sensitive capacitive)
#define btnMicro        23                                                            // Bouton Arrêt/Marche micro (en réalité pas d'interprétation de ce qui est entendu) (Touche sensitive capacitive)
#define ledMicro        33                                                            // (Rvb) LED Arrêt/Marche micro (en réalité pas d'interprétation de ce qui est entendu) (partie rouge de la LED RVB)
#define ledEcoute       32                                                            // (rvB) LED Arrêt/Marche écoute (allumé avec "Hé! GÉMI" et s'éteint après la commande ou l'abandon utilisateur après ~5/6s ou commande non comprise (avec message).
#define ledWiFi         15                                                            // (rVb) Allumée pendant la mise à l'heure ou la connexion WiFi (verte)
                                                                                      // Etat des couleurs de la LED RVB => Rouge = micro coupé (juste pas d'analyse). Bleu = écoute de cde. Verte = Pas de WiFi ou Mise à l'heure en cours.
/* Diverses variables */
int Volume = 15;                                                                      // Volume par défaut au premier démarrage
int VolDef = 15;                                                                      // Volume par défaut pour certaine réponse importante (ex: alarme arrivée à son terme)
bool VolReg = false;                                                                  // Pour enregistrer après un temps le réglage du volume depuis la borne
bool LedReg = false;                                                                  // Pour l'allumage des leds de volume
int LedVal = 1;                                                                       // Valeur des LEDs allumées
const uint8_t VolumeTable[8]={1, 3, 7, 15, 31, 63, 127, 255};                         // 8 elements (8 LEDs)
bool Micro = true;                                                                    // Micro en fonction (par défaut à true)
unsigned long VolMillis;                                                              // Temporisation avant de sauvegarder la valeur du volume
unsigned long LedMillis;                                                              // Temporisation avant l'exctinction des leds
byte NumStation = 1;                                                                  // Station par défaut
byte NbreRadios = 22;                                                                 // Nombre maxi de Stations de radio
byte NbreBornes = 3;                                                                  // Nombre maxi de bornes domotique
String RadioOld = "";                                                                 // Lien http vers la station de radio en cours.
bool RadioON = false;                                                                 // Passe à true quand la radio marche, sinon false
bool BtnON = false;                                                                   // Flag bouton appuyé
bool TouchON = false;                                                                 // Flag Touche sensitive appuyée
String startString = "";                                                              // Date heure de démarrage
String webString = "";
bool bgReg[8];                                                                        // Registre à décalage du 74HC595
bool bgSens = false;
bool Fait = false;                                                                    // Commande terminée
bool HandShakeWait = false;                                                           // Passe à true si l'ESP32 recois "open" (ouverture d'une commande => le NANO entend "Hé GEMI" et envoi "open" à l'ESP32). L'ESP32 joue le son "on.mp3" et envoi "ready" au NANO (HandShake)
bool HandShakeState = false;                                                          // Statut du HandShake de toute la commande => passe à true au début et ne passe à false que lorsque toute la commande est terminée (réponse de l'objet connecté comprise).
unsigned long HandShakeMillis;                                                        // Pour le timeout du "HandShake" en cas d'un faux positif.
int HandShakeTimeout = 10000;                                                         // Nombre de millisecondes du timeout de la communication NANO/ESP32 ("HandShake")
bool AudioState = false;                                                              // Passe à true au début de lecture d'un MP3 et à false lorsque c'est terminé.
bool Alarme = false;                                                                  // Pour le déclenchement d'une alarme (ex: "mets une alarme dans 10mn" ou "mets une larme à 10H15")
bool AlarmeEtape = false;                                                             // Fait sonner l'alarme une deuxième fois au bot de 5s
uint32_t AlarmeEnSeconde = 0;                                                         // Temps en secondes pour la programmation de l'alarme
float TempOffSet = 0.0;                                                               // OffSet (décalage) de la température mesurée (pour tenir compte de la température dûe au fonctionnement de l'électronique)
bool TestLEDStart = false;                                                            // Faire un test des LEDs au démarrage (oui/non => Defaut: NON) on peut régler ce paramètre ou faire un test depuis la page WEB.

/* functions qui retournent la date en strings (char) en francais */
// Longueur des chaînes pour chaque jour ou mois
#define SECS_PER_HOUR ((time_t)(3600UL))                                              // Necessaire au calcul en fonction de l'heure recueillie par NTP.
#define dt_SHORT_STR_LEN  3                                                           // La longueur des chaînes courtes pour les ou les mois
#define dt_MAX_STRING_LEN 9                                                           // Longueur de la chaîne de date la plus longue (hors null de fin)
static char buffer[dt_MAX_STRING_LEN+1];                                              // Doit être suffisamment grand pour la chaîne la plus longue et la valeur null de fin
const PROGMEM char * const PROGMEM monthNames_P[] = {"Décembre","Janvier","Février","Mars","Avril","Mai","Juin","Juillet","Aout","Septembre","Octobre","Novembre","Décembre"};
const PROGMEM char * const PROGMEM dayNames_P[] = {"Dimanche","Lundi","Mardi","Mercredi","Jeudi","Vendredi","Samedi","Dimanche"};
const char dayShortNames_P[] PROGMEM = "DimLunMarMerJeuVenSam";
const char monthShortNames_P[] PROGMEM = "ErrJanFevMarAvrMaiJunJulAouSepOctNovDec";

char* monthStr(uint8_t month) {
  strcpy_P(buffer, (PGM_P)pgm_read_ptr(&(monthNames_P[month])));
  return buffer;
}

char* monthShortStr(uint8_t month) {
  for (int i=0; i < dt_SHORT_STR_LEN; i++)
    buffer[i] = pgm_read_byte(&(monthShortNames_P[i+ (month*dt_SHORT_STR_LEN)]));
  buffer[dt_SHORT_STR_LEN] = 0;
  return buffer;
}

char* dayStr(uint8_t day) {
  strcpy_P(buffer, (PGM_P)pgm_read_ptr(&(dayNames_P[day])));
  return buffer;
}

char* dayShortStr(uint8_t day) {
  uint8_t index = day*dt_SHORT_STR_LEN;
  for (int i=0; i < dt_SHORT_STR_LEN; i++)
    buffer[i] = pgm_read_byte(&(dayShortNames_P[index + i]));
  buffer[dt_SHORT_STR_LEN] = 0;
  return buffer;
}

/* Necessaire pour la date et l'heure */                                              // NTP actuel : Adresse IP de la box (si NTP a été mis dans les paramètres) ou exemple ntp.midway.ovh (80, 67, 184, 1)
IPAddress TimeServer(192, 168, 000, 000);                                             // Pour l'heure avec l'adresse de la box internet (certaines comme free ont un serveur NTP)
const int NTP_PACKET_SIZE = 48;                                                       // L'horodatage NTP se trouve dans les 48 premiers octets du message
byte packetBuffer[NTP_PACKET_SIZE];                                                   // Tampon pour contenir paquets entrants et sortants
uint16_t TimePort = 2390;                                                             // Port local pour écouter les paquets UDP Normalement écoute sur 2390 et envoi sur 123 (essai possible : 2390, 123, 8888)
int TimeZone = 1;                                                                     // Central European Time
bool HeureOk = false;                                                                 // L'heure a été trouvé sur internet
bool MaHenCours = false;                                                              // La mise à l'heure est en cours d'exécution
byte oldJour = 0;                                                                     // Dernier jour mémorisé (flag)
byte oldHeure = 0;                                                                    // Dernière heure mémorisée (flag)
byte oldMinute = 0;                                                                   // Dernière minute mémorisée (flag)
byte oldSecond = 0;                                                                   // Dernière seconde mémorisée (flag)
bool hEteHiver = true;                                                                // Appliquer le changement d'heure été/hiver
uint16_t nbMinJour = 0;                                                               // Nombre de minute écoulées depuis le début de journée (depuis 00:00)

/* Instantiation et gestion des objets */
WiFiMulti wifiMulti;
WiFiUDP udp;                                                                          // Une instance UDP pour nous laisser envoyer et recevoir des paquets NTP
WebServer server(80);                                                                 // Initialisation de la librairie serveur
WiFiClient client;                                                                    // Initialisation de la librairie client (WiFiClient)
FtpServer ftpSrv;                                                                     // Initialisation du serveur FTP
Audio audio;                                                                          // Création de l'objet Audio
RTC_DS3231 rtc;                                                                       // Création de l'objet RTC pour l'heure
DateTime now;
bool EteHiver = true;                                                                 // Appliquer l'heure d'été si pas de changement été/hiver, sinon l'heure d'hiver sera prise.
HardwareSerial SerialNano(1);                                                         // Déclaration du port série

void Attend(int tmp) {                                                                // Remplace delay() sans arrêter les taches du processeur (en millisecondes)
  unsigned long horloge = millis() + tmp;
  while (horloge > millis()) {
    yield();
  }
}

void setup() {
  bool Connexion = false;                                                               // Statut de la connexion réseau
  pinMode(btnPlus, INPUT_PULLDOWN);
  pinMode(btnMoins, INPUT_PULLDOWN);
  pinMode(btnMicro, INPUT_PULLDOWN);
  pinMode(ledMicro, OUTPUT);
  pinMode(ledEcoute, OUTPUT);
  pinMode(ledWiFi, OUTPUT);
  pinMode(bgClock, OUTPUT);
  pinMode(bgLatch, OUTPUT);
  pinMode(bgData, OUTPUT);
  digitalWrite(ledMicro, LOW);
  digitalWrite(ledEcoute, LOW);
  digitalWrite(ledWiFi, LOW);
//  Serial.println(F("\nDebut du SETUP.\n"));

  Serial.begin(115200);                                                               // Pour le debuggage
  SerialNano.begin(115200, SERIAL_8N1, Serial_RX, Serial_TX);                         // initialize UART pour dialogue avec NANO BLE Sense

  SPIFFS.begin();
//  Serial.println(F("Démarrage RTC"));
  if (!rtc.begin()) {
//    Serial.println(F("Impossible de trouver le RTC !"));
  } else {
//    Serial.println(F("Le RTC a été trouvé !"));
  }
  Attend(100);
  now = rtc.now();

  /* Selection du volume et d'une Station par défaut */
  LireConfig();
  LireConfigLog();
  LireAlarme();                                                                       // Lecture et affichage de la programmation des alarmes (LED).
  LireValeurDefaut();
  if (Volume == 0) Volume = 15;

  digitalWrite(LED_BUILTIN, HIGH);                                                    // Extinction de la LED_BUILTIN
  digitalWrite(ledWiFi, HIGH);
  WiFi.setHostname(MyName.c_str());
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    Attend(10);
  }
  char s1[ssid1.length()+1];
  char p1[pass1.length()+1];
  ssid1.toCharArray(s1, ssid1.length()+1);
  pass1.toCharArray(p1, pass1.length()+1);
  char s2[ssid2.length()+1];
  char p2[pass2.length()+1];
  ssid2.toCharArray(s2, ssid2.length()+1);
  pass2.toCharArray(p2, pass2.length()+1);
  char s3[ssid3.length()+1];
  char p3[pass3.length()+1];
  ssid3.toCharArray(s3, ssid3.length()+1);
  pass3.toCharArray(p3, pass3.length()+1);
  char s4[ssid4.length()+1];
  char p4[pass4.length()+1];
  ssid4.toCharArray(s4, ssid4.length()+1);
  pass4.toCharArray(p4, pass4.length()+1);
  wifiMulti.addAP(s1, p1);                                                            // Ajoute les réseaux Wifi auxquels on souhaite se connecter
  wifiMulti.addAP(s2, p2);
  wifiMulti.addAP(s3, p3);
  wifiMulti.addAP(s4, p4);
  WiFi.setAutoReconnect(true);
  WiFi.config(ipAdresse, gateway, subnet, dns);                                       // Le DNS n'est pas indispensable
  int i = 1;
  int state;
  while (wifiMulti.run() != WL_CONNECTED) {
    Attend(500);
//    Serial.print(F("."));
    state = state == LOW ? HIGH : LOW;
    digitalWrite(LED_BUILTIN, state);                                                 // Voyant de fonctionnement au bleu (chargement et connection)
    i++;
    if (i >= 40) {
      break;
    }
  }
//  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Connexion = true;
    WiFi.setHostname(MyName.c_str());
    uint8_t mac[6];
    WiFi.macAddress(mac);
    MAC = macToStr(mac);
    ipAdresse = WiFi.localIP();
    MyIP = String(ipAdresse[0]) + "." + String(ipAdresse[1]) + "." + String(ipAdresse[2]) + "." + String(ipAdresse[3]);
    MySSID = WiFi.SSID();
    MyPass = WiFi.psk();
//    Serial.println(MyIP);
//    Serial.println(MySSID);
//    Serial.println(MAC);
//    Serial.println();
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(ledWiFi, LOW);
  } else {
//    Serial.println(F("Connexion WiFi ultérieur."));
  }

//  Serial.print(F("Mise à l'heure "));
  HeureOk = false;
  udp.begin(TimePort);
  // Connexion OK, on mets l'horloge RTC à jour
  getNtpTime();
  // Récupère les paramètres du RTC
  oldJour = now.day();
  oldHeure = now.hour();
  startString = String(dayShortStr(now.dayOfTheWeek())) + " " + twoDigits(now.day()) + " " + String(monthShortStr(now.month())) + " " + String(now.year()) + " " + twoDigits(now.hour()) + ":" + twoDigits(now.minute()) + ":" + twoDigits(now.second());
  if(HeureOk == false) {
//    Serial.println(F("à faire"));
    digitalWrite(ledWiFi, HIGH);
  } else {
//    Serial.println(F("ok."));
//    Serial.println(DateDuJourLong() + " " + String(now.hour()) + ":" + String(now.minute()));
    digitalWrite(ledWiFi, LOW);
  }

  ftpSrv.begin(FTPuser, FTPpass);                                                     // Username, password pour le FTP.  Modifier le ports dans ESP8266FtpServer.h  (defaut 21, 50009 pour PASV)
//  Serial.println(F("Server FTP lancé."));

  server.on("/", []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    loadFromSPIFFS("/index.htm");
  });
  server.on("/ajaxsauve", []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    handleSave();
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", utf8ascii(F("Sauvegarde effectuée.")));
  });
  server.on("/ajaxparam", handleAjaxParam);
  server.on("/ajaxcde", handleAction);
  server.on("/ajaxlistesons", handleListeSons);
  server.on("/ajaxlisteradios", handleListeRadios);
  server.on("/testled", []() {
    String argName = "";
    String argValue = "";
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    if (server.args() > 0) {
      argName = server.argName(0);
      argValue = server.arg(0);
      webString = TestLED(argValue.toInt());
      server.send(200, "text/plain", webString);
    } else {
      server.send(200, "text/plain", F("L'argument nécessaire au test est manquant !"));
    }
    server.sendHeader("Connection", "close");
  });
  server.on("/play",  []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
//    Serial.println(F("Commande Play Web"));
    handleJouerSon();
  });
  server.on("/config", []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    handleFichier("/config.ini");                                                     // Affiche le contenu des fichiers de configuration
    server.sendHeader("Connection", "close");
  });
  server.on("/configlog", []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    handleFichier("/configlog.ini");                                                  // Affiche le contenu des fichiers de configuration
    server.sendHeader("Connection", "close");
  });
  server.on("/defaut", []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    handleFichier("/defaut.txt");                                                     // Affiche le contenu des fichiers de configuration
    server.sendHeader("Connection", "close");
  });
  server.on("/logfile", []() {                                                        // Affiche les fichiers LOGs
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    handleLogFile();
    server.sendHeader("Connection", "close");
  });
  server.on("/micro", []() {
    Micro = !Micro;
    if (Micro == false) {
      digitalWrite(ledMicro, HIGH);
//      Serial.println(F("Micro coupé."));
      AudioLocalVol("micro_off", VolDef);
      server.send(200, "text/plain", F("Le micro a été désactivé."));
    } else {
      digitalWrite(ledMicro, LOW);
//      Serial.println(F("Micro actif."));
      LedVolume(Volume);
      LedMillis = millis() + 3000;
      LedReg = true;
      AudioLocalVol("micro_on", VolDef);
      server.send(200, "text/plain", F("Le micro a été activé."));
    }
    server.sendHeader("Connection", "close");
  });
  server.on("/heure", []() {
    String argName = "";
    String argValue = "";
    uint8_t d, m, h, n, s;
    uint16_t y;
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    if (server.args() == 0) {
      HeureOk = false;
      server.send(200, "text/plain", F("Mise à l'heure en cours."));
      digitalWrite(ledWiFi, HIGH);
    } else if (server.args() != 2) {
      server.send(200, "text/plain", F("Mise à l'heure impossible.\nNombre d'arguments invalides."));
    } else {
      for (uint8_t i=0; i<server.args(); i++) {
        argName = server.argName(i);
        argValue = server.arg(i);
        argValue.trim();
//        Serial.print(argName);
//        Serial.print(F(" = "));
//        Serial.println(argValue);
        if(argName == "date") {
          y = MotPos(argValue, "-", 1).toInt();
          m = MotPos(argValue, "-", 2).toInt();
          d = MotPos(argValue, "-", 3).toInt();
        } else if(argName == "heure") {
          h = MotPos(argValue, ":", 1).toInt();
          n = MotPos(argValue, ":", 2).toInt();
          s = MotPos(argValue, ":", 3).toInt();
        }
      }
      rtc.adjust(DateTime(y, m, d, h, n, s));                                                                  // mise à l'heure de l'horloge
      HeureOk = true;
      server.send(200, "text/plain", F("Mise à l'heure effectuée."));
    }
    server.sendHeader("Connection", "close");
  });
  server.on("/ajaxlstradio", []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    handleRadios();
  });
  server.on("/ajaxsauveradio", []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    handleSauveRadios();
    server.sendHeader("Connection", "close");
  });
  server.on("/radio",  []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
//    Serial.println(F("Commande Play Radio depuis le Web"));
    handleJouerRadio();
  });
  server.on("/radiostop",  []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
//    Serial.println(F("Commande Stop Radio depuis le Web"));
    RadioON = false;
    RadioOld = "";
    AudioState = false;
    audio.stopSong();
    if (server.argName(0) == "nom") {
      webString = F("La radio ");
      webString += server.arg(0);
      webString += F(" a été arrêtée.");
    } else {
      webString = F("La radio a été arrêtée.");
    }
//    Serial.println(webString);
    server.send(200, "text/plain", webString);
    server.sendHeader("Connection", "close");
  });
  server.on("/volume",  []() {
    int Vol = 0;
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
//    Serial.println(F("Commande volume Radio et Sons depuis le Web"));
    if (server.args()) {
      if (server.argName(0) == "vol") {
        Vol = server.arg(0).toInt();
        SetVolumeWeb(Vol);
//        Serial.print(F("Volume : "));
//        Serial.println(Vol);
      }
      webString = F("Le volume a été provisoirement réglé sur : ");
      webString += String(Vol);
      server.send(200, "text/plain", webString);
      server.sendHeader("Connection", "close");
    }
  });
  server.on("/ajaxlstborne", []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
//    Serial.println(F("Liste des bornes"));
    handleBornes();
  });
  server.on("/ajaxsauveborne", []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    handleSauveBornes();
    server.sendHeader("Connection", "close");
  });

  /* Necessaire à la mise à jour par le WEB */
  server.on("/firmware", HTTP_GET, []() {
    if (!server.authenticate(USR_WEB, PSS_WEB))
      return server.requestAuthentication();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginOTA);
  });
  server.on("/serverota", HTTP_POST, []() {
    byte narg = 0;
    for (uint8_t i = 0; i < server.args(); i++) {
      if (server.argName(i) == "userid" && server.arg(i) == FTPuser) narg++;
      if (server.argName(i) == "pwd" && server.arg(i) == OTApass) narg++;
    }
    server.sendHeader("Connection", "close");
    if (narg == 2) {
      server.send(200, "text/html", serverOTA);
    } else {
      server.send(200, "text/html", F("Erreur utilisateur ou mot de passe !"));
    }
  });
  /* gestion du téléchargement du fichier de firmware */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP */
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (!Update.end(true)) { // true to set the size to the current progress
        Update.printError(Serial);
      }
    }
  });
  server.onNotFound(handleNotFound);
  server.begin();                                                                     // Lancement du serveur http
//  Serial.println(F("Server WEB lancé."));

  // Setup I2S
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT); //, I2S_PIN_NO_CHANGE);

  // Test des LEDs
  if (TestLEDStart == true) {
    TestLED(-1);
  }
  // Set Volume
  audio.setVolume(Volume);
  audio.forceMono(true);
  audio.setTone(6, 6, 1); // De -40db à 6db pour chaque valeur (réglage de la tonalité)
  LedVolume(Volume);
//  Serial.print(F("Volume : "));
//  Serial.println(Volume);

  // Open music file
//  AudioLocalVol("wobble", VolDef);
  AudioLocal("wobble");
//  Serial.println(DateDuJourLong() + " " + twoDigits(now.hour()) + ":" + twoDigits(now.minute()));
  digitalWrite(ledMicro, LOW);
  digitalWrite(ledEcoute, LOW);
  digitalWrite(ledWiFi, LOW);
  LedVal = 1;
  LedReg = true;
  // Debug
//  Serial.print(F("Moins = "));
//  Serial.println(digitalRead(btnMoins));
//  Serial.print(F("Plus = "));
//  Serial.println(digitalRead(btnPlus));
//  Serial.print(F("Micro = "));
//  Serial.println(digitalRead(btnMicro));
//  Serial.print(F("Version : "));
//  Serial.println(Version);
//  Serial.println();
}

void loop() {
  server.handleClient();
  ftpSrv.handleFTP();
  now = rtc.now();
  audio.loop();

  /* Changement de jour et forcer une mise à l'heure */
  if (oldJour != now.day()) {                                                         // Tous les jours à 03h03 mise à l'heure pour prise en compte de l'heure Hiver/Eté
    if (oldHeure != now.hour()) {
      if (now.hour() == 3 && now.minute() == 3) {
        oldJour = now.day();
        oldHeure = now.hour();
        HeureOk = false;
        digitalWrite(ledWiFi, HIGH);
      }
    }
  }
  if (now.second() % 10 == 0 && oldSecond != now.second()) {                          // Pour une tentative toutes les 10s
    if (HeureOk == false) {
//      Serial.print(F("Mise à l'heure "));
      time_t dt = getNtpTime();
      if (HeureOk == true) {
        oldJour = now.day();
        oldHeure = now.hour();
        oldSecond = now.second();
//        Serial.println(F("ok"));
//        Serial.println(DateDuJourLong() + " " + twoDigits(now.hour()) + ":" + twoDigits(now.minute()));
        digitalWrite(ledWiFi, LOW);
      } else {
//        Serial.println(F("à faire"));
        oldSecond = now.second();
        digitalWrite(ledWiFi, HIGH);
      }
    }
  }
  /* Gestion du son et du HandShake */
  if (AudioState == true) {
    if (!audio.isRunning()) {
//      Serial.println(F("Audio stop"));
      audio.setVolume(Volume);
      audio.stopSong();
      AudioState = false;
      if (Fait == true) EnAttenteCde();
      if (HandShakeState == false && HandShakeWait == false) {
//        Serial.println(F("HandShake => false"));
        if (RadioOld != "" && RadioON == true) {
          AudioHost(RadioOld);
        }
      }
    }
  }
  /* Communication HandShake */
  if(SerialNano.available() > 0) {                                                    // Pour dialogue avec NANO BLE Sense
    String r = "";
    while(SerialNano.available()) {
      char s = SerialNano.read();
      r += s;
      Attend(10);
    }
    r.toLowerCase();
    r.trim();
    r.replace("\r", "");
    r.replace("\n", "");
//    Serial.print(F("Recu: "));
//    Serial.println(r);
    if(Micro == true) {
//      Serial.println(F("Micro ouvert"));
      digitalWrite(ledEcoute, HIGH);
      if (r == "open") {
        audio.stopSong();
        HandShakeWait = true;
        AudioLocalVol("on", VolDef);
        SerialNano.println(F("ready"));
        HandShakeMillis = millis() + HandShakeTimeout;
//        Serial.println(F("Prise en compte"));
      } else if(r != "erreur") {
        audio.stopSong();
        HandShakeMillis = millis() + HandShakeTimeout;
        HandShakeState = true;
//        Serial.println(F("Analyse début"));
        Analyser(r);
//        Serial.println(F("Analyse fin"));
        HandShakeMillis = millis() + HandShakeTimeout;
      } else if (r == "erreur") {
        AudioLocal("erreur");
        Fait = true;
      } else {
//        Serial.println(F("ESP32 n'a pas compris"));
        AudioLocal("pascompris");
        Fait = true;
      }
    } else {
//      Serial.println(F("Micro fermé"));
      HandShakeWait = true;
      if (r == "open") {
        audio.stopSong();
        AudioLocalVol("click", VolDef);
        SerialNano.println(F("ready"));
//        Serial.println(F("Prise en compte"));
      } else if (r != "erreur") {
        if (r.indexOf("micro") > 0) {
//          Serial.println(F("Analyse début"));
          Analyser(r);
//          Serial.println(F("Analyse fin"));
        }
      }
      HandShakeMillis = millis() + HandShakeTimeout;
    }
  }
  /* Timeout du HandShake */
  if (HandShakeState == true || HandShakeWait == true) {
    if (HandShakeMillis < millis()) {
      EnAttenteCde();
    }
  }
  /* Debug et commandes locales */
  if(Serial.available()>0) {                                                          // Pour debug, changer le volume sonore, faire jouer un son ou un streaming
    audio.stopSong();
    String r=Serial.readString(); r.trim();
    r.toLowerCase();
//    Serial.print(F("Recu DEBUG: "));
//    Serial.println(r);
    if (r.startsWith("cde=")) {                                                       // Pour tester une commande (ex: "cde=object:tv,location:séjour,number:5" ou "cde=object:tv,ordre:éteint,location:séjour")
      Analyser(MotPos(r, "=", 2));
    } else if (r.startsWith("vol=")) {
      SetVolume(MotPos(r, "=", 2).toInt());
    } else if (r.startsWith("http")) {
      AudioHost(r);                                                                   // Par exemple: http://cdn.nrjaudio.fm/audio1/fr/40104/mp3_128.mp3 pour Chérie FM
    } else {
      audio.setVolume(Volume);
      AudioLocal(r);
    }
  }
  /* Gestion de l'alarme */
  if(Alarme == true) {
    if (HeureEnSeconde(now.hour(), now.minute(), now.second()) >= AlarmeEnSeconde) {
      AudioLocalVol("alarmeoff", VolDef);
      AlarmeEnSeconde += 5;
      if(AlarmeEtape == false) {
        SupprimeAlarme();
        Alarme = false;
//        Serial.println(F("Fin de l'alarme."));
      }
      AlarmeEtape = false;
    }
  }
  /* Timeout du réglage de volume sonore */
  if(VolReg == true) {
    if(VolMillis < millis()) {
      SauveValeurDefaut();
      AudioLocal("volsave");
      VolReg = false;
//      Serial.println(F("Sauvegarde du volume"));
    }
  }
  /* Exctinction des LEDs après timeout */
  if(LedReg == true) {
    if(LedMillis < millis()) {
      EraseReg();
      LedReg = false;
//      Serial.println(F("Exctinction des LEDs"));
    }
  }
  /* Perte de connexion WiFi */
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ledWiFi, HIGH);
    if(LedMillis < millis()) {
      if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
        MyIP = IPtoString(WiFi.localIP());
        MySSID = WiFi.SSID();
        digitalWrite(ledWiFi, LOW);
      } else {
        if (bgSens == false) {
          LedVal = LedVal << 1;
          if (LedVal > 128) {
            bgSens = true;
            LedVal = 64;
          }
        } else {
          LedVal = LedVal >> 1;
          if (LedVal < 1) {
            bgSens = false;
            LedVal = 2;
          }
        }
        MakeReg(LedVal);
        LedMillis = millis() + 500;
        LedReg = true;
        MyIP = "";
      }
    }
  } else if (MyIP == "") {
    MyIP = IPtoString(WiFi.localIP());
    MySSID = WiFi.SSID();
    digitalWrite(ledWiFi, LOW);
  }
  /* Gestion des boutons */
  if(digitalRead(btnPlus) == HIGH && BtnON == false) {
    Volume++;
    SetVolume(Volume);
    BtnON = true;
//    Serial.println(F("Bouton plus"));
  }
  if(digitalRead(btnMoins) == HIGH && BtnON == false) {
    Volume--;
    SetVolume(Volume);
    BtnON = true;
//    Serial.println(F("Bouton moins"));
  }
  if(digitalRead(btnMicro) == HIGH && BtnON == false) {
    Micro = !Micro;
    if (Micro == false) {
      digitalWrite(ledMicro, HIGH);
//      Serial.println(F("Micro inactif."));
      AudioLocalVol("micro_off", VolDef);
    } else {
      digitalWrite(ledMicro, LOW);
//      Serial.println(F("Micro actif."));
      LedVolume(Volume);
      LedMillis = millis() + 3000;
      LedReg = true;
      AudioLocalVol("micro_on", VolDef);
    }
    BtnON = true;
//    Serial.println(F("Bouton micro"));
  }
  if (BtnON == true) {
    BtnON = digitalRead(btnPlus) == LOW && digitalRead(btnMoins) == LOW && digitalRead(btnMicro) == LOW ? false : true;
  }
}

void EnAttenteCde() {
  audio.setVolume(Volume);
  if (Micro == true) {
    AudioLocal("off");
  } else {
    AudioLocal("click");
  }
  digitalWrite(ledEcoute, LOW);
  HandShakeState = false;
  HandShakeWait = false;
  Fait = false;
//  Serial.println(F("En attente Cde a été activé."));
}

void Analyser(String Chaine) {
  int nbElement = NbMots(Chaine, ",");
  String Cde = "";
  String Mot1 = "";
  String Mot2 = "";
  String Unite[3];
  uint8_t Valeur[3];
  String Couleur = "";
  String Objet = "";
  String Objet2 = "";
  String Ordre = "";
  String Local = "";
  String Prenom = "";
  String Station = "";
  String Prepos = "";
//  uint32_t Total = 0;
  unsigned Delais;
  // Initialisation des variables
  for(int i=0; i<3; i++) {
    Valeur[i] = 0;
    Unite[i] = "";
  }
//  Serial.print(F("Nombe d'éléments à analyser: "));
//  Serial.println(nbElement);

  for (int i=1; i<= nbElement; i++) {
    Mot1 = MotPos(MotPos(Chaine, ",", i), ":", 1);
    Mot2 = MotPos(MotPos(Chaine, ",", i), ":", 2);

    if (Mot1 == "Conserne") {
      if(Mot2 == "changeColor" || Mot2 == "objet" || Mot2 == "virtuel") {
        Objet = "";
      }
    } else if (Mot1 == "color") {                                                                     /* CLOULEURS */
      Couleur = Mot2;
    } else if (Mot1 == "location") {                                                                  /* LOCALISATION */
      Local = Mot2;
      if (Mot2 == "salle de séjour" || Mot2 == "séjour" || Mot2 == "salle à manger") {
        if (Objet == "tv" || Objet == "lum" || Objet == "ventilo") {
          Cde = "http://192.168.000.000/";
        }
      } else if (Mot2 == "salon") {
        if (Objet == "tv") {
          Cde = "http://192.168.000.000/";
        }
      }
//      Serial.print(F("Local: "));
//      Serial.println(Mot2);
    } else if (Mot1 == "object") {                                                                   /* OBJETS */
      if (Mot2 == "télé" || Mot2 == "tv" || Mot2 == "télévision") {
        Objet = "tv";
      } else if (Mot2.startsWith("frig")) {
        Objet = "frigo";
      } else if (Mot2.startsWith("lum")) {
        Objet = "lum";
      } else if (Mot2.startsWith("ventil")) {
        Objet = "ventilo";
      } else if (Mot2.startsWith("boite") || Mot2 == "courrier") {
        Objet = "lettre";
      } else if (Mot2 == "GÉMI" || Mot2 == "serveur") {
        Objet = SysDomo;
      } else if (Mot2 == "adresse IP" || Mot2.startsWith("adresse")) {
        Objet = "adresse";
      } else if (Mot2 == "portail" || Mot2 == "Portillon") {
        Objet = Mot2;
        Cde = "http://192.168.000.000/";
      } else if (Mot2.startsWith("clim")) {
        Objet = "clim";
      } else if (Mot2 == "anniversaire") {
        Objet = "anniv";
      } else if (Mot2 == "temps" || Mot2 == "météo") {
        Objet = "meteo";
      } else if (Mot2 == "volume" || Mot2 == "son") {
        Objet = "vol";
      } else if (Mot2 == "micro") {
        Objet = "micro";
      } else {
        if (Objet == "") {
          Objet = Mot2;
        } else {
          Objet2 = Mot2;
        }
      }
//      Serial.print(F("Objet: "));
//      Serial.println(Objet);
    } else if (Mot1.startsWith("ordre")) {                                                            /* ORDRE */
      if (Mot2 == "marche" || Mot2 == "mets"  || Mot2 == "remets" || Mot2 == "mettre" || Mot2 == "allume" || Mot2 == "ouvre" || Mot2 == "en route") {
        Ordre = "on";
      } else if (Mot2.startsWith("arrêt") || Mot2.startsWith("étein") || Mot2 == "ferme" || Mot2 == "coupe") {
        Ordre = "off";
      } else if (Mot2 == "ajoute" || Mot2 == "monte" || Mot2 == "augmente") {
        Ordre = "plus";
      } else if (Mot2 == "supprime" || Mot2 == "annule" || Mot2 == "baisse" || Mot2 == "diminue") {
        Ordre = "moins";
      } else if (Mot2 == "redémarre") {
        Ordre = "faire";
      } else if (Mot2 == "souhaite") {
        Ordre = "dire";
      }
//      Serial.print(F("Ordre: "));
//      Serial.println(Ordre);
    } else if (Mot1.startsWith("time")) {                                                             /* TEMPS */
      if (Unite[0] == "") {
        Unite[0] = Mot2;
      } else if (Unite[1] == "") {
        Unite[1] = Mot2;
      } else {
        Unite[2] = Mot2;
      }
//      Serial.print(F("Unité: "));
//      Serial.print(Unite[0]);
//      Serial.print(F(", "));
//      Serial.print(Unite[1]);
//      Serial.print(F(", "));
//      Serial.println(Unite[2]);
    } else if (Mot1 == "prenom") {                                                                    /* PRENOM */
      Prenom = Mot2;
//      Serial.print(F("Prenom: "));
//      Serial.println(Prenom);
    } else if (Mot1 == "station") {
      Station = Mot2;
//      Serial.print(F("Station: "));
//      Serial.println(Station);
    } else if (Mot1 == "prepos") {
      Prepos = Mot2;
//      Serial.print(F("Préposition: "));
//      Serial.println(Prepos);
    } else if (Mot1.startsWith("number")) {                                                           /* CHIFFRES */
      if (Objet == "tv") {
        Cde += "tv?ch=" + Mot2;
      } else if (Objet == "alarme") {
        if (Valeur[0] == 0) {
          Valeur[0] = Mot2.toInt();
        } else if (Valeur[1] == 0) {
          Valeur[1] = Mot2.toInt();
        } else {
          Valeur[2] = Mot2.toInt();
        }
      }
//      Serial.print(F("Number: "));
//      Serial.print(Valeur[0]);
//      Serial.print(F(", "));
//      Serial.print(Valeur[1]);
//      Serial.print(F(", "));
//      Serial.println(Valeur[2]);
    }
  }
  /* ****************************** Fin des analyses et Préparation de l'exécution ***************************** */
  /* Micro pour l'écoute */
  if (Ordre == "off" && Objet == "micro") {
    HandShakeMillis = millis() + HandShakeTimeout;
    Micro = false;
    Cde = "";
    digitalWrite(ledMicro, HIGH);
//    Serial.println(F("Micro coupé."));
    AudioLocalVol("micro_off", VolDef);
    Fait = true;
  } else if (Ordre == "on" && Objet == "micro") {
    HandShakeMillis = millis() + HandShakeTimeout;
    Micro = true;
    Cde = "";
    digitalWrite(ledMicro, LOW);
//    Serial.println(F("Micro actif."));
    LedVolume(Volume);
    LedMillis = millis() + 3000;
    LedReg = true;
    AudioLocalVol("micro_on", VolDef);
    Fait = true;
  }
  /* Télévision */
  if (Objet == "tv") {
    if (Cde.indexOf("tv?ch=")<0) {
      if (Ordre == "on" || Ordre == "off") {
        Cde += "tv?pw=1";
      } else if (Ordre == "plus") {
        Cde += "tv?vol=plus";
      } else if (Ordre == "moins") {
        Cde += "tv?vol=moins";
      } else if (Objet2 = "pause") {
        Cde += "tv?vol=mute";
      }
    }
//    Serial.print(F("TV: "));
//    Serial.println(Cde);
    Fait = true;
  }
  /* Lumières */
  if (Objet == "lum") {
    if (Ordre == "on") {
      Cde += "lon";
    } else if (Ordre == "off") {
      Cde += "loff";
    }
//    Serial.print(F("Lumière: "));
//    Serial.println(Cde);
    Fait = true;
  }
  /* Ventilateur */
  if (Objet == "ventilo") {
    if(Cde == "") Cde = "http://192.168.000.000/";
    if (Ordre == "on") {
      Cde += "von";
    } else if (Ordre == "off") {
      Cde += "voff";
    }
//    Serial.print(F("Ventilo: "));
//    Serial.println(Cde);
    Fait = true;
  }
  /* Courrier */
  if (Objet == "lettre") {
    Cde = "http://192.168.000.000/URI";
//    Serial.print(F("Courrier: "));
//    Serial.println(Cde);
    Fait = true;
  }
  /* Adresse IP */
  if (Objet.startsWith("adresse")) {
    Cde = "";
//    Serial.print(F("Adresse IP: "));
//    Serial.println(Cde);
    AudioHost(MyIP);
    Fait = true;
  }
  /* Radios */
  if (Objet == "radio") {
    if (Ordre == "on") {
      if (Station == "") {
        AudioHost("voici la radio " + GetNameStation(NumStation));
        Delais = millis() + 8000;
        while(audio.isRunning()) {
          audio.loop();
          if(Delais < millis()) break;
        }
        RadioON = true;
        RadioOld = GetURLRadio(NumStation);
        AudioHost(RadioOld);
      } else {
        byte nStation = GetNumStation(Station);
        if(nStation != 0) {
          if (nStation != NumStation) {
            NumStation = nStation;
            SauveValeurDefaut();
          }
          AudioHost("voici la radio " + GetNameStation(NumStation));
          Delais = millis() + 8000;
          while(audio.isRunning()) {
            audio.loop();
            if(Delais < millis()) break;
          }
          RadioON = true;
          RadioOld = GetURLRadio(NumStation);
          AudioHost(RadioOld);
        } else {
          RadioON = false;
          AudioHost("Désolé, mais je n'ai pas les coordonnées de la radion " + Station);
        }
      }
    } else if (Ordre == "off") {
      RadioON = false;
      RadioOld = "";
      AudioState = false;
      audio.stopSong();
    }
    Fait = true;
  }
  /* Réglage du volume */
  if (Objet == "vol") {
    if (Ordre == "plus") {
      Volume = Volume + 2;
    }
    if (Ordre == "moins") {
      Volume = Volume - 2;
    }
    SetVolume(Volume);
    Fait = true;
  }
  /* Portail */
  if (Objet.startsWith("port")) {
    if (Ordre == "on") {
      if (Objet == "portillon") Cde += "ouvrevent";
      if (Objet == "portail") Cde += "ouvreport";
    } else if (Ordre == "off") {
      Cde += "fermeport";
    }
//    Serial.print(F("Portail: "));
//    Serial.println(Cde);
    Fait = true;
  }
  /* Date et heure */
  if (Objet == "heure" && nbElement == 1) {
    AudioHost("il est " + twoDigits(now.hour()) + ":" + twoDigits(now.minute()));
    Fait = true;
  }
  if (Objet == "date" && nbElement == 1) {
    AudioHost("nous sommes le " + DateDuJourLong());
    Fait = true;
  }
  /* Alarmes */
  if (Objet == "alarme") {                                                                            // IMPORTANT: dans cette partie de la fonction, Valeur[0) contient toujours l'heure, Valeur[1] toujours les minutes etc.
//    Serial.println();
//    Serial.print(F("Ordre: "));
//    Serial.println(Ordre);
//    Serial.print(F("Prepos: "));
//    Serial.println(Prepos);
//    Serial.println();
//    for(int i=0; i<3; i++) {
//      Serial.print(F("Unite["));
//      Serial.print(i);
//      Serial.print(F("]: "));
//      Serial.println(Unite[i]);
//      Serial.print(F("Valeur["));
//      Serial.print(i);
//      Serial.print(F("]: "));
//      Serial.println(Valeur[i]);
//    }
//    Serial.println();
//    Serial.print(F("Heure Actuelle: "));
//    Serial.println(HeureEnSeconde(now.hour(), now.minute(), now.second()));
//    Serial.println();
    if (Ordre == "on" || Ordre == "plus") {
      if (Prepos == "à" || Prepos == "pour") {                                                        // Pour mettre une alarme à une certaine heure (ex: mets une alarme à 10h15")
//        Serial.println(F("Alarme à une heure précise"));
        if (Valeur[0] != 0 && Unite[0].startsWith("heure")) {
          if (Valeur[1] != 0 && (Unite[1].startsWith("minute") || Unite[1] == "")) {                  // On donne des heures et des minutes (en précisant le mot minute ou pas)
            if (Valeur[2] == 0) Valeur[2] = now.second();                                             // Là c'est au choix, on met les secondes pour secondes sinon on met en REM
            AlarmeEnSeconde = HeureEnSeconde(Valeur[0], Valeur[1], Valeur[2]);                        // Déclenchement à la seconde près
            Alarme = true;
            EcrireAlarme();                                                                           // On sauvegarde l'alarme, en cas de reboot celle-ci court toujours (fichier lu au démarrage)
            AudioLocalVol("alarmeon", VolDef);                                                        // Peu importe le réglage du volume, la mise en service de l'alarme se fait assez fort (audible de loin)
            AlarmeEtape = true;
            Fait = true;
            Cde = "";
          }
        }
//        Serial.print(F("Alarme heure fixe: "));
//        Serial.println(AlarmeEnSeconde);
      } else {                                                                                        // Pour mettre une alarme dans un certain temps
//        Serial.println(F("Alarme après un certain temps"));
        if (Valeur[0] != 0 && Unite[0].startsWith("heure")) {                                         // On précise les heures et minutes à partir de l'heure courante
          if (Valeur[1] != 0 && (Unite[1].startsWith("minute"))) {
            if (Valeur[2] == 0) Valeur[2] = now.second();                                             // Là c'est au choix, on met les secondes pour secondes si elles n'ont pas été précisées, sinon on met en REM
          }
          AlarmeEnSeconde = HeureEnSeconde(now.hour(), now.minute(), now.second()) + HeureEnSeconde(Valeur[0], Valeur[1], Valeur[2]);
          Alarme = true;
          AlarmeEtape = true;
          EcrireAlarme();
          AudioLocalVol("alarmeon", VolDef);                                                          // Peu importe le réglage du volume, la mise en service de l'alarme se fait assez fort (audible de loin)
          Fait = true;
          Cde = "";
//          Serial.print(F("Alarme après HM: "));
//          Serial.println(AlarmeEnSeconde);
        } else if (Valeur[0] != 0 && Unite[0].startsWith("heure")) {                                  // Pour mettre une alarme dans un certain temps (ex: "mets une alarme dans 1h30s" sans précision des minutes)
          if (Valeur[1] != 0 && (Unite[1].startsWith("seconde"))) {
            Valeur[2] = Valeur[1];
            Valeur[1] = 0;
          }
          AlarmeEnSeconde = HeureEnSeconde(now.hour(), now.minute(), now.second()) + HeureEnSeconde(Valeur[0], Valeur[1], Valeur[2]);
          Alarme = true;
          AlarmeEtape = true;
          EcrireAlarme();
          AudioLocalVol("alarmeon", VolDef);                                                          // Peu importe le réglage du volume, la mise en service de l'alarme se fait assez fort (audible de loin)
          Fait = true;
          Cde = "";
//          Serial.print(F("Alarme après HS: "));
//          Serial.println(AlarmeEnSeconde);
        } else if (Valeur[0] != 0 && (Unite[0].startsWith("minute"))) {                               // On donne le temps en minutes et optionnellement en secondes
          Valeur[2] = Valeur[1];
          Valeur[1] = Valeur[0];
          Valeur[0] = 0;
          AlarmeEnSeconde = HeureEnSeconde(now.hour(), now.minute(), now.second()) + HeureEnSeconde(Valeur[0], Valeur[1], Valeur[2]);
          Alarme = true;
          AlarmeEtape = true;
          EcrireAlarme();
          AudioLocalVol("alarmeon", VolDef);                                                          // Peu importe le réglage du volume, la mise en service de l'alarme se fait assez fort (audible de loin)
          Cde = "";
//          Serial.print(F("Alarme après MS: "));
//          Serial.println(AlarmeEnSeconde);
        } else if (Valeur[0] != 0 && (Unite[0].startsWith("seconde"))) {                              // On donne le temps seulement en secondes
          Valeur[2] = Valeur[0];
          Valeur[1] = 0;
          Valeur[0] = 0;
          AlarmeEnSeconde = HeureEnSeconde(now.hour(), now.minute(), now.second()) + HeureEnSeconde(Valeur[0], Valeur[1], Valeur[2]);
          Alarme = true;
          AlarmeEtape = true;
          EcrireAlarme();
          AudioLocalVol("alarmeon", VolDef);                                                              // Peu importe le réglage du volume, la mise en service de l'alarme se fait assez fort (audible de loin)
          Cde = "";
//          Serial.print(F("Alarme après S: "));
//          Serial.println(AlarmeEnSeconde);
        }
      }
//      Serial.println();
    } else if (Ordre == "moins") {
      Cde = "";
      Alarme = false;
      SupprimeAlarme();
      audio.setVolume(Volume);
      AudioLocal("almsuppr");
    }
    Fait = true;
  }
  if (Cde != "") {
//    Serial.print(F("Commande à executer: "));
//    Serial.println(Cde);
    Execute(Cde);
  }
//  if (Fait == false) {
//    Serial.println(F("Fait = false"));
//    AudioLocalVol("erreur", VolDef);
//    Fait = true;
//  }
}

void Execute(String Cde) {
//  Serial.print(F("Commande à passer: "));
//  Serial.println(Cde);
  String Reponse;
  audio.setVolume(Volume);
  if (Cde.indexOf("http") >-1) { // Il s'agit d'une commande http
    Reponse = InterroHTTP(Cde);
    Reponse.trim();
    if(Reponse == "") {
      AudioLocal("desole");
//      Serial.println(F("Son désolé"));
    } else if (Reponse.indexOf("ok")>-1) {
      AudioLocal("ok");
//      Serial.println(F("Son OK"));
    } else {
      AudioHost(Reponse);
//      Serial.println(F("Son réponse"));
    }
  } else {
    // Choses à executer en interne ou en lancant une application (API)
  }
//  Serial.print(F("Réponse: "));
//  Serial.println(Reponse);
}

String InterroHTTP(String Site) {
  String Ligne = "";
  String URL = Site.substring(0, Site.indexOf("/", 9));
  String URI = Site.substring(Site.indexOf("/", 9));
  URL = URL.substring(URL.indexOf("//")+2);
  char Srv[URL.length()+1];
  URL.toCharArray(Srv, URL.length()+1);

//  Serial.print(F("Srv = "));
//  Serial.println(Srv);
//  Serial.print(F("URL = "));
//  Serial.println(URL);
//  Serial.print(F("URI = "));
//  Serial.println(URI);

  if (client.connect(Srv, 80)) {
    client.print(String("GET ") + URI + " HTTP/1.1\r\n" + "Host: " + Srv + "\r\nConnection: close\r\n\r\n");
    client.setTimeout(8000);
    // Lecture de la réponse
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Ligne += String(c);
//        Serial.print(c);
      }
    }
    client.stop();
  } else {
    Ligne = "Connexion impossible, le serveur ne répond pas!";
  }

  if (Ligne == "") {
    Ligne = "La connexion a réussie, mais le serveur n'a pas donné de réponse.";
  } else if (Ligne.startsWith("HTTP")) {
    Ligne.toLowerCase();
    Ligne.replace("\r", " ");
    Ligne.replace("\n", " ");
    if (Ligne.indexOf("close") > 0) {
      Ligne = MotPos(Ligne, "close", 2).substring(5);
    }
    Ligne.trim();
  }
//  Serial.println();
//  Serial.print(F("Ligne: "));
//  Serial.println(Ligne);
  return(Ligne);
}

void AudioLocal(String sFile) {
  unsigned long AudioTimeOut = millis() + 8000;
  if (AudioState == true) {
    while(audio.isRunning()) {
      if (RadioON == true) break;
      if (AudioTimeOut < millis()) break;
    }
  }
  if(!sFile.startsWith("/")) sFile = "/" + sFile;
  if(!sFile.endsWith(".mp3")) sFile += ".mp3";
  audio.stopSong();
  audio.connecttoFS(SPIFFS, sFile.c_str());
  AudioState = true;
  Attend(100);
//  Serial.print(F("Play : "));
//  Serial.println(sFile);
}

void AudioLocalVol(String sFile, int Vol) {
  unsigned long AudioTimeOut = millis() + 8000;
  if (AudioState == true) {
    while(audio.isRunning()) {
      if (RadioON == true) break;
      if (AudioTimeOut < millis()) break;
    }
  }
  if(!sFile.startsWith("/")) sFile = "/" + sFile;
  if(!sFile.endsWith(".mp3")) sFile += ".mp3";
  audio.setVolume(Vol);
  audio.stopSong();
  audio.connecttoFS(SPIFFS, sFile.c_str());
  AudioState = true;
  Attend(100);
//  Serial.print(F("Play : "));
//  Serial.println(sFile);
}

void AudioHost(String URL) {
  if (!URL.startsWith("http")) URL = VoiceRss + URL;
  audio.stopSong();
  audio.connecttohost(URL.c_str());
  AudioState = true;
  Attend(100);
//  Serial.print(F("Play : "));
//  Serial.println(URL);
}

String GetNameStation(byte nSt) {
  String Lg = "";
  String St = "";
  char Car;
  byte N = 0;
  File file = SPIFFS.open("/radios.lst", "r");
  if (file) {
    for(int i=1; i<NbreRadios; i++) {
      Lg = file.readStringUntil('\n');
//      Serial.print(F("Lg= "));
//      Serial.println(Lg);
      Lg.replace("\r", "");
      Lg.replace("\n", "");
      N = MotPos(Lg, ";", 1).toInt();
      St = MotPos(Lg, ";", 2);
      St.toLowerCase();
//      Serial.print(F("Station trouvée: "));
//      Serial.println(St);
      if (nSt == N) {
        St = MotPos(Lg, ";", 2);
        break;
      }
    }
    file.close();
  }
  return St;
}

String GetURLRadio(byte nSt) {
  String Lg = "";
  byte N = 0;
  String St = "";
  String URLRadio = "";
  char Car;
  File file = SPIFFS.open("/radios.lst", "r");
  if (file) {
    for(int i=1; i<NbreRadios; i++) {
      Lg = file.readStringUntil('\n');
//      Serial.print(F("Lg= "));
//      Serial.println(Lg);
      Lg.replace("\r", "");
      Lg.replace("\n", "");
      N = MotPos(Lg, ";", 1).toInt();
      St = MotPos(Lg, ";", 2);
      St.toLowerCase();
//      Serial.print(F("Station trouvée: "));
//      Serial.println(St);
      if (nSt == N) {
        URLRadio = MotPos(Lg, ";", 3);
        break;
      }
    }
    file.close();
  }
  return URLRadio;
}

byte GetNumStation(String nSt) {
  String Lg = "";
  byte N = 0;
  String St = "";
  char Car = 0;
  File file = SPIFFS.open("/radios.lst", "r");
  if (file) {
    for(int i=1; i<NbreRadios; i++) {
      Lg = file.readStringUntil('\n');
//      Serial.print(F("Lg= "));
//      Serial.println(Lg);
      Lg.replace("\r", "");
      Lg.replace("\n", "");
      St = MotPos(Lg, ";", 2);
      St.toLowerCase();
//      Serial.print(F("Station trouvée: "));
//      Serial.println(St);
      if (nSt == St) {
        N = MotPos(Lg, ";", 1).toInt();
        break;
      }
    }
//    Serial.println(F("Fichier \"radios.lst\" fermé."));
    file.close();
  } else {
//    Serial.println(F("Fichier \"radios.lst\" impossible à ouvrir !"));
  }
  if ( N != 0) {
//    Serial.print(F("N° Trouvé: "));
//    Serial.println(N);
  }
  return N;
}

int GetMP3List(fs::FS &fs, const char *dirname, uint8_t levels, String mp3list[30], String mp3size[30]) {
//  Serial.printf("Listing directory: %s\n", dirname);
  int i = 0;

  File root = fs.open(dirname);
  if (!root) {
//    Serial.println("Failed to open directory");
    return i;
  }
  if (!root.isDirectory()) {
//    Serial.println("Not a directory");
    return i;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String temp = file.name();
      if (temp.endsWith(".wav")) {
        mp3list[i] = temp;
        mp3size[i] = file.size();
        i++;
      } else if (temp.endsWith(".mp3")) {
        mp3list[i] = temp;
        mp3size[i] = file.size();
        i++;
      }
    }
    file = root.openNextFile();
  }
  return i;
}

void SetVolume(int Valeur) {
  if (Valeur < 6) {
    Valeur = 6;
    LedVolume(Valeur);
    AudioLocal("volmin");
//    Serial.print(F("Volume: "));
//    Serial.println(Valeur);
    return;
  }
  if (Valeur > 21) {
    Valeur = 21;
    LedVolume(Valeur);
    AudioLocal("volmax");
//    Serial.print(F("Volume: "));
//    Serial.println(Valeur);
    return;
  }
  Volume = Valeur;
  audio.setVolume(Volume);
  LedVolume(Volume);
  VolReg = true;
//  Serial.print(F("SetVolume: "));
//  Serial.println(Valeur);
}

void SetVolumeWeb(int Valeur) {
  if (Valeur < 6) {
    Valeur = 6;
    LedVolume(Valeur);
    AudioLocalVol("volmin", VolDef);
//    Serial.print(F("Volume: "));
//    Serial.println(Valeur);
    return;
  }
  if (Valeur > 21) {
    Valeur = 21;
    LedVolume(Valeur);
    AudioLocalVol("volmax", VolDef);
//    Serial.print(F("Volume: "));
//    Serial.println(Valeur);
    return;
  }
  audio.setVolume(Valeur);
  LedVolume(Valeur);
//  Serial.print(F("SetVolume: "));
//  Serial.println(Valeur);
}

void LedVolume(int Valeur) {
  EraseReg();
  int V = (Valeur - 5) / 2;
//  Serial.print(F("V = "));
//  Serial.println(V);
  for (int i=0; i<8; i++) {
    if (i < V) {
      bgReg[i] = HIGH;
//      Serial.print(i);
    }
  }
//  Serial.println();
  WriteReg();
  LedMillis = millis() + 3000;
  VolMillis = LedMillis;
  LedReg = true;
}

void WriteReg() {
  digitalWrite(bgLatch, LOW);
  for (int i=7; i>=0; i--) {
    digitalWrite(bgClock, LOW);
    digitalWrite(bgData, bgReg[i]);
    digitalWrite(bgClock, HIGH);
  }
  digitalWrite(bgLatch, HIGH);
}

void MakeReg(int V) {
  EraseReg();
  for (int i=0; i<8; i++) {
    if (V & int(pow(2, i))) {
      bgReg[i] = HIGH;
    }
  }
  WriteReg();
}

void EraseReg() {
  for (int i=0; i<8; i++) {
    bgReg[i] = LOW;
  }
  WriteReg();
  if(Micro == false) {
    digitalWrite(ledMicro, HIGH);
  } else {
    digitalWrite(ledMicro, LOW);
  }
}

String TestLED(int ld) {
//    Serial.println(F("Début du test LED"));
  if (ld == -1) {
    digitalWrite(ledMicro, LOW);
    digitalWrite(ledEcoute, LOW);
    digitalWrite(ledWiFi, LOW);
    EraseReg();
    for (int i=0; i<8; i++) {
      if (i > 0) {
        bgReg[i-1] = LOW;
        WriteReg();
      }
      bgReg[i] = HIGH;
      WriteReg();
//      Serial.print(F("Test LED "));
//      Serial.println(i);
      Attend(500);
    }
    EraseReg();
//    Serial.println(F("Test LED Micro"));
    digitalWrite(ledMicro, HIGH);
    Attend(500);
//    Serial.println(F("Test LED Ecoute"));
    digitalWrite(ledMicro, LOW);
    digitalWrite(ledEcoute, HIGH);
    Attend(500);
//    Serial.println(F("Test LED Wifi"));
    digitalWrite(ledEcoute, LOW);
    digitalWrite(ledWiFi, HIGH);
    Attend(500);
    digitalWrite(ledWiFi, LOW);
//    Serial.println(F("Test LED terminé."));
    webString = F("Le test de toutes les LEDs a été effectué.");
  } else {
    webString = F("Le test marche/arrêt de la LED N°");
    webString += String(ld);
    if (ld < 8) {
      bgReg[ld] = !bgReg[ld];
      WriteReg();
      webString += F(" du volume sonore a été effectué.");
    } else {
      if (ld == 8) {
        digitalWrite(ledMicro, !digitalRead(ledMicro));
        webString += F(" Micro a été réalisé.");
      } else if (ld == 9) {
        digitalWrite(ledEcoute, !digitalRead(ledEcoute));
        webString += F(" Ecoute a été réalisé.");
      } else if (ld == 10) {
        digitalWrite(ledWiFi, !digitalRead(ledWiFi));
        webString += F(" WiFi a été réalisé.");
      }
//      Serial.println(webString);
    }
  }
  return(webString);
}

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-= TIME =-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-
   -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- Horloge Interne -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
   -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-
*/
String twoDigits(int digits) {                                                                                              // Fonction utilitaire pour l'affichage de l'horloge numérique: imprime en tête 0 si necessaire
  if (digits < 10) {
    String i = '0' + String(digits);
    return i;
  } else {
    return String(digits);
  }
}

bool isSummerTime (int an, byte mois, byte jour, byte heure, byte tzHours) {                                                // paramètres d'entrée: "heure normale" pour l'année, le mois, le jour, l'heure et les heures (0 = UTC, 1 = MEZ)
  if ((mois < 3) || (mois > 10)) return false;                                                                              // Pas d'heure d'été en janvier, février, novembre, décembre
  if ((mois > 3) && (mois < 10)) return true;                                                                               // Heure d'été en avril, mai, juin, juil, aout, sept
  if (mois == 3 && (mois + 24 * jour) >= (1 + tzHours + 24 * (31 - (5 * an / 4 + 4) % 7)) || mois == 10 && (heure + 24 * jour) < (1 + tzHours + 24 * (31 - (5 * an / 4 + 1) % 7))) {
    return true;
  } else {
    return false;
  }
}

time_t getNtpTime() {
  byte tzHours = TimeZone;
  time_t t;
  now = rtc.now();

  if (MaHenCours) return 0;
  MaHenCours = true;
  while (udp.parsePacket() > 0) ;                                                                                           // Supprimer tous les paquets précédemment reçus

  sendNTPpacket(TimeServer);

  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      udp.read(packetBuffer, NTP_PACKET_SIZE);                                                                              // Lire le paquet dans le tampon
      unsigned long secsSince1900;
      // Convertir quatre octets commençant à l'emplacement 40 en un entier long
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      t = secsSince1900 - 2208988800UL + tzHours * SECS_PER_HOUR;
//      Serial.print(F("Réponse server NTP: "));
//      Serial.println(t);
      if (hEteHiver == true) {
        if (isSummerTime(now.year(), now.month(), now.day(), now.hour(), TimeZone)) {
          tzHours = TimeZone + 1;
          t = secsSince1900 - 2208988800UL + tzHours * SECS_PER_HOUR;
        }
      }
//      Serial.println(F("--- Mise à l'heure effectuée ---"));
      rtc.adjust(t);
      HeureOk = true;
      MaHenCours = false;
      return t;
    }
  }
//  Serial.println(F("--- Mise à l'heure impossible ---"));
  MaHenCours = false;
  return 0;                                                                                                                 // Renvoie 0 si impossible d'obtenir l'heure
}

// Envoyer une requête au serveur NTP de temps à l'adresse indiquée
void sendNTPpacket(IPAddress &adresse) {
// Mettre tous les octets du tampon à 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
// Initialiser les valeurs nécessaires pour former la requête NTP
// (Voir l'URL ci-dessus pour plus de détails sur les paquets)
  packetBuffer[0] = 0b11100011;                                                                                             // LI, Version, Mode
  packetBuffer[1] = 0;                                                                                                      // Strate, ou type d'horloge
  packetBuffer[2] = 6;                                                                                                      // Intervalle d'interrogation
  packetBuffer[3] = 0xEC;                                                                                                   // Précision de l'horloge par les pairs
  // 8 octets de zéro pour Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // Tous les champs NTP ont reçu des valeurs, vous pouvez
  // maintenant envoyer un paquet demandant un horodatage :
  udp.beginPacket(adresse, 123);                                                                                            // Les requêtes NTP sont destinées au port 123 normalement.
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void DateTimeSet(uint8_t d, uint8_t m, uint16_t y, uint8_t h, uint8_t n, uint8_t s) {                                       // données pour mettre à l'heure l'horloge
  rtc.adjust(DateTime(y, m, d, h, n, s));                                                                                   // mise à l'heure de l'horloge
}

uint32_t HeureEnSeconde(byte H, byte M, byte S) {
  if(H >= 24) H -= 24;
  uint32_t T = H * 3600;
           T += M * 60;
           T += S;
  return(T);
}

String DateDuJourLong() {
  now = rtc.now();
  String DateDuJour = String(dayStr(now.dayOfTheWeek())) + " " + twoDigits(now.day()) + " " + String(monthStr(now.month())) + " " + String(now.year());
//  Serial.print(F("Date du jour: "));
//  Serial.println(DateDuJour);
  return(DateDuJour);
}

void LireAlarme() {
  if (SPIFFS.exists("/Alarme.txt")) {
    File file = SPIFFS.open("/Alarme.txt", "r");
    if (file) {
      AlarmeEnSeconde = file.read();
      file.close();
      Alarme = true;
    }
  }
}

void EcrireAlarme() {
  if (SPIFFS.exists("/Alarme.txt")) SPIFFS.remove("/Alarme.txt");
  if (AlarmeEnSeconde > 0) {
    File file = SPIFFS.open("/Alarme.txt", "w");
    if (file) {
      file.print(AlarmeEnSeconde);
      file.close();
    }
  }
}

void SupprimeAlarme() {
  if (SPIFFS.exists("/Alarme.txt")) SPIFFS.remove("/Alarme.txt");
}

void LireValeurDefaut() {
  String Chaine = "";
  if (SPIFFS.exists("/defaut.txt")) {
    File dataFile = SPIFFS.open("/defaut.txt", "r");                                                                        // Ouverture fichier pour le lire
    for (int i = 0; i < dataFile.size(); i++) {
      Chaine += (char)dataFile.read();                                                                                      // Lit le fichier
    }
    //    Serial.println(alm);
    dataFile.close();
    if (NbMots(Chaine, ";") != 5) {
      CreerValeurDefaut();
    } else {
      VolDef = MotPos(Chaine, ";", 1).toInt();
      Volume = MotPos(Chaine, ";", 2).toInt();
      NumStation = MotPos(Chaine, ";", 3).toInt();
      NbreRadios = MotPos(Chaine, ";", 4).toInt();
      NbreBornes = MotPos(Chaine, ";", 5).toInt();
    }
  } else {
    CreerValeurDefaut();
  }
}

void CreerValeurDefaut() {
  int idxStations = 0;
  int idxBornes = 0;
  char Car;
  String Ligne = "";
  bool Ajuster = false;

  if (SPIFFS.exists("/defaut.txt")) {
    File dataFile = SPIFFS.open("/defaut.txt", "r");                                                                        // Ouverture fichier pour le lire
    for (int i = 0; i < dataFile.size(); i++) {
      Ligne += (char)dataFile.read();                                                                                      // Lit le fichier
    }
    //    Serial.println(alm);
    dataFile.close();
    VolDef = MotPos(Ligne, ";", 1).toInt();
    Volume = MotPos(Ligne, ";", 2).toInt();
    NumStation = MotPos(Ligne, ";", 3).toInt();
    NbreRadios = MotPos(Ligne, ";", 4).toInt();
    NbreBornes = MotPos(Ligne, ";", 5).toInt();

    if (SPIFFS.exists("/radios.lst")) {
      File fic = SPIFFS.open("/radios.lst", "r");
      if (fic) {
        idxStations = 0;
        while (fic.available() > 0) {
          Ligne = "";
          Car = fic.read();
          while (Car != '\n') {
            Ligne += Car;
            Car = fic.read();
          }
          Ligne.trim();
          if (Ligne != "") idxStations++;
        }
        fic.close();
      }
    }
  }
  if (SPIFFS.exists("/bornes.lst")) {
    File fic = SPIFFS.open("/bornes.lst", "r");
    if (fic) {
      idxBornes = 0;
      while (fic.available() > 0) {
        Ligne = "";
        Car = fic.read();
        while (Car != '\n') {
          Ligne += Car;
          Car = fic.read();
        }
        Ligne.trim();
        if (Ligne != "") idxBornes++;
      }
      fic.close();
    }
  }
  if (idxStations != NbreRadios) {
    NbreRadios = idxStations;
    Ajuster = true;
  }
  if (idxBornes != NbreBornes) {
    NbreBornes = idxBornes;
    Ajuster = true;
  }
  if (Volume < 6 || Volume > 21) {
    Volume = 15;
    Ajuster = true;
  }
  if (NumStation < 1 || NumStation > NbreRadios) {
    NumStation = 1;
    Ajuster = true;
  }
  if (Ajuster == true) SauveValeurDefaut();
}

void SauveValeurDefaut() {
  String Ligne = String(VolDef) + ";" + String(Volume) + ";" + String(NumStation) + ";" + NbreRadios + ";" + NbreBornes;
  if (SPIFFS.exists("/defaut.txt")) {
    if (SPIFFS.exists("/defaut.old")) SPIFFS.remove("/defaut.old");
    SPIFFS.rename("/defaut.txt", "/defaut.old");
  }
  File dataFile = SPIFFS.open("/defaut.txt", "w");                                                                          // Ouverture fichier pour l'écriture
  dataFile.println(Ligne);
  dataFile.close();
}

bool LireConfig() {
  String Lg = "";
  String Key = "";
  String Val = "";
  char Car = 0;
  int Egale = 0;

  if (SPIFFS.exists("/config.ini")) {
    File myFile = SPIFFS.open("/config.ini", "r");
    if (myFile) {
      while(myFile.available() > 0) {
        Lg = "";
        Car = myFile.read();
        while(Car != '\n') {
          Lg += Car;
          Car = myFile.read();
        }
        Egale = Lg.indexOf('=');
        if (Egale > 0) {
          Key = Lg.substring(0, Egale);
          Val = Lg.substring(Egale + 1);
          Key.trim();
          Val.trim();
          if(Key == "Name") {
            MyName = Val;
          } else if(Key == "SSID1") {
            ssid1 = Val;
          } else if(Key == "Pass1") {
            pass1 = Val;
          } else if(Key == "SSID2") {
            ssid2 = Val;
          } else if(Key == "Pass2") {
            pass2 = Val;
          } else if(Key == "SSID3") {
            ssid3 = Val;
          } else if(Key == "Pass3") {
            pass3 = Val;
          } else if(Key == "SSID4") {
            ssid4 = Val;
          } else if(Key == "Pass4") {
            pass4 = Val;
          } else if(Key == "ipAdresse") {
            ipAdresse = StrToIP(Val);
            MyIP = Val;
          } else if(Key == "Gateway") {
            gateway = StrToIP(Val);
          } else if(Key == "Subnet") {
            subnet = StrToIP(Val);
          } else if(Key == "DNS") {
            dns = StrToIP(Val);
          } else if(Key == "FTPuser") {
            FTPuser = Val;
          } else if(Key == "FTPpass") {
            FTPpass = Val;
          } else if(Key == "OTApass") {
            OTApass = Val;
          } else if(Key == "User_WEB") {
            USR_WEB[Val.length()+1];
            Val.toCharArray(USR_WEB, Val.length()+1);
          } else if(Key == "Pass_WEB") {
            PSS_WEB[Val.length()+1];
            Val.toCharArray(PSS_WEB, Val.length()+1);
          } else if(Key == "TimeServer") {
            TimeServer = StrToIP(Val);
          } else if(Key == "TimePort") {
            TimePort = Val.toInt();
          } else if(Key == "TimeZone") {
            TimeZone = Val.toInt();
          } else if(Key == "hEteHiver") {
            hEteHiver = Val == "1" ? true : false;
          } else if(Key == "EteHiver") {
            EteHiver = Val == "1" ? true : false;
          } else if(Key == "Log") {
            Log = Val == "1" ? true : false;
          } else if(Key == "UID_Nano") {
            UID_Nano = Val;
          } else if(Key == "VoiceRSS") {
            VoiceRss = Val;
          } else if(Key == "Access_Key") {
            Access_Key = Val;
          } else if(Key == "HandShakeTimeout") {
            HandShakeTimeout = Val.toInt();
          } else if(Key == "TempOffset") {
            TempOffSet = Val.toFloat();
          } else if(Key == "TestLEDStart") {
            TestLEDStart = Val == "1" ? true : false;
//          } else if(Key == "voldef") {
//            VolDef = Val.toInt();
//          } else if(Key == "volume") {
//            Volume = Val.toInt();
          }
        }
      }
      myFile.close();
      return(true);
    } else {
      return(false);
    }
  } else {
    SauveConfig();
    SauveValeurDefaut();
    SauveConfigLog();
    return(true);
  }
}

bool SauveConfig() {
  String Val = "";
  if (SPIFFS.exists("/config.ini")) SPIFFS.remove("/config.ini");
  File myFile = SPIFFS.open("/config.ini", "w");                                                                               // ouvre le fichier en écriture NB : le fichier est créé si il n'existe pas !
  if (myFile) {
    myFile.print(F("Name="));
    myFile.println(MyName);
    myFile.print(F("SSID1="));
    myFile.println(ssid1);
    myFile.print(F("Pass1="));
    myFile.println(pass1);
    myFile.print(F("SSID2="));
    myFile.println(ssid2);
    myFile.print(F("Pass2="));
    myFile.println(pass2);
    myFile.print(F("SSID3="));
    myFile.println(ssid3);
    myFile.print(F("Pass3="));
    myFile.println(pass3);
    myFile.print(F("SSID4="));
    myFile.println(ssid4);
    myFile.print(F("Pass4="));
    myFile.println(pass4);
    myFile.print(F("ipAdresse="));
    myFile.println(MyIP);
    myFile.print(F("Gateway="));
    myFile.println(IPtoString(gateway));
    myFile.print(F("Subnet="));
    myFile.println(IPtoString(subnet));
    myFile.print(F("DNS="));
    myFile.println(IPtoString(dns));
    myFile.print(F("FTPuser="));
    myFile.println(FTPuser);
    myFile.print(F("FTPpass="));
    myFile.println(FTPpass);
    myFile.print(F("OTApass="));
    myFile.println(OTApass);
    myFile.print(F("User_WEB="));
    myFile.println(USR_WEB);
    myFile.print(F("Pass_WEB="));
    myFile.println(PSS_WEB);
    myFile.print(F("TimeServer="));
    myFile.println(IPtoString(TimeServer));
    myFile.print(F("TimePort="));
    myFile.println(TimePort);
    myFile.print(F("TimeZone="));
    myFile.println(TimeZone);
    Val = hEteHiver == true ? "1" : "0";
    myFile.print(F("hEteHiver="));
    myFile.println(Val);
    Val = EteHiver == true ? "1" : "0";
    myFile.print(F("EteHiver="));
    myFile.println(Val);
    myFile.print(F("UID_Nano="));
    myFile.println(UID_Nano);
    myFile.print(F("VoiceRSS="));
    myFile.println(VoiceRss);
    myFile.print(F("Access_Key="));
    myFile.println(Access_Key);
    myFile.print(F("HandShakeTimeout="));
    myFile.println(HandShakeTimeout);
    myFile.print(F("TempOffset="));
    myFile.println(TempOffSet);
    myFile.print(F("TestLEDStart="));
    Val = TestLEDStart == true ? "1" : "0";
    myFile.println(Val);
//    myFile.print(F("VolDef="));
//    myFile.println(VolDef);
//    myFile.print(F("Volume="));
//    myFile.println(Volume);

    myFile.println();
    myFile.print(F("Sauvegarde="));
    myFile.println(String(dayShortStr(now.dayOfTheWeek())) + " " + twoDigits(now.day()) + " " + String(monthShortStr(now.month())) + " " + String(now.year()) + " " + twoDigits(now.hour()) + ":" + twoDigits(now.minute()) + ":" + twoDigits(now.second()));
    myFile.close();
    return(true);
  } else {
    return(false);
  }
}

void SauveConfigLog() {
  DateTime now = rtc.now();

  if(HeureOk == true) {                                                                                                     // On prend le mois en cours si l'heure a été mise à jour, sinon LogMois = le mois précédemment enregistré dans le fichier qui est lu au démarrage.
    LogMois = now.month();
  }
  String bLog = Log == true ? "1" : "0";
  if (SPIFFS.exists("/configlog.ini")) SPIFFS.remove("/configlog.ini");
  File logFile = SPIFFS.open("/configlog.ini", "w");
  if (logFile) {
    logFile.print(F("Log="));
    logFile.println(bLog);
    logFile.print(F("LogMois="));
    logFile.println(LogMois);

    logFile.println();
    logFile.print(F("Sauvegarde="));
    logFile.println(String(dayShortStr(now.dayOfTheWeek())) + " " + twoDigits(now.day()) + " " + String(monthShortStr(now.month())) + " " + String(now.year()) + " " + twoDigits(now.hour()) + ":" + twoDigits(now.minute()) + ":" + twoDigits(now.second()));
    logFile.close();
  }
}

void LireConfigLog() {
  DateTime now = rtc.now();

  /* Taille du buffer */
  const byte BUFFER_SIZE = 80;
  /* Déclare le buffer qui stockera une ligne du fichier, ainsi que les deux pointeurs key et value */
  char buffer[BUFFER_SIZE], *key, *valeur;
  String Val = "";
  int car = 0;

  /* Déclare l'itérateur et le compteur de lignes */
  byte i, buffer_lenght, line_counter = 0;

  if (SPIFFS.exists("/configlog.ini")) {
    File logFile = SPIFFS.open("/configlog.ini", "r");
    if (logFile) {
      while (logFile.available() > 0) {
        /* Récupère une ligne entière dans le buffer */
        i = 0;
        while ((buffer[i++] = logFile.read()) != '\n') {
          /* Si la ligne dépasse la taille du buffer */
          if (i == BUFFER_SIZE) {
            /* On finit de lire la ligne mais sans stocker les données */
            car = 0;
            while (logFile.read() != '\n') {
              car++;
              if (car > BUFFER_SIZE) break;
            }
            break; // Et on arrête la lecture de cette ligne
          }
        }

        /* On garde de côté le nombre de char stocké dans le buffer */
        buffer_lenght = i;

        /* Finalise la chaine de caractéres ASCIIZ en supprimant le \n au passage */
        buffer[--i] = '\0';

        /* Incrémente le compteur de lignes */
        ++line_counter;

        /* Ignore les lignes vides ou les lignes de commentaires */
        if (buffer[0] == '\0' || buffer[0] == '#') continue;

        /* Cherche l'emplacement de la clef en ignorant les espaces et les tabulations en début de ligne */
        i = 0;
        while (buffer[i] == ' ' || buffer[i] == '\t') {
          if (++i == buffer_lenght) break; // Ignore les lignes contenant uniquement des espaces et/ou des tabulations
        }
        if (i == buffer_lenght) continue; // Gère les lignes contenant uniquement des espaces et/ou des tabulations
        key = &buffer[i];

        /* Cherche l'emplacement du séparateur = en ignorant les espaces et les tabulations après la clef */
        while (buffer[i] != '=') {

          /* Ignore les espaces et les tabulations */
          if (buffer[i] == ' ' || buffer[i] == '\t') buffer[i] = '\0';

          if (++i == buffer_lenght) {
            // Ligne mal formée
            break; // Ignore les lignes mal formées
          }
        }
        if (i == buffer_lenght) continue; // Gère les lignes mal formées

        /* Transforme le séparateur = en \0 et continue */
        buffer[i++] = '\0';

        /* Cherche l'emplacement de la valeur en ignorant les espaces et les tabulations après le séparateur */
        while (buffer[i] == ' ' || buffer[i] == '\t') {
          if (++i == buffer_lenght) {
            break; // Ignore les lignes mal formées
          }
        }
        if (i == buffer_lenght) continue; // Gère les lignes mal formées
        valeur = &buffer[i];

//        Serial.print(key);
//        Serial.print(F(" = "));
//        Serial.println(valeur);

        /* Transforme les données texte en valeur utilisable */
        /************** Morceaux de code à adapter à l'application **************/
        if (strcmp(key, "Log") == 0) {
          Val = valeur;
          Log = Val.toInt() == 1 ? true : false;
        } else if (strcmp(key, "LogMois") == 0) {
          Val = valeur;
          LogMois = Val.toInt();
          if (LogMois != now.month()) {
            logFile.close();
            LogMois = now.month();
            SauveConfigLog();
            break;
          }
        }
      }
      logFile.close();
    }
  } else {
    LogMois = now.month();
  }
}

// Fonctions necessaire aux pages web
bool returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
  return false;
}

void handleNotFound() {
  if (!loadFromSPIFFS(server.uri())) {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
  }
}

bool loadFromSPIFFS(String path) {
  String dataType = "text/plain";
  //  Serial.print(F("HTTP - Ouverture: "));
  //  Serial.println(path);
  //if(path.endsWith("/")) path += "index.htm";
  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".html")) dataType = "text/html";
  else if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js"))  dataType = "application/javascript";
  else if (path.endsWith(".xml")) dataType = "application/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";
  else if (path.endsWith(".json")) dataType = "application/json";
  else if (path.endsWith(".ini")) dataType = "text/plain";
  else if (path.endsWith(".log")) dataType = "text/plain";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".webp")) dataType = "image/webp";
  else if (path.endsWith(".ino")) dataType = "text/plain";
  else if (path.endsWith(".wav")) dataType = "audio/x-wav";
  else if (path.endsWith(".ttf")) dataType = "font/ttf";

  File dataFile = SPIFFS.open(path.c_str(), "r");

  if (!dataFile) return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    webString  = F("HTTP: Fichier ");
    webString += path;
    webString += F(" envoi incomplet");
    //    Serial.println("A envoyé moins de données que prévu!");
  }

  dataFile.close();
  return true;
}

bool handleSave() {
  if (server.args() == 0) return returnFail(F("BAD ARGS"));                                                                 // Au moins un argument doit être fourni
  String argName = "";
  String argValue = "";

  for (uint8_t i=0; i<server.args(); i++){
    argName = server.argName(i);
    argValue = server.arg(i);
    argValue.trim();
//    Serial.print(argName);
//    Serial.print(F(" = "));
//    Serial.println(argValue);

    if (argName == "ipfix") {
      MyIP = argValue;
      ipAdresse = StrToIP(argValue);
    }
    if (argName == "nom") {
      MyName = SysDomo + "_" + argValue;
    }
    if (argName == "ipgw") {
      gateway = StrToIP(argValue);
    }
    if (argName == "ipdns") {
      dns = StrToIP(argValue);
    }
    if (argName == "subnet") {
      subnet = StrToIP(argValue);
    }
    if (argName == "ipntp") {
      TimeServer = StrToIP(argValue);
    }
    if (argName == "tport") {
      TimePort = argValue.toInt();
    }
    if (argName == "tzone") {
      TimeZone = argValue.toInt();
    }
    if (argName == "webuser") {
      USR_WEB[argValue.length()+1];
      argValue.toCharArray(USR_WEB, argValue.length()+1);
    }
    if (argName == "webpass") {
      PSS_WEB[argValue.length()+1];
      argValue.toCharArray(PSS_WEB, argValue.length()+1);
    }
    if (argName == "ssid1") {
      ssid1 = argValue;
    }
    if (argName == "pass1") {
      pass1 = argValue;
    }
    if (argName == "ssid2") {
      ssid2 = argValue;
    }
    if (argName == "pass2") {
      pass2 = argValue;
    }
    if (argName == "ssid3") {
      ssid3 = argValue;
    }
    if (argName == "pass3") {
      pass3 = argValue;
    }
    if (argName == "ssid4") {
      ssid4 = argValue;
    }
    if (argName == "pass4") {
      pass4 = argValue;
    }
    if (argName == "uidnano") {
      UID_Nano = argValue;
    }
    if (argName == "vrss") {
      VoiceRss = argValue;
    }
    if (argName == "acckey") {
      Access_Key = argValue;
    }
    if (argName == "hetehiver") {
      hEteHiver = argValue.toInt() == 1 ? true : false;
    }
    if (argName == "etehiver") {
      EteHiver = argValue.toInt() == 1 ? true : false;
    }
    if (argName == "log") {
      Log = argValue.toInt() == 1 ? true : false;
    }
    if (argName == "tajst") {
      TempOffSet = argValue.toFloat();
    }
    if (argName == "ftpuser") {
      FTPuser = argValue;
    }
    if (argName == "ftppass") {
      FTPpass = argValue;
    }
    if (argName == "otapass") {
      OTApass = argValue;
    }
    if (argName == "hst") {
      HandShakeTimeout = argValue.toInt();
    }
    if (argName == "testled") {
      TestLEDStart = argValue.toInt() == 1 ? true : false;
    }
    if (argName == "voldef") {
      VolDef = argValue.toInt();
    }
    if (argName == "volume") {
      Volume = argValue.toInt();
    }
  }
  SauveConfig();
  SauveValeurDefaut();
  SauveConfigLog();
  webString = F("La sauvegarde a été effectuée.\nCes nouvelles données sont dès à présent prisent en compte.\r\n\r\n");
  server.send(200, "text/html", webString);
  return true;
}

void handleAjaxParam() {
  String bHEH = hEteHiver == true ? "1" : "0";                                                                              // Heure été / hiver
  String bEH  = EteHiver == true ? "1" : "0";                                                                               // Garder heure été ou hiver si pas de changement
  String bLOG = Log == true ? "1" : "0";
  String tLED = TestLEDStart == true ? "1" : "0";
  String bNom = MotPos(MyName, "_", 2);
  MySSID = WiFi.SSID();
  MyIP = String(ipAdresse[0]) + "." + String(ipAdresse[1]) + "." + String(ipAdresse[2]) + "." + String(ipAdresse[3]);

  webString = MyIP + "," + MyName + "," + IPtoString(gateway) + "," + IPtoString(dns) + "," + IPtoString(subnet) + "," + IPtoString(TimeServer) + ",";
  webString += String(TimePort) + "," + String(TimeZone) + "," + String(USR_WEB) + "," + String(PSS_WEB) + ",";
  webString += ssid1 + "," + pass1 + "," + ssid2 + "," + pass2 + "," + ssid3 + "," + pass3 + "," + ssid4 + "," + pass4 + ",";
  webString += VoiceRss + "," + UID_Nano + "," + Access_Key + "," + String(TempOffSet) + "," + bHEH + "," + bEH + ",";
  webString += FTPuser + "," + FTPpass + "," + OTApass + "," + String(HandShakeTimeout) + "," + bLOG + ",";
  webString += MyIP + " - " + MAC + " - Ver. " + String(Version) + "," + MySSID + " - " + bNom + "," + tLED + ",";
  webString += String(VolDef) + "," + String(Volume);
  server.send(200, "text/html", webString);
}

bool handleAction() {
  if (server.args() == 0) return returnFail(F("BAD ARGS"));                                                                 // Au moins un argument doit être fourni
  String argName = "";
  String argValue = "";

  for (uint8_t i=0; i<server.args(); i++){
    argName = server.argName(i);
    argValue = server.arg(i);
    argValue.trim();
//    Serial.print(argName);
//    Serial.print(F(" = "));
//    Serial.println(argValue);

    if (argName == "exec") {
//      Serial.print(F("Analyse WEB: "));
//      Serial.println(argValue);
      Analyser(argValue);
      webString = F("La commande a été effectuée.\r\n\r\n");
    } else {
      webString = F("Désolé, mais la commande n'a pas été effectuée.");
    }
  }
  server.send(200, "text/html", webString);
  return true;
}

void handleFichier(char *Fichier) {
  webString = F("<!DOCTYPE html><html><head>\n");
  webString += F("<meta http-equiv=\"Content-Type\" content=\"application/xhtml+xml; charset=UTF-8\"/>\n");
  webString += F("<meta http-equiv=\"Content-language\" content=\"fr\"/>\n");
  webString += F("<meta http-equiv=\"Pragma\" content=\"no-cache\"/>\n");
  webString += F("<meta http-equiv=\"Cache-Control\" content=\"no-cache\"/>\n");
  webString += F("<meta http-equiv=\"Content-Style-Type\" content=\"text/css\"/>\n");
  webString += F("<meta http-equiv=\"Content-Script-Type\" content=\"text/javascript\"/>\n");
  webString += F("<meta name=\"Description\" content=\"Borne ");
  webString += SysDomo;
  webString += F(": ");
  webString += String(Fichier).substring(1);
  webString += F("\"/>\n");
  webString += F("<meta name=\"Author\" content=\"Jean-Luc NAPOLITANO\"/>\n");
  webString += F("<link rel=\"icon\" type=\"image/vnd.microsoft.icon\" href=\"/favicon.ico\"/>\n");
  webString += F("<link rel=\"icon\" type=\"image/x-icon\" href=\"/favicon.ico\"/>\n");
  webString += F("<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon.ico\"/>\n");
  webString += F("<link rel=\"icon\" type=\"image/png\" href=\"/favicon.png\"/>\n");
  webString += F("<link rel=\"apple-touch-icon\" href=\"/favicon.png\"/>\n");
  webString += F("<title>Borne ");
  webString += SysDomo;
  webString += F("</title>\n");
  webString += F("</head>\n");
  webString += F("<body style='text-align:center;'>\n");
  webString += F("<strong><font size='14px' color='blue'>Fichier: ");
  webString += String(Fichier).substring(1);
  webString += F("</font></strong><br/><br/>\n");
  webString += F("<div id='contenu' style='text-align:center;display:inline-block;magin:auto;'><p style='text-align:left;'>\n");
  server.sendContent(webString);
  webString = F("<strong><u>Contenu du fichier:</u></strong><br/><br/>");
  server.sendContent(webString);
  if (SPIFFS.exists(Fichier)) {
    File fic = SPIFFS.open(Fichier, "r");
    if (fic) {
      while (fic.available() > 0) {
        webString = "";
        char Car = fic.read();
        while (Car != '\n') {
          webString += Car;
          Car = fic.read();
        }
        webString += "<br/>";
        server.sendContent(webString);
      }
    }
    fic.close();
  } else {
    webString += F("Fichier ");
    webString += String(Fichier);
    webString += F(" introuvable !<br/>");
    server.sendContent(webString);
  }
  webString = "";
  webString += F("</p></div>\n");
  webString += F("<br/><br/>");
  webString += F("</body>\n");
  webString += F("</html>\n");
  server.sendContent(webString);
}

void handleRadios() {
  String num = "";
  String nom = "";
  String lien = "";
  String Ligne = "";
  if (SPIFFS.exists("/radios.lst")) {
    File fic = SPIFFS.open("/radios.lst", "r");
    if (fic) {
      webString = String(NbreRadios) + "*<h2 id='nbr'>" + String(NbreRadios) + " Stations</h2><table id=\"lstradios\" align='center'>\n";
      server.sendContent(webString);
      while (fic.available() > 0) {
        Ligne = "";
        char Car = fic.read();
        while (Car != '\n') {
          Ligne += Car;
          Car = fic.read();
        }
        num = MotPos(Ligne, ";", 1);
        nom = MotPos(Ligne, ";", 2);
        lien = MotPos(Ligne, ";", 3);
        webString = F("<tr><td id='num");
        webString += num;
        webString += F("'>");
        webString += num;
        webString += F("</td><td id='nom");
        webString += num;
        webString +=F("'>");
        webString += nom;
        webString += F("</td><td id='lien");
        webString += num;
        webString += F("'>");
        webString += lien;
        webString += F("</td><td id='web");
        webString += num;
        webString += F("' style=\"display:none;\"><audio id=\"radio");
        webString += num;
        webString += F("\" preload=\"none\" src=\"");
        webString += lien;
        webString += F("\"/>");
        webString += F("</td><td id=\"md");
        webString += num;
        webString += F("\">");
        if(num.toInt() == 1) {
          webString += F("<img src=\"/haut_gris.png\"/>");
        } else {
          webString += F("<img src=\"/haut.png\" onclick=\"javascript:Monter(");
          webString += num;
          webString += F(");\" title=\"Monter\"/>");
        }
        webString += F("&nbsp;");
        if(num.toInt() == NbreRadios) {
          webString += F("<img src=\"/bas_gris.png\"/>");
        } else {
          webString += F("<img src=\"/bas.png\" onclick=\"javascript:Descendre(");
          webString += num;
          webString += F(");\" title=\"Descendre\"/>");
        }
        webString += F("</td><td id='joue");
        webString += num;
        webString += F("'><img src=\"/droitev.png\" onclick=\"javascript:Jouer('");
        webString += nom + "','" + lien + "',this.src";
        webString += F(");this.src=ChangeImage(this.src);\" title=\"Jouer/Arrêter sur la borne.\"/></td>");
        webString += F("<td id='joueweb");
        webString += num;
        webString += F("'><img src=\"/droiter.png\" onclick=\"javascript:JouerWeb(");
        webString += num;
        webString += F(");\" title=\"Jouer depuis le navigateur.\"/></td>");
        webString += F("<td><img src=\"/edit.png\" onclick=\"javascript:Editer(");
        webString += num;
        webString += F(");\" title=\"Editer\"/>&nbsp;<img src=\"/corbeille.png\" onclick=\"javascript:Supprimer(");
        webString += num;
        webString += F(");\" title=\"Supprimer définitivement!\"/></td></tr>\n");
        server.sendContent(webString);
//        Serial.print(webString);
      }
      fic.close();
      webString = "</table>\n";
      webString += "*" + MyIP + " - " + MAC + " - Ver. " + String(Version) + "*" + MySSID + " - " + MotPos(MyName, "_", 2) + "*" + MotPos(MyName, "_", 1);
      server.sendContent(webString);
    } else {
      webString = F("Impossible d'ouvrir le fichier \"radios.lst\".");
    }
    server.sendContent(webString);
  } else {
    webString = F("Fichier \"radios.lst\" introuvable !<br/>");
    server.sendContent(webString);
  }
  server.sendHeader("Connection", "close");
}

bool handleSauveRadios() {
  if (server.args() == 0) {
    return returnFail(F("BAD ARGS"));                                                                 // Au moins un argument doit être fourni
  }
  String argName = "";
  String argValue = "";
  String Ligne = "";
  String nomStation = "";
  String lienStation = "";
  int idx = 0;


  if (SPIFFS.exists("/radios.old")) SPIFFS.remove("/radios.old");
  if (SPIFFS.exists("/radios.lst")) SPIFFS.rename("/radios.lst", "/radios.old");
  File fic = SPIFFS.open("/radios.lst", "w");
  if (fic) {
    for (uint8_t i=0; i<server.args(); i++) {
      argName = server.argName(i);
      argValue = server.arg(i);
      argValue.trim();
//      Serial.print(argName);
//      Serial.print(F(" = "));
//      Serial.println(argValue);

      if (argName == "nb") {
        NbreRadios = argValue.toInt();
      } else if (argName.startsWith("nom")) {
        nomStation = argValue;
      } else if (argName.startsWith("lien")) {
        lienStation = argValue;
      }
      if (nomStation != "" && lienStation != "") {
        idx++;
        Ligne = String(idx) + ";" + nomStation + ";" + lienStation;
        fic.println(Ligne);
//        Serial.println(Ligne);
        nomStation = "";
        lienStation="";
      }
    }
    fic.close();
    SauveValeurDefaut();
    webString = ("Sauvegarde effectuée sur toutes les bornes.\n");
    webString += MajBornes("/radios.lst");
    server.send(200, "text/plain", webString);
  } else {
    webString = F("Un problème est survenu, les données n'ont pas été sauvegardée.");
    server.send(200, "text/plain", webString);
  }
  return true;
}

bool handleJouerRadio() {
  if (server.args() == 0) {
    return returnFail(F("BAD ARGS"));                                                                 // Au moins un argument doit être fourni
  }
  int numStation = 0;
  String nomStation = "";
  String lienStation = "";
  String argName = "";
  String argValue = "";

  for (uint8_t i=0; i<server.args(); i++) {
    argName = server.argName(i);
    argValue = server.arg(i);
    argValue.trim();
//    Serial.print(argName);
//    Serial.print(F(" = "));
//    Serial.println(argValue);

    if (argName == "nom") {
      nomStation = argValue;
    } else if (argName == "lien") {
//      argValue.replace("£", "&");
//      argValue.replace("%", "=");
      lienStation = argValue;
    } else if (argName == "vol") {
      if(Volume != argValue.toInt()) {
        Volume = argValue.toInt();
        SetVolume(Volume);
//        Serial.print(F("Volume : "));
//        Serial.println(Volume);
      }
    }
  }
  byte nStation = GetNumStation(nomStation);
  if(nStation != 0) {
    NumStation = nStation;
  }
  AudioHost("voici la radio " + nomStation);
  unsigned long Delais = millis() + 8000;
  while(audio.isRunning()) {
    audio.loop();
    if(Delais < millis()) break;
  }
  RadioON = true;
  RadioOld = lienStation;
  AudioHost(RadioOld);
  if (AudioState == true) {
    webString = F("La station ");
    webString += nomStation;
    webString += F(" est en cours de lecture.\r\n\r\n");
  } else {
    webString = F("Une erreur s'est produite, vérifiez que le lien est correcte!\r\n\r\n");
  }
  server.send(200, "text/html", webString);
  server.sendHeader("Connection", "close");
  return(true);
}

void handleBornes() {
  String num = "";
  String nom = "";
  String lien = "";
  String Ligne = "";
  if (SPIFFS.exists("/bornes.lst")) {
//    Serial.println(F("Fichier bornes.lst trouvé"));
    File fic = SPIFFS.open("/bornes.lst", "r");
    if (fic) {
      webString = String(NbreBornes) + "*<h2 id='nbr'>" + String(NbreBornes) + " Bornes</h2><table id=\"lstbornes\" align='center'>\n";
      server.sendContent(webString);
      while (fic.available() > 0) {
        Ligne = "";
        char Car = fic.read();
        while (Car != '\n') {
          Ligne += Car;
          Car = fic.read();
        }
        num = MotPos(Ligne, ";", 1);
        nom = MotPos(Ligne, ";", 2);
        lien = MotPos(Ligne, ";", 3);
        webString = F("<tr><td id='num");
        webString += num;
        webString += F("'>");
        webString += num;
        webString += F("</td><td id='nom");
        webString += num;
        webString +=F("'>");
        webString += nom;
        webString += F("</td><td id='lien");
        webString += num;
        webString += F("'>");
        webString += lien;
        webString += F("</td><td id=\"md");
        webString += num;
        webString += F("\">");
        if(num.toInt() == 1) {
          webString += F("<img src=\"/haut_gris.png\"/>");
        } else {
          webString += F("<img src=\"/haut.png\" onclick=\"javascript:Monter(");
          webString += num;
          webString += F(");\" title=\"Monter\"/>");
        }
        webString += F("&nbsp;");
        if(num.toInt() == NbreBornes) {
          webString += F("<img src=\"/bas_gris.png\"/>");
        } else {
          webString += F("<img src=\"/bas.png\" onclick=\"javascript:Descendre(");
          webString += num;
          webString += F(");\" title=\"Descendre\"/>");
        }
        webString += F("<td><img src=\"/edit.png\" onclick=\"javascript:Editer(");
        webString += num;
        webString += F(");\" title=\"Editer\"/>&nbsp;<img src=\"/corbeille.png\" onclick=\"javascript:Supprimer(");
        webString += num;
        webString += F(");\" title=\"Supprimer définitivement!\"/></td></tr>\n");
        server.sendContent(webString);
//        Serial.println(webString);
      }
      fic.close();
      webString = "</table>\n";
      webString += "*" + MyIP + " - " + MAC + " - Ver. " + String(Version) + "*" + MySSID + " - " + MotPos(MyName, "_", 2) + "*" + MotPos(MyName, "_", 1);
      server.sendContent(webString);
//      Serial.println();
//      Serial.println(F("Données envoyées."));
    } else {
      webString = F("Impossible d'ouvrir le fichier \"bornes.lst\".");
      server.sendContent(webString);
//      Serial.println(webString);
    }
  } else {
    webString = F("Fichier \"bornes.lst\" introuvable !<br/>");
    server.sendContent(webString);
//    Serial.println(webString);
  }
  server.sendHeader("Connection", "close");
}

bool handleSauveBornes() {
  if (server.args() == 0) {
    return returnFail(F("BAD ARGS"));                                                                 // Au moins un argument doit être fourni
  }
  String argName = "";
  String argValue = "";
  String Ligne = "";
  String nomBorne = "";
  String lienBorne = "";
  int idx = 0;

  if (SPIFFS.exists("/bornes.old")) SPIFFS.remove("/bornes.old");
  if (SPIFFS.exists("/bornes.lst")) SPIFFS.rename("/bornes.lst", "/bornes.old");
  File fic = SPIFFS.open("/bornes.lst", "w");
  if (fic) {
    for (uint8_t i=0; i<server.args(); i++) {
      argName = server.argName(i);
      argValue = server.arg(i);
      argValue.trim();
//      Serial.print(argName);
//      Serial.print(F(" = "));
//      Serial.println(argValue);

      if (argName == "nb") {
        NbreBornes = argValue.toInt();
      } else if (argName.startsWith("nom")) {
        nomBorne = argValue;
      } else if (argName.startsWith("lien")) {
        lienBorne = argValue;
      }
      if (nomBorne != "" && lienBorne != "") {
        idx++;
        Ligne = String(idx) + ";" + nomBorne + ";" + lienBorne;
        fic.println(Ligne);
//        Serial.println(Ligne);
        nomBorne = "";
        lienBorne="";
      }
    }
    fic.close();
    SauveValeurDefaut();
    webString = ("Sauvegarde effectuée sur toutes les bornes.\n");
    webString += MajBornes("/bornes.lst");
    server.send(200, "text/plain", webString);
  } else {
    webString = F("Un problème est survenu, les données n'ont pas été sauvegardée.");
    server.send(200, "text/plain", webString);
  }
  return true;
}

String MajBornes(String Fichier) {
  IPAddress IPborne;
  String retour = "";
  String Ligne = "";
  String num = "";
  String nom = "";
  String lien = "";

  if (SPIFFS.exists("/bornes.trs")) SPIFFS.remove("/bornes.trs");
  SPIFFS_Copy("/bornes.lst", "/bornes.trs");
  if (SPIFFS.exists("/bornes.trs")) {
    File fic = SPIFFS.open("/bornes.trs", "r");
    if (fic) {
      while (fic.available() > 0) {
        Ligne = "";
        char Car = fic.read();
        while (Car != '\n') {
          Ligne += Car;
          Car = fic.read();
        }
        num = MotPos(Ligne, ";", 1);
        nom = MotPos(Ligne, ";", 2);
        lien = MotPos(Ligne, ";", 3);
        if (lien != MyIP) {
          IPborne = StrToIP(lien);
          if (PingBorne(IPborne) == true) {
            TransmetFichier(IPborne, Fichier);
          } else {
            retour += F("La borne ");
            retour += nom;
            retour += F(" (");
            retour += lien;
            retour += F(") ne répond pas!\n");
          }
        }
      }
      retour += F("Le fichier de transmission \"/bornes.trs\" ne peut être ouvert!");
    }
  }
  return retour;
}

String TransmetFichier(IPAddress ipBorne, String Fichier) {
  String num = "";
  String nom = "";
  String lien = "";
  String Ligne = "";
  String Fic = "";
  String retour = "";
  char Car;
  int idx = 0;

  if (!Fichier.startsWith("/")) {
    Fic = "/" + Fichier;
  } else {
    Fic = Fichier;
  }

  if (Fic.indexOf("borne") > 0) { // Fichier "bornes.lst"
    if (SPIFFS.exists(Fic)) {
      if (client.connect(ipBorne, 80)) {
        String URI = "/ajaxsauveborne?nb=" + String(NbreBornes);
        File file = SPIFFS.open(Fic, "r");
        while (file.available() > 0) {
          Ligne = "";
          char Car = file.read();
          while (Car != '\n') {
            Ligne += Car;
            Car = file.read();
          }
          num = MotPos(Ligne, ";", 1);
          nom = MotPos(Ligne, ";", 2);
          lien = MotPos(Ligne, ";", 3);
          URI += "&nom" + String(idx) + "=" + nom + "&lien" + String(idx) + "=" + lien;
          idx++;
        }
        file.close();
        client.print(String("GET ") + URI + " HTTP/1.1\r\n" + "Host: " + IPtoString(ipBorne) + "\r\nConnection: open\r\n\r\n");
        return F("ok");
      } else {
        return F("Erreur connexion");
      }
      client.stop();
    } else {
      retour = F("Fichier \"");
      retour += Fic;
      retour +=  F("\" non trouvé!");
      return(retour);
    }
  } else if (Fic.indexOf("radio") > 0) { // Fichier "radios.lst"
    if (SPIFFS.exists(Fic)) {
      if (client.connect(ipBorne, 80)) {
        String URI = "/ajaxsauveradio?nb=" + String(NbreRadios);
        File file = SPIFFS.open(Fic, "r");
        while (file.available() > 0) {
          Ligne = "";
          char Car = file.read();
          while (Car != '\n') {
            Ligne += Car;
            Car = file.read();
          }
          num = MotPos(Ligne, ";", 1);
          nom = MotPos(Ligne, ";", 2);
          lien = MotPos(Ligne, ";", 3);
          URI += "&nom" + String(idx) + "=" + nom + "&lien" + String(idx) + "=" + lien;
          idx++;
        }
        file.close();
        client.print(String("GET ") + URI + " HTTP/1.1\r\n" + "Host: " + IPtoString(ipBorne) + "\r\nConnection: open\r\n\r\n");
        return F("ok");
      } else {
        return F("Erreur connexion");
      }
      client.stop();
    } else {
      retour = F("Fichier \"");
      retour += Fic;
      retour +=  F("\" non trouvé!");
      return(retour);
    }
  }
}

void handleLogFile() {
  FS * fsys = &SPIFFS;
  String Fichier = "/events_" + String(LogMois) + ".log";
  String argName = "";
  String argValue = "";
  //  String Lg = "";
  char Car = 0;
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  WiFiClient client = server.client();
  int val = 0;
  for (uint8_t i = 0; i < server.args(); i++) {
    argName = server.argName(i);
    server.arg(i);
    if (argName == "doc") {
      Fichier = argValue;
      Fichier.trim();
    }
    if (argName == "del") {
      if (SPIFFS.exists(argValue)) {
        SPIFFS.remove(argValue);
      }
    }
  }
  webString = F("<!DOCTYPE html><html><head>\n");
  webString += F("<meta http-equiv=\"Content-Type\" content=\"application/xhtml+xml; charset=UTF-8\"/>\n");
  webString += F("<meta http-equiv=\"Content-language\" content=\"fr\"/>\n");
  webString += F("<meta http-equiv=\"Pragma\" content=\"no-cache\"/>\n");
  webString += F("<meta http-equiv=\"Cache-Control\" content=\"no-cache\"/>\n");
  webString += F("<meta http-equiv=\"Content-Style-Type\" content=\"text/css\"/>\n");
  webString += F("<meta http-equiv=\"Content-Script-Type\" content=\"text/javascript\"/>\n");
  webString += F("<meta name=\"Description\" content=\"Borne ");
  webString += SysDomo;
  webString += F(": Fichiers log\"/>\n");
  webString += F("<meta name=\"Author\" content=\"Jean-Luc NAPOLITANO\"/>\n");
  webString += F("<link rel=\"icon\" type=\"image/vnd.microsoft.icon\" href=\"/favicon.ico\"/>\n");
  webString += F("<link rel=\"icon\" type=\"image/x-icon\" href=\"/favicon.ico\"/>\n");
  webString += F("<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon.ico\"/>\n");
  webString += F("<link rel=\"icon\" type=\"image/png\" href=\"/favicon.png\"/>\n");
  webString += F("<link rel=\"apple-touch-icon\" href=\"/favicon.png\"/>\n");
  webString += F("<title>Borne ");
  webString += SysDomo;
  webString += F("</title>\n");
  webString += F("<script type=\"text/javascript\">\n");
  webString += F("  var log = new XMLHttpRequest();\n");
  webString += F("  function Fichier() {\n");
  webString += F("    var fich = \"/logfile?doc=\" + document.getElementById('fichier').value;\n");
  webString += F("    document.location.href = fich;\n");
  webString += F("  }\n");
  webString += F("  function Del(fic) {\n");
  webString += F("    var fich = \"/logfile?del=\" + fic;\n");
  webString += F("    document.location.href = fich;\n");
  webString += F("  }\n");
  webString += F("</script>\n");
  webString += F("</head>\n");
  webString += F("<body style='text-align:center;'>\n");
  webString += F("<strong><font size='14px' color='blue'>Fichiers Log</font></strong><br/><br/>\n");
  webString += F("(Fichier et taille en octets)<br/>");
  webString += F("<select id='fichier' style='width:180px;' onChange='javascript:Fichier();'>");

  File root = fsys->open("/", "r");

  File file = root.openNextFile();
  while (file) {
    if (String(file.name()).endsWith(".log") || String(file.name()).endsWith(".bak")) {
      webString += F("<option value='");
      webString += file.name();
      webString += F("'");
      if (Fichier == file.name()) {
        webString += F(" selected");
      }
      webString += F("> ");
      webString += file.name();
      webString += F("&nbsp;&nbsp;-&nbsp;&nbsp;");
      webString += (long) file.size();
      webString += F(" </option>\n");
    }
    file = root.openNextFile();
  }
  file.close();

  webString += F("</select><br/><br/>\n");

  webString += F("<div id='contenu' style='text-align:center;display:inline-block;magin:0 auto;'><p style='text-align:left;'>\n");
  server.sendContent(webString);
  webString = F("<strong><u>Contenu:</u></strong><br/>");
  if (SPIFFS.exists(Fichier)) {
    File fic = SPIFFS.open(Fichier, "r");
    if (fic) {
      while (fic.available() > 0) {
        webString = "";
        Car = fic.read();
        while (Car != '\n') {
          webString += Car;
          Car = fic.read();
        }
        webString += "<br/>";
        server.sendContent(webString);
      }
    }
    fic.close();
  }
  webString = "";
  webString += F("</p></div>\n");
  webString += F("<br/><br/><input type='button' value='Supprimer ");
  webString += Fichier;
  webString += F("' onClick=\"javascript:Del('");
  webString += Fichier;
  webString += F("');\"/>");
  webString += F("</body>\n");
  webString += F("</html>\n");
  server.sendContent(webString);
  server.sendHeader("Connection", "close");
}

void handleListeSons() {
  String file_list[20];
  String file_size[20];

  int file_num = GetMP3List(SPIFFS, "/", 0, file_list, file_size);
//  Serial.print("Nombre de fichiers musicaux:");
//  Serial.println(file_num);
//  Serial.println("Toutes les musiques:");
  webString = F("<table align='center'>\n");
  for (int i = 0; i < file_num; i++) {
    webString += F("<tr><td align='left'>");
    webString += file_list[i];
    webString += F("</td><td align='right'>");
    webString += file_size[i];
    webString += F("</td><td id=\"son");
    webString += String(i);
    webString += F("\"><img src='/play.png' onclick=\"javascript:Jouer('/");
    webString += file_list[i];
    webString += F("', 'son");
    webString += String(i);
    webString += F("');\" title=\"Jouer ce son.\"/></td></tr>\n");
//    Serial.println(file_list[i]);
  }
  webString += F("</table>\n");
  webString += "*" + MyIP + " - " + MAC + " - Ver. " + String(Version) + "*" + MySSID + " - " + MotPos(MyName, "_", 2) + "*" + MotPos(MyName, "_", 1);
  server.sendContent(webString);
  server.sendHeader("Connection", "close");
}

void handleListeRadios() {
  String Lg = "";
  String Chaine = "Aucun";
  char Car;

  if (SPIFFS.exists("/radios.lst")) {
    File fic = SPIFFS.open("/radios.lst", "r");
    if (fic) {
      while (fic.available() > 0) {
        Lg = "";
        Car = fic.read();
        while (Car != '\n') {
          Lg += Car;
          Car = fic.read();
        }
        Lg.replace("\r", "");
        Lg.replace("\n", "");
        Lg = MotPos(Lg, ";", 2);
        Chaine += "," + Lg;
      }
      fic.close();
      server.send(200, "text/plain", Chaine);
      server.sendHeader("Connection", "close");
    }
  }
}

bool handleJouerSon() {
  if (server.args() == 0) return returnFail(F("BAD ARGS"));                                                                 // Au moins un argument doit être fourni
  String argName = "";
  String argValue = "";
  String Son = "";
  int Vol = Volume;

  for (uint8_t i=0; i<server.args(); i++) {
    argName = server.argName(i);
    argValue = server.arg(i);
    argValue.trim();
//    Serial.print(argName);
//    Serial.print(F(" = "));
//    Serial.println(argValue);

    if (argName == "son") {
      Son = argValue;
    }
    if (argName == "vol") {
      Vol = argValue.toInt();
    }
  }
  if (Vol != Volume) {
    AudioLocalVol(Son, VolDef);
  } else {
    audio.setVolume(Volume);
    AudioLocal(Son);
  }
  if (AudioState == true) {
    webString = F("La lecture du son ");
    webString += Son.substring(1);
    webString += F(" a été exécuté.\r\n\r\n");
  } else {
    webString = F("Une erreur s'est produite, vérifiez que le fichier ");
    webString += Son.substring(1);
    webString += F(" existe!\r\n\r\n");
  }
  server.send(200, "text/html", webString);
  return true;
}

// Copie un fichier sous un nom différent dans le SPIFFS
bool SPIFFS_Copy(String src, String dest) {
  bool ret = false;
  String Ligne = "";

  if (SPIFFS.exists(src)) {
    if (SPIFFS.exists(dest)) SPIFFS.remove(dest);
    File FicSrc = SPIFFS.open(src, "r");
    if (FicSrc) {
      File FicDest = SPIFFS.open(dest, "w");
      if (FicDest) {
        while (FicSrc.available() > 0) {
          Ligne = "";
          char Car = FicSrc.read();
          while (Car != '\n') {
            Ligne += Car;
            Car = FicSrc.read();
          }
          FicDest.println(Ligne);
        }
      }
      FicDest.close();
      FicSrc.close();
    }
  }
}

// Fontion split : retourne le nombre de mot (chaine) trouvé
int NbMots(String chaine, String sep) {
  int lgNb = 0, lgOld = 0, lgPos = 0;

  lgPos = chaine.indexOf(sep, 0);
  while (lgPos > 0) {
    lgNb++;
    lgOld = lgPos;
    lgPos = chaine.indexOf(sep, lgPos + 1);
  }
  if (lgOld < chaine.length()) {
    lgNb++;
  }
  return lgNb;
}

// Fonction split : renvoi le mot (chaine) demandé
String MotPos(String chaine, String sep, int lgNoMot) {
  int lgOld = 0, lgPos = 0, lgNb = 0;
  String sChaine = "";

  lgPos = sChaine.indexOf(sep, 0);
  while (lgNb < lgNoMot) {
    lgNb++;
    if (lgNb <= lgNoMot) {
      lgOld = lgPos;
      lgPos = chaine.indexOf(sep, lgPos + 1);
    }
  }

  lgOld++;
  if (lgPos > lgOld) {
    sChaine = chaine.substring(lgOld, lgPos);
  } else {
    sChaine = chaine.substring(lgOld, chaine.length());
  }
  sChaine.trim();
  return sChaine;
}

String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    if(mac[i] < 10) {
      result += "0" + String(mac[i], 16);
    } else {
      result += String(mac[i], 16);
    }
    if (i < 5) result += ':';
  }
  result.toUpperCase();
  return result;
}

// Convertir une adresse IP en string
String IPtoString(IPAddress nIP) {
  String result;
  for (int i = 0; i < 4; ++i) {
    result += String(nIP[i]);
    if (i < 3) result += '.';
  }
  return result;
}

// Convertir une adresse String en IP
IPAddress StrToIP(String nIP) {
  IPAddress result;
  for (int i = 0; i < 4; ++i) {
    result[i] += MotPos(nIP,".",i+1).toInt();
  }
  return result;
}

bool PingBorne(IPAddress adr) {
  if (ping_start(adr, 4, 0, 0, 5)) {
    return true;
  } else {
    return false;
  }
}

/***************** UTF8-Decoder: convert UTF8-string to extended ASCII *****************/
byte c1;                                                                                                                    // Last character buffer
/* Convert a single Character from UTF8 to Extended ASCII
   Return "0" if a byte has to be ignored
*/
byte utf8ascii(byte ascii) {
  if ( ascii < 128 ) {                                                                              // Standard ASCII-set 0..0x7F handling
    c1 = 0;
    return ( ascii );
  }

  // get previous input
  byte last = c1;                                                                                   // get last char
  c1 = ascii;                                                                                       // remember actual character

  switch (last) {                                                                                   // conversion depending on first UTF8-character
    case 0xC2: return  (ascii);  break;
    case 0xC3: return  (ascii | 0xC0);  break;
    case 0x82: if (ascii == 0xAC) return (0x80);                                                    // special case Euro-symbol
  }
  return  (0);                                                                                      // otherwise: return zero, if character has to be ignored
}

// convert String object from UTF8 String to Extended ASCII
String utf8ascii(String s) {
  String r = "";
  char c;
  for (int i = 0; i < s.length(); i++) {
    c = utf8ascii(s.charAt(i));
    if (c != 0) r += c;
  }
  return r;
}

// In Place conversion UTF8-string to Extended ASCII (ASCII is shorter!)
void utf8ascii(char* s) {
  int k = 0;
  char c;
  for (int i = 0; i < strlen(s); i++) {
    c = utf8ascii(s[i]);
    if (c != 0)
      s[k++] = c;
  }
  s[k] = 0;
}











/* END */
