# Enceinte-Connectée-Domotique
Créer une enceinte connectée pour votre domotique est possible à moindre frais.

Une enceinte connectée avec peu délément et capable de commander votre domotique sans passer par un serveur externe est faisable facilement avec des éléments Arduino et un  peu d'électronique.
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

Vous trouverez ici l'ensemble de la réalisation ainsi qu'une courte vidéo.

Cette version est une version beta et correspond à ma domotique qui est quasiment entièrement faite par moi au fil du temps.
J'ai commencé la domotique vers la fin des années 80. Jusque là (et c'est toujours le cas) c'est mon serveur qui tourne 24/7 qui s'en occupait totalement, mais ces dernières années avec l'arrivé des enceintes connectées (j'ai 3 "Google Home" et 2 "Amazon Alexa") mais Google s'est retiré de IFTTT du coup cela devenait moins pratique pour moi qui ait plus d'objets connectés DIY que du commerce, alors j'ai décidé de créer la mienne et de partager mon expérience avec vous.

<b>Nota:</b> Ce partage ne peut en aucun cas faire l'objet de fabrications commerciales, et n'est là qu'a titre ludique pour des amateurs comme moi désireux de garder pour soit des informations qui sont souvent exploitées par les grandes firmes lors de nos commandes vocales par le biais de celles ci.
