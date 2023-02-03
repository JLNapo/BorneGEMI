# Enceinte-Connectée-Domotique
Créer une enceinte connectée pour votre domotique est possible à moindre frais.

Une enceinte connectée avec peu d'élément et capable de commander votre domotique sans passer par un serveur externe est faisable facilement avec des éléments Arduino et un  peu d'électronique.
Cette borne (appelons la comme cela) est principalement composée de 2 processeurs.
- Un Arduino NANO 33 BLE Sense modèle ABX0035 (maintenant remplacé par ABX0070, même fonctionnement mais librairie différentes)
- Un ESP-WROOM-32 module prenant moins de place que les ESP32 et tout leur système de connexion et programmation.
- Un TTL 74HC595 (registre à décalage) permettant la gestion de 8 LEDs pour le réglage du volume (principalement)
- 9 LEDs dont une RVB piloté par 3 transistors afin de ne pas surcharger les GPIO de l'ESP32
- Quelques resistances
- des connecteurs
- Une alimentation secteur à sortie 5V 1,5A ou plus (1A permet le fonctionnement, mais il est préférable de garder une marge)
- Un haut parleur
- Un boitier fait avec une imprimante 3D (3 pièces)
- Un peu de tissus spécial haut parleur (un tissus à mailles pas très serré peu convenir)

# Logiciels employés:
- Picovoice (https://console.picovoice.ai)
- Voicerss (http://api.voicerss.org)
- Arduino IDE (Ver. 1.8.19.0)
- Sketchup 17 (Google gratuit) boitier 3D
- Cura (Ver. 5.0.0.0) pour le GCode
- Editplus (pour les pages WEB) Notepad++ ou tout autre éditeur peut convenir.
- Kicad 5.0 pour la conception du circuit imprimé (réalisation chez JLCPCB https://jlcpcb.com)

# Librairies ESP32
- WebServer.h
- WiFiClient.h
- WiFi.h
- WiFiMulti.h
- Update.h
- WiFiUdp.h
- "RTClib.h"
- Audio.h
- SPIFFS.h
- HardwareSerial.h
- "ESP8266FtpServer.h"
- "OTApage.h" (données pour la mise à jour par le WEB)
- "esp32_ping.h"

# Librairie NANO 33 BLE Sense
- Picovoice_FR.h (plusieurs langues disponnibles)
- "params.h" (données obtenues sur le site Picovoice)

# Matériel pour le boitier (plusieurs essais) j'ai 4 imprimantes 3D ça aide:
- Imprimante 3D Tenlog TL-D3 Pro
- Artillery Sidewinder X2
- Elegoo Saturn (résine)
- Leapfrog (pas utilisé)
- Tissus Haut Parleur Audio acheté sur Amazon.fr

<b>Nota:</b> toutes imprimantes peut convenir (en tenant compte des dimensions)

Vous trouverez ici l'ensemble de la réalisation ainsi qu'une courte vidéo.
# Réalisation
Cette version est une version beta et correspond à ma domotique qui est quasiment entièrement faite par moi au fil du temps.
J'ai commencé la domotique vers la fin des années 80. Jusque là (et c'est toujours le cas) c'est mon serveur qui tourne 24/7 qui s'en occupait totalement, mais ces dernières années avec l'arrivé des enceintes connectées (j'ai 3 "Google Home" et 2 "Amazon Alexa") mais Google s'est retiré de IFTTT, du coup cela devenait moins pratique pour moi qui ait plus d'objets connectés DIY que du commerce, alors j'ai décidé de créer la mienne et de partager mon expérience avec vous.<br/>
J'en ai construit 3 (pour le moment) qui donne satisfaction au quotidien et sans solliciter aucun serveur externe, certaine réponses des objets connectés sont du texte qui ne peut pas être traité en TTS par l'ESP32, alors j'envoi au serveur Voicerss le texte et je stream le mp3 créé en temps réel sans saturer le SPIFFS de l'ESP32 et la réponse est quasi instantannée. Les commandes et réponses se font au même rytme qu'avec les enceintes connectées du marché mais je n'ai pas recourt à un serveur pour connaitre la commande et l'exécuter (protection des données personnelles)
# Aide
Je suis ouvert à toutes suggestions ou questions et même à une aide (dans la mesure du possible)<br/>
Je peux fournir les fichiers du CI (fichiers Kicad) par exemple.

<b>Nota:</b> Ce partage ne peut en aucun cas faire l'objet de fabrications commerciales, et n'est là qu'a titre ludique pour des amateurs comme moi désireux de garder pour soit des informations qui sont souvent exploitées par les grandes firmes lors de nos commandes vocales par le biais de celles ci.
