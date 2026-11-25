# Chelles Vol de voiture (plugin)

Vol de voiture façon GTA dans CHELLES 3D, avec des autos en plus dans les rues et la police
qui te poursuit. Rien à poser dans la carte : le plugin s'ajoute tout seul quand on clique sur Play.
Il ne modifie aucun Blueprint, aucun pack et aucun autre plugin.

## Ce que ça fait

- **F près d'une voiture de la circulation** : le conducteur est éjecté et s'enfuit en courant
  (personnage du pack *Hijacker*), et tu prends le volant d'une vraie voiture du même modèle
  (*Vehicle Variety Pack* : Hatchback, SUV, Pickup, Sportive, Camion).
- **Autos garées** : 8 autos libres sont garées au bord des rues autour de toi (DV_Car_Horo et Bugatti). Elles apparaissent hors de ta vue et
  disparaissent quand tu t'éloignes, pour garder le jeu fluide. **F** à côté = monter.
- **Conduite** : Z/S avancer/reculer, Q/D tourner, Espace frein à main, souris pour regarder,
  H klaxon (plugin Sons des véhicules), **F** ou **E** pour descendre.
- **Niveau RECHERCHÉ** (en haut de l'écran : `RECHERCHE * * . . .`) : chaque voiture volée dans
  la circulation donne une étoile. La police arrive (1 voiture par étoile, 3 au plus), sirène
  allumée. Reste loin d'elle (plus de 120 m) pendant 25 secondes pour perdre une étoile.
  Immobile à moins de 7 m d'une voiture de police pendant 4 secondes = **ARRÊTÉ**, étoiles effacées.
- Marche en solo et en multijoueur (le serveur crée les voitures, chacun voit les mêmes).

## 25/11/2026 : autos garées DV_Car_Horo + Bugatti

- Les autos garées au bord des rues sont maintenant les voitures du pack **DV_Car_Horo** (3 sur 4)
  et la **Bugatti** de Rachid (1 sur 4). Toutes les plaques affichent **CHELLES-3D**.
- Dans une DV_Car_Horo : **L** phares, **R** saut américain, **Retour arrière** remettre sur ses roues + réparer,
  **G** / **U** clignotants gauche / droite, **V** warnings, **H** klaxon, **C** caméra intérieure / extérieure,
  **Inser** ordinateur de bord, **F** ou **E** descendre. Phares, saut, réparation, clignotants et warnings
  sont rejoués chez tous les joueurs en multijoueur.
- La Bugatti est une DV_Car_Horo (même conduite, mêmes fonctions) dont l'apparence est remplacée :
  carrosserie de Rachid + 4 roues accrochées aux os des roues de la Horo, calées toutes seules.
- Installation : `Tools\VoituresHoro\INSTALLER_VOITURES_HORO.bat` (Unreal fermé) : compile ce plugin,
  importe la Bugatti dans `/Game/Chelles/Bugatti`, crée `BP_Bugatti_Chelles`, ajoute les touches C et Inser.
- Réglages : section « 25/11 » de `VolVoiture.txt` (liste `Garees`, `Plaque`, touches, options du pack,
  retouches de la Bugatti).

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
