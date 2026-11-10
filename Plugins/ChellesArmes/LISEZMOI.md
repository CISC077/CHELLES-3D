# Chelles Armes (plugin ChellesArmes)

## 1. Nouvelles apparences des armes du kit FPS_Game

Chaque arme du kit (enfant de `BP_WeaponBase`) reçoit un modèle des packs **FPS_Assault_Pack**,
**FPS_Weapon_Bundle** et **FPSAssets**. Le nouveau modèle est posé **par-dessus** l'ancien, qui devient
invisible mais reste en place : animations, mains, visée, munitions, rechargement et tir du Blueprint
ne changent pas. **Aucun Blueprint n'est modifié** : retirer le plugin (ou `Apparences=Non`) = armes d'origine.

| Arme du kit FPS_Game | Nouvelle apparence |
|---|---|
| BPC_WeaponRifle (fusil d'assaut) | FPS_Assault_Pack **SK_Rifle** |
| BPC_WeaponRecoil | FPS_Assault_Pack **SK_RU74** |
| BPC_WeaponSniper | FPS_Weapon_Bundle **KA Val** |
| BPC_WeaponPistolAuto | FPS_Weapon_Bundle **SMG11** |
| BPC_WeaponSword (machette) | FPS_Weapon_Bundle **couteau M9** |
| BPC_WeaponUE4 / BPC_WeaponTemplate | FPSAssets **M4A1** / **AK** (sinon AR4 / KA47 du Bundle) |
| Pistol, Shotgun, Launcher, Laser_New, Beam | gardent leur apparence (pas d'équivalent) → tir spécial |

Les armes **M4A1 et AK de FPSAssets** ne sont que des fichiers bruts (dans `_Quarantaine\Content\FPSAssets\Weapon`) :
les importer une fois avec `Tools\Armes\importer_armes_fpsassets.py` (Fichier > Exécuter un script Python).

Taille et sens du nouveau modèle sont calculés tout seuls (longueur du canon, bout du canon, chargeur en bas).
Si une arme est mal placée, la corriger **en jeu** avec la console (touche ²), l'arme en main :

- `Chelles.Arme Roulis 180` : retourne l'arme (si elle est à l'envers)
- `Chelles.Arme Lacet 180` : fait pivoter (si le canon est vers l'arrière)
- `Chelles.Arme Avant 3` / `Haut -2` / `Cote 1` : déplace de quelques cm
- `Chelles.Arme Tangage 5` : lève/baisse le canon
- `Chelles.Arme Echelle 1.1` : 10 % plus grand (`0.9` = plus petit)
- `Chelles.Arme Zero` : annule les réglages · `Chelles.Arme Original` : garder l'ancienne apparence pour cette arme
- `Chelles.Arme` seul : affiche le réglage actuel

Chaque réglage est enregistré tout seul dans `Armes.txt` (ligne `Reglage.<arme>=`).
`Chelles.Armes 0` / `Chelles.Armes 1` : comparer avec les armes d'origine.

## 2. Tirs spéciaux, EN PLUS du tir normal

Quand l'arme en main tire (ses munitions baissent), un tir spécial part aussi :

- **LASER** : trait rouge très rapide
- **BOULE DE FEU** : explose à l'impact (dégâts autour)
- **GLACIAL** : gèle la cible 3 s (elle ne bouge plus)
- **FOUDRE** : l'éclair saute sur les 3 ennemis les plus proches

Touche **O** : fait défiler le tir spécial de l'arme en main (AUCUN → LASER → FEU → GLACIAL → FOUDRE).
Le choix est retenu pour la partie suivante. Le tir spécial actif s'affiche en bas de l'écran.

Au départ : Pistol = Laser, Shotgun = Glacial, Launcher = Boule de feu, Laser_New = Foudre, Beam = Glacial,
les autres armes = aucun (O pour en choisir un). Tout se règle dans `Armes.txt` (dégâts, cadence, vitesse, durée du gel...).

Zombies : la règle du plugin Zombies reste valable (tête = mort, corps = 33 %).
Multijoueur : c'est le serveur qui crée les tirs, tout le monde les voit ; le PVP du menu est respecté.

## Installer

1. Fermer Unreal.
2. Double-cliquer `Plugins\ChellesArmes\Compiler_le_plugin.bat`.
3. Ouvrir le projet. (Une fois) Fichier > Exécuter un script Python > `Tools\Armes\importer_armes_fpsassets.py`.
4. Play.

Journal : lignes `[Armes]` dans `Saved\Logs\FPS_Chelles.log`.
Pour tout enlever : supprimer le dossier `Plugins\ChellesArmes` (rien d'autre n'est modifié).
