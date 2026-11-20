# Chelles Vol de voiture (plugin)

Vol de voiture façon GTA dans CHELLES 3D, avec des autos en plus dans les rues et la police
qui te poursuit. Rien à poser dans la carte : le plugin s'ajoute tout seul quand on clique sur Play.
Il ne modifie aucun Blueprint, aucun pack et aucun autre plugin.

## Ce que ça fait

- **F près d'une voiture de la circulation** : le conducteur est éjecté et s'enfuit en courant
  (personnage du pack *Hijacker*), et tu prends le volant d'une vraie voiture du même modèle
  (*Vehicle Variety Pack* : Hatchback, SUV, Pickup, Sportive, Camion).
- **Autos garées** : 8 autos libres sont garées au bord des rues autour de toi (Vehicle Variety Pack,
  2 sportives du *Sports Vehicles Pack*, le *Buggy*). Elles apparaissent hors de ta vue et
  disparaissent quand tu t'éloignes, pour garder le jeu fluide. **F** à côté = monter.
- **Conduite** : Z/S avancer/reculer, Q/D tourner, Espace frein à main, souris pour regarder,
  H klaxon (plugin Sons des véhicules), **F** ou **E** pour descendre.
- **Niveau RECHERCHÉ** (en haut de l'écran : `RECHERCHE * * . . .`) : chaque voiture volée dans
  la circulation donne une étoile. La police arrive (1 voiture par étoile, 3 au plus), sirène
  allumée. Reste loin d'elle (plus de 120 m) pendant 25 secondes pour perdre une étoile.
  Immobile à moins de 7 m d'une voiture de police pendant 4 secondes = **ARRÊTÉ**, étoiles effacées.
- Marche en solo et en multijoueur (le serveur crée les voitures, chacun voit les mêmes).

## Installation (une seule fois)

1. Fermez Unreal.
2. Double-cliquez sur `Plugins\ChellesVolVoiture\Compiler_le_plugin.bat` (1 à 5 minutes,
   jusqu'à « TERMINE »).
3. Ouvrez le projet, cliquez sur Play, approchez-vous d'une voiture qui roule et appuyez sur **F**.

## Réglages

Tout est dans `Plugins\ChellesVolVoiture\VolVoiture.txt` (Bloc-notes) : touches, nombre d'autos
garées, nombre de voitures de police, durée des étoiles, voitures utilisées pour chaque modèle.
Après une modification : relancez la partie, ou tapez `Chelles.Vol Relire` dans la console (touche ²).

Console (touche ²) :
- `Chelles.Vol` : état (modèles trouvés, autos sur la carte, police en poursuite) ;
- `Chelles.Vol Etoiles 3` : se donner 3 étoiles pour tester la poursuite (`Etoiles 0` = plus recherché) ;
- `Chelles.Vol Garer` : garer des autos tout de suite.

Journal : lignes `[Vol]` dans `Saved\Logs\FPS_Chelles.log`.

## Bon à savoir

- Descends de la voiture (F) avant de changer de mode avec la roue TAB.
- Les voitures de police qui te poursuivent sont celles du pack *Sports Vehicles Pack* :
  le plugin Police te laisse les prendre avec **E** si tu arrives à en approcher une.
- Pour le jeu exporté, les dossiers des voitures sont ajoutés à la liste « toujours inclure »
  de `Config\DefaultGame.ini` (VehicleVarietyPack, VH_Buggy, Hijacker).
