# Sons des véhicules (CHELLES 3D)

Bruit du moteur, de l'accélération et klaxon pour **tous les véhicules** :

| Véhicule | Moteur | Klaxon |
|---|---|---|
| Voitures de la circulation | ralenti à l'arrêt, monte avec la vitesse, change de rapport, plus fort quand elle accélère | de temps en temps, et quand elle est bloquée (bouchon, piéton ou joueur devant) |
| Bus (invasion zombie) | moteur diesel plus grave | « tut-tut » quand il repart d'un arrêt |
| Motoneige (touche M) | son du pack (déjà prévu par le pack), sinon celui du plugin | touche **H** (klaxon du pack) |
| Voiture de la roue TAB et autres véhicules à roues | son du pack s'il marche, sinon celui du plugin (suit le vrai régime moteur) | touche **H** |

- On klaxonne **tant qu'on appuie sur H**. En multijoueur, les autres joueurs l'entendent.
- Volume : **OPTIONS > AUDIO > VÉHICULES** (agit aussi sur les sons de moteur des packs).
- Sons utilisés : uniquement des sons déjà dans le projet (DriveableCar_basic, DV_Car_Horo, DV_Snowmobile). Aucun son ajouté.
- Seuls les moteurs les plus proches sont joués (8 par défaut, à 45 m) pour ne pas alourdir le jeu.

## Installation

Unreal fermé, double-cliquez `Tools\SonsVehicules\INSTALLER_SONS_VEHICULES.bat`.
Il compile ce plugin puis ChellesMenu (pour le curseur VÉHICULES). Rien à poser dans la carte.

## Réglages

`Plugins\ChellesSonsVehicules\SonsVehicules.txt` (Bloc-notes) : volume, touche, distance, sons, fréquence des klaxons.
Pas besoin de recompiler : relancez le jeu ou tapez dans la console (touche ²) `Chelles.Vehicules Relire`.

## Console (touche ²)

- `Chelles.Vehicules` : état (véhicules trouvés, moteurs entendus, vitesse, régime)
- `Chelles.Vehicules Klaxon` : fait klaxonner le véhicule le plus proche (test)
- `Chelles.Vehicules 0` / `1` : couper / remettre
- Journal : lignes `[Vehicules]` dans `Saved\Logs\FPS_Chelles.log`

## Retirer

Supprimez le dossier `Plugins\ChellesSonsVehicules`. Aucun Blueprint ni autre plugin n'a été modifié
(sauf la ligne VÉHICULES du menu, sans effet si ce plugin n'est plus là).
