# Missions dans le quartier (CHELLES 3D)

Trois missions à jouer dans Chelles, en solo ou en multijoueur (c'est l'hôte
qui les lance). Appuyez sur **N** pour ouvrir l'écran **MISSIONS**.

1. **ESCORTE DU TRAMWAY** : rejoignez le TRAMWAY (repère doré), puis gardez-le
   en vie pendant **3 arrêts**. À chaque arrêt, des zombies sortent de terre
   autour de lui. Un zombie collé au tramway lui retire de la vie. Si plus
   personne n'est près du tramway pendant 25 s, la mission est ratée.
2. **DÉFENDRE LA MAIRIE** : allez à la Mairie, puis tenez **3 minutes** contre
   des vagues de zombies de plus en plus grosses. Les zombies qui entrent dans
   la Mairie lui retirent de la vie.
3. **CHASSE AUX ZOMBIES** : tuez **25 zombies en 4 minutes**, là où vous êtes.

Mission réussie = de l'**XP** pour les pouvoirs (+150, +150, +100).

À l'écran : le panneau de la mission est **en haut à gauche** (la mini-carte
reste en haut à droite). Un **losange doré** montre où aller, avec la distance,
et une **colonne de lumière dorée** de 60 m marque l'objectif dans le quartier.

## Installation (une seule fois)

1. Fermez Unreal.
2. Double-cliquez `Plugins\ChellesMissions\Compiler_le_plugin.bat` et attendez
   la fin (1 à 5 minutes).
3. Ouvrez le projet, lancez Play, appuyez sur **N**.

Rien d'autre à recompiler : les plugins Zombies, Tramway, Dernier Refuge et
Pouvoirs ne sont **pas modifiés**. Les missions les pilotent par le nom de
leurs réglages, comme le fait déjà le Dernier Refuge.

## La place de la Mairie

Par défaut, c'est le centre du Dernier Refuge (déjà enregistré au parc de la
Mairie). Pour la changer : allez au bon endroit en jeu, ouvrez la console
(touche **²**) et tapez `Chelles.MissionIci Mairie`. C'est enregistré dans
`Missions_Positions.txt`.

## Commandes pour tester (console, touche ²)

- `Chelles.Mission 1` : lance l'escorte du tramway (2 = Mairie, 3 = chasse)
- `Chelles.Mission` : ouvre l'écran MISSIONS (comme N)
- `Chelles.MissionFin` : abandonne la mission en cours
- `Chelles.Missions` : état des missions dans le journal (lignes `[Missions]`)

## Réglages (Bloc-notes, pas besoin de recompiler)

`Missions.txt` : touche, nombre d'arrêts, vie du tramway et de la Mairie,
nombre de zombies, durées, XP gagnée. Relancez le jeu après avoir enregistré.

## Bon à savoir

- Pendant une mission, l'invasion automatique toutes les 2 h est en pause.
  Elle reprend comme avant à la fin.
- Pas de mission pendant le Dernier Refuge (F7), ni pendant une invasion.
- Le tramway doit être sur la carte (plugin ChellesTrain) pour l'escorte.
- Rien n'est modifié dans les Blueprints ni les assets du projet.
