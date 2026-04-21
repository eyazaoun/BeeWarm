# Écoconception – Projet BeeWarm
**Chauffage de ruche solaire avec suivi solaire automatique**  
ELSE4 FISA – Polytech Nice Sophia-Antipolis – 2025/2026  
Groupe : Eya ZAOUN, Valentin GIROD-ROUX, Hussein ELSOUDA, Anas ESSALIMI

---

## 1. Description du projet

### Contexte
Le projet BeeWarm est un système embarqué autonome destiné à réguler la température intérieure d'une ruche d'abeilles en hiver. Il répond à un besoin réel : les abeilles doivent maintenir une température de 30–35 °C à l'intérieur de la ruche pour survivre et se développer, même en période de froid. Le système est conçu pour fonctionner sans raccordement au réseau électrique, en exploitant exclusivement l'énergie solaire.

### Description technique du système

Le système BeeWarm est composé des blocs fonctionnels suivants :

**Bloc 1 – Carte de contrôle PCB (ESP32)**  
La carte électronique principale, conçue sur Altium Designer et fabriquée en 2 couches (Pb-free), héberge :
- Un microcontrôleur **ESP32-WROOM-32E** (WiFi + Bluetooth) qui pilote l'ensemble du système et expose une interface de monitoring à distance via application mobile (App Inventor) en Bluetooth.
- Un capteur de puissance **INA219** (I²C) qui mesure en temps réel la tension et le courant fournis par le panneau solaire, permettant l'implémentation d'un algorithme **MPPT** (Maximum Power Point Tracking) pour optimiser l'extraction d'énergie.
- Quatre capteurs de température **DS18B20** (bus 1-Wire) placés à l'intérieur de la ruche et dans le bac de végétaline pour surveiller la distribution thermique.
- Deux **MOSFET P-channel** (FQB22P10TM, TO-263) commandés en PWM pour réguler la puissance dissipée dans les résistances chauffantes du cadre.
- Un circuit de démarrage conditionnel (comparateur **AP331A** + diode Zener 6,8 V + pont diviseur) qui n'active le système que lorsque la tension du panneau dépasse 10 V.
- Un régulateur **LDO 3,3 V** pour alimenter l'ESP32 et les capteurs.
- Un connecteur **XT60** pour le raccordement au panneau solaire.

**Bloc 2 – Système d'orientation solaire**  
Un panneau solaire (≈ 10 W) est monté sur un support orientable motorisé :
- Un **moteur pas à pas 28BYJ-48** avec son driver **ULN2003** fait pivoter le panneau de gauche à droite.
- Deux **photorésistances (LDR)** mesurent l'intensité lumineuse de chaque côté ; la différence de tension entre les deux guide le moteur vers la position optimale.
- Le support mécanique est réalisé en impression 3D (PLA).

**Bloc 3 – Stockage thermique (végétaline)**  
Un bac contenant de la **végétaline** (graisse végétale, environ 500 g) entoure les résistances chauffantes. Ce matériau à changement de phase (point de fusion ≈ 36 °C) permet de stocker l'énergie thermique accumulée pendant la journée et de la restituer progressivement la nuit, réduisant ainsi les cycles de chauffe.

**Bloc 4 – Cadre chauffant**  
Des résistances chauffantes (fil résistif, ~40 g) sont intégrées dans un cadre format ruche standard. Ce cadre se pose directement sur la ruche, sans modification de la structure existante.

### Flux d'énergie
Le panneau solaire produit jusqu'à 10 W selon l'ensoleillement. L'énergie est acheminée via le connecteur XT60 vers la carte PCB. L'algorithme MPPT optimise le point de fonctionnement. Les MOSFETs régulent la puissance envoyée aux résistances chauffantes selon les mesures de température (consigne : 32 °C). Les données (température, tension, courant, puissance) sont transmises en temps réel via Bluetooth à l'application mobile.

---

## 2. Unité Fonctionnelle (UF)

> **Maintenir la température intérieure d'une ruche standard (10 cadres Dadant) entre 30 et 35 °C pendant 5 mois d'hiver consécutifs (novembre à mars), grâce à un système de chauffage solaire autonome, installé sur le campus de Polytech Nice Sophia-Antipolis (Alpes-Maritimes, France), sur une durée de vie du système de 10 ans.**

### Paramètres de l'unité fonctionnelle

| Paramètre | Valeur |

| Fonction principale | Chauffage thermique d'une ruche |
| Température cible | 30–35 °C (consigne : 32 °C) |
| Volume chauffé | Intérieur d'une ruche Dadant 10 cadres (≈ 40 L) |
| Durée d'utilisation annuelle | 5 mois (novembre–mars) |
| Durée de vie du système | 10 ans |
| Source d'énergie | Solaire photovoltaïque exclusivement (panneau ≈ 10 W) |
| Lieu d'utilisation | Alpes-Maritimes, France (ensoleillement moyen ≈ 1 700 kWh/m²/an) |
| Quantité | 1 ruche |

### Justification des choix

- **5 mois** : période hivernale critique pour la survie des colonies en région PACA.
- **10 ans** : durée de vie typique d'un panneau solaire petite puissance et des composants électroniques en milieu semi-protégé (rucher).
- **Sans réseau électrique** : caractéristique différenciante du système (autonomie complète).
- **1 ruche** : unité de référence cohérente avec les besoins de M. Peter (ruches sur le campus) et facilement extensible.

---

## 3. Périmètre du système pour l'ACV

Le périmètre retenu pour l'Analyse du Cycle de Vie inclut :

- **Fabrication** : production de tous les composants électroniques (PCB, composants passifs, actifs, connecteurs), du panneau solaire, du moteur et du support mécanique.
- **Utilisation** : énergie solaire captée (flux naturel, impact nul), consommation électrique pour l'application mobile (Bluetooth, négligeable).
- **Fin de vie** : non modélisée dans cette version (à améliorer).

Les éléments **exclus** du périmètre :
- La ruche elle-même (existante).
- Le câblage externe (fils).
- Le travail humain d'installation.
- Le développement logiciel (App Inventor, Arduino).

---

## 4. Alternative envisagée pour la comparaison

Afin de comparer deux déclinaisons du projet, l'alternative suivante est proposée :

| Critère | Version actuelle (BeeWarm v1) | Alternative (BeeWarm v2) |
|---|---|---|
| Microcontrôleur | ESP32-WROOM-32E (WiFi + BT) | ESP32-C3-MINI (BT seul) |
| Panneau solaire | 10 W monocristallin | 5 W monocristallin |
| Stockage thermique | Végétaline 500 g | Sans stockage |
| Suivi solaire | Moteur pas à pas + LDR | Panneau fixe (inclinaison optimale fixe) |

La version v2 est plus simple, moins consommatrice en matériaux, mais potentiellement moins efficace thermiquement. L'ACV permettra de quantifier ce compromis.