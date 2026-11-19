# Chelles Météo (plugin ChellesMeteo)

Ce que fait la météo, toute seule à chaque partie (rien n'est modifié dans la map Rachid) :

| Quoi | Comment |
|---|---|
| **Jour / nuit à l'heure réelle** | Le ciel GoodSky suit l'heure du PC. Lever et coucher du soleil calculés pour Chelles selon la date (heure d'hiver / d'été automatique). |
| **Le soir en hiver** | Après le coucher (~17h) le ciel passe au **bleu nuit**, bleu nuit profond à **19h**, puis **orangé** à partir de 19h30 (orange plein à **20h**). L'orange s'efface le matin avant le lever. |
| **Neige** | Du 15/11 au 28/02 : la neige du pack WinterTown (`PS_Snow`, celle qui marche) tombe autour de chaque joueur. |
| **Sapins de Noël** | Du 15/11 au 06/01 : sapins du Christmas_pack posés sur l'herbe du quartier, **guirlandes lumineuses** du pack + petites lumières **rouges, vertes, bleues, or, roses** qui clignotent la nuit. |

Pourquoi ça ne marchait pas avant : le ciel jour/nuit (Tools\CycleJourNuit) et la neige (Tools\Neige) avaient été
installés sur l'ancienne map. La map **Rachid** n'a ni ciel GoodSky ni neige. Le plugin les ajoute au lancement, sans toucher la map.

## Installer (une seule fois)

1. Fermer Unreal.
2. Double-cliquer `Plugins\ChellesMeteo\Compiler_le_plugin.bat` (1 à 5 minutes).
3. Ouvrir le projet, Play.

## Tester tout de suite (console, touche ²)

- `Chelles.Meteo Heure 19` : voir le ciel de 19h (bleu nuit). `Chelles.Meteo Heure 20.5` : ciel orangé. `Chelles.Meteo Heure 12` : midi.
- `Chelles.Meteo Auto` : retour à l'heure réelle.
- `Chelles.Meteo Hiver 1` / `Hiver 0` : forcer / couper la neige et les sapins.
- `Chelles.Meteo PoserSapin` : pose un sapin devant vous (gardé dans `Sapins_Positions.txt`). `Chelles.Meteo RetirerSapin` : enlève le plus proche.
- `Chelles.Meteo Info` : heure, lever/coucher, % de bleu et d'orange, neige, nombre de sapins.
- `Chelles.Meteo Relire` : relire `Meteo.txt` sans relancer.

## Régler

Tout est dans `Meteo.txt` (Bloc-notes, pas besoin de recompiler) : mode Réel / Accéléré / Fixe, minutes du bleu et de l'orange
après le coucher, couleurs, luminosité, dates de la neige et des sapins, nombre et taille des sapins, lumières.

Journal : lignes `[Meteo]` dans `Saved\Logs\FPS_Chelles.log`.
Pour tout enlever : supprimer le dossier `Plugins\ChellesMeteo`.
