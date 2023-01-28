/*
IMPORTANT: Etre connecté avec son compte: mon_email@fai.fr / mot de passe

Nota: Ce fichier est là juste pour mémoire de ce qui est prévu dans le vocabulaire que j'ai voulu, et ne correspondra surement pas à vos attentes et n'est rien d'autre qu'un exemple.

Access Key: Clé de votre compte
UID: UID du NANO
VoiceRssKey = "Clé de votre compte"

Lien vers le site de création de commandes verbales: https://console.picovoice.ai
Création du mot d'activation: Choisir ensuite "Porcupine"
Création des commandes: Choisir "Rhino"

Activation: Hé, GEMI  // (pour moi) GEMI est un accronyme qui veut dire Gestion Electronique de Maison Individuelle (nom choisis vers la fin des années 80's)

context:
  expressions:
    changeColor:
      - "[Mets, Mettre, Change] [les, la] [lumières, lumière] (en) $color:color"
      - "[Mets, Mettre, Change] [la, le] $object1:object [du, de, de la]
        $location:location [en, de, du, de la] (couleur) $color:color"
    commande:
      - $ordre:ordre [le, la, les, des, un, une] $object1:object [à, en, à l', à
        la, en l', pour, dans, du] $ordre:ordre2
      - $ordre:ordre [le, la, les, des, un, une] $object1:object [à, en, à l', à
        la, en l', pour, dans, du] $ordre:ordre2 [pour, pendant]
        $pv.TwoDigitInteger:number $temps:time
      - $ordre:ordre [le, la, les, des] $object1:object [à, en, à l', en l',
        pour, pendant] $pv.TwoDigitInteger:number $temps:time
      - $ordre:ordre $object1:object
      - $ordre:ordre [tous, tout] les $object1:object
      - $ordre:ordre [le, la] $location:location
      - "[Mets, Mettre] [les, la, le] $object1:object [de la, dans la, dans le,
        du] $location:location [sur la, sur la chaine, chaine]
        $pv.TwoDigitInteger:number"
      - "[Mets, Mettre] [à, au, du] (l', en) $ordre:ordre [la, le, les]
        $object1:object"
      - "[Mets, Mettre] [les, la, le] $object1:object [de la, dans la, dans le,
        du] $location:location [à, en, à l'] $ordre:ordre"
      - "[Mets, Mettre] [à, au, du] (l', en) $ordre:ordre [la, le, les]
        $object1:object [du, de la, dans la, dans le] $location:location"
      - $ordre:ordre [les, la, le] $object1:object [de la, dans la, dans le, du]
        $location:location
      - $ordre:ordre [les, la] $object1:object
      - $ordre:ordre [les, la] $object1:object [du, de la, dans la, dans le,
        dans l'] $location:location
      - $ordre:ordre (le, la, les, des, un, une) $object1:object (sur, à, en, à
        l', à la, en l', pour, dans, du) $stations:station
      - $ordre:ordre (la, le, l') $object1:object
      - $ordre:ordre [le, la, les, des] $object1:object $preposition:prepos
        $object2:objet
    virtuel:
      - (est ce) (qu') (il) [y a, comment est] [du, de la, le] $object1:object
      - "[dis, dire] $object1:object à $prenom:prenom"
      - "[c'est qui, qui est, tu es] [le, la, les] $object1:object"
      - "[quel, quelle] $object1:object est il"
      - "[quel, quelle] $object1:object sommes nous"
      - "[quel, quelle] est [la, le, l'] $object1:object"
      - "[quel, quelle] $object1:object [du, de la, de l'] $object1:object2
        sommes nous"
      - "[quel, quelle] $object1:object fait il"
      - "[quel, quelle] est la $object1:object [du, de la, de]
        $location:location"
      - "[quel, quelle] est la $object1:object de la $location:location de
        $prenom:prenom"
      - $ordre:ordre un bon $object1:object à $prenom:prenom
      - "[quel, quelle] $object1:object fait il [au, dans le, dans la]
        $location:location"
      - $ordre:ordre (une, l') $object1:object $preposition:prepos
        $pv.TwoDigitInteger:number $temps:time
      - $ordre:ordre (une, l') $object1:object $preposition:prepos
        $pv.TwoDigitInteger:number $temps:time $pv.TwoDigitInteger:number2
      - $ordre:ordre (une, l') $object1:object $preposition:prepos
        $pv.TwoDigitInteger:number $temps:time $pv.TwoDigitInteger:number2
        $temps:time2
  slots:
    color:
      - bleu
      - vert
      - orange
      - rose
      - violet
      - rouge
      - blanc
      - jaune
    location:
      - salle de bains
      - toilettes
      - chambre
      - chambre sylvie
      - chambre jean-luc
      - chambre à coucher
      - penderie
      - placard
      - couloir
      - cuisine
      - salle de séjour
      - salon
      - garde manger
      - séjour
      - sous-sol
      - dressing
      - escalier
      - garage
      - salle à manger
      - cabinet de toilette
    object1:
      - télé
      - tv
      - télévision
      - cafetière
      - lave-vaisselle
      - machine à laver
      - four
      - frigidaire
      - frigo
      - ventilateur
      - boite aux lettres
      - boite
      - courrier
      - alarme
      - GÉMI
      - serveur
      - portail
      - portillon
      - volets
      - bonjour
      - meilleur
      - heure
      - jour
      - date
      - semaine
      - mois
      - année
      - température
      - météo
      - porte
      - fenêtre
      - volet
      - lumière
      - ventilo
      - clim
      - climatiseur
      - climatisation
      - anniversaire
      - couleur
      - radio
      - temps
      - volume
      - son
      - pause
      - micro
      - jauge
      - jauge à fioul
      - niveau
      - niveau fioul
      - chauffage
    ordre:
      - marche
      - arrêt
      - ajoute
      - supprime
      - ouvre
      - ferme
      - allume
      - éteint
      - mets
      - mettre
      - souhaite
      - annule
      - redémarre
      - en route
      - monte
      - augmente
      - baisse
      - diminue
      - arrête
      - coupe
      - remets
    temps:
      - minute
      - minutes
      - seconde
      - secondes
      - heure
      - heures
      - jour
      - jours
    prenom:
      - Inès
      - Karine
      - Cédric
      - Sylvie
      - Jean-Luc
      - Michel
      - Philippe
      - Bruno
      - Fabienne
      - Thomas
      - Gabriel
      - Karim
      - Erick
      - Jenifer
      - Benoit
      - Guillaume
      - Linda
      - Patricia
    stations:
      - Chérie FM
      - RFI
      - France-Culture
      - Fip
      - RTL
      - RTL Deux
      - France Info
      - Radio Classique
      - France Musique
      - Radio Crooner
      - Europe Numéro Un
      - Sud Radio
      - France Inter
      - Fréquence Jazz
      - Le Mouve
      - Suisse première
      - RTBF
      - Radio Grenouille
      - RMC
      - Skyrock
      - RFM
      - Rires et Chansons
    preposition:
      - sur
      - dans
      - à
      - pour
    object2:
      - gémi
      - serveur
  macros: {}
*/
