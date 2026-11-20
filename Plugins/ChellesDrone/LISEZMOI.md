# Drone compagnon (CHELLES 3D)

Chaque joueur a son drone : le modèle **Police Drone** du dossier `Content\PoliceDrône` (blanc et noir, deux canons), avec son animation. Si ce modèle est introuvable, l'ancien petit drone en formes simples revient tout seul. Il vole au-dessus de l'épaule droite, suit le joueur partout (à pied, en vol, en véhicule) et tire tout seul sur les ennemis et les zombies proches.

## Installer (une fois)

1. Fermez Unreal.
2. Double-cliquez sur `Plugins\ChellesDrone\Compiler_le_plugin.bat` (1 à 5 minutes).
3. Ouvrez le projet et lancez la partie (Play) : le drone apparaît à côté de vous.

Rien n'est ajouté dans la carte ni dans les Blueprints : le drone est fabriqué par le code au lancement.

## En jeu

| Touche | Action |
|---|---|
| F10 | ranger le drone / le rappeler |

- Il vise l'ennemi **le plus proche qu'il voit** (pas à travers les murs), à 25 m maximum autour de vous.
- 2 tirs par seconde, dans le torse : un zombie de l'invasion tombe en 3 tirs. Vos tirs à la tête restent plus efficaces.
- Il ne tire jamais sur les joueurs, les passants, les policiers ni les marchands.
- Ses tirs comptent comme les vôtres : l'ennemi touché vous prend en chasse.
- Quand vous êtes mort, il attend sans tirer.
- Multijoueur : chaque joueur a son drone, le serveur décide des tirs, tout le monde voit les drones et les lasers.

## Réglages

Ouvrez `Plugins\ChellesDrone\Drone.txt` avec le Bloc-notes (portée, cadence, dégâts, couleurs, place, touche, volume). Relancez la partie, pas besoin de recompiler.

## Le modèle 3D

- Dans `Drone.txt` : `Modele=` (chemin du modèle), `Animation=`, `TailleModeleCm=` (envergure, 90 par défaut).
- S'il vole de côté ou à l'envers : `RotationModele=90` (ou `-90`, `180`).
- `Modele=` vide = l'ancien drone en formes simples.
- Jeu exporté (démo ou jeu complet) : ajoutez le dossier `PoliceDrône` aux dossiers toujours cuisinés (Project Settings > Packaging > Additional Asset Directories to Cook), sinon le jeu exporté montrera l'ancien drone.

## Console (touche ²)

- `Chelles.Drone` : ranger / rappeler (comme F10)
- `Chelles.DroneInfo` : écrit dans le journal chaque drone, le joueur qu'il suit et sa cible (lignes `[Drone]`)

## Désactiver

Dans `Drone.txt`, mettez `Actif=Non`. Ou supprimez le dossier `Plugins\ChellesDrone`.

## Serveur VPS

Le serveur dédié Linux doit être recompilé avec ce plugin pour que les drones existent en ligne.
