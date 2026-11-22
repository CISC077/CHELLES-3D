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

## 3. ARSENAL (2026-11-17) : blaster, bazooka, sabre lumineux, grenade à zombies

Touche **N** (à pied) : ARME NORMALE → **BLASTER** → **BAZOOKA** → **SABRE LUMINEUX** → **GRENADE À ZOMBIES** → ARME NORMALE.
Pendant qu'une arme de l'arsenal est en main, l'arme du kit FPS_Game est cachée et son tir est bloqué.
**Molette** = retour aux armes normales. Le nom de l'arme et les touches s'affichent en bas de l'écran.

- **BLASTER** : maintenir le clic gauche = rafale de traits lumineux, son « piou-piou » de science-fiction.
- **BAZOOKA** : clic gauche = fusée avec traînée de fumée. **Pendant son vol elle suit ton viseur** :
  bouge la souris, la fusée tourne. Elle explose au contact (ou au bout de 8 s) : dégâts autour,
  les ennemis et zombies sont **projetés en l'air**, les objets physiques poussés.
- **SABRE LUMINEUX** : clic gauche = coup au corps-à-corps (la lame s'allume et bourdonne).
  Un coup **tranche un zombie** ; les autres ennemis prennent des dégâts et reculent.
- **GRENADE À ZOMBIES** : clic gauche = lancer. Elle se pose, **bipe et attire les zombies** à 30 m
  pendant 5 s (ils foncent dessus au lieu de toi), puis explose.

Tous les sons sont fabriqués par le code (aucun fichier à importer). Les armes en main sont dessinées
avec des formes simples ; pour des lames et traits **qui brillent**, lancer une fois
`Tools\Arsenal\creer_materiau_lueur.py` (Fichier > Exécuter un script Python).

Réglages (dégâts, couleurs du sabre et du blaster, taille d'explosion, durée d'appât...) :
`Plugins\ChellesArmes\Arsenal.txt`. Console : `Chelles.Arsenal Sabre` (ou Blaster, Bazooka, Grenade, Aucune, Recharger).

Multijoueur : c'est le serveur qui crée les tirs et les explosions, tout le monde les voit ; le PVP du menu est respecté.
L'arme tenue en main n'est dessinée que chez son joueur.
La grenade a besoin du plugin **ChellesZombies** recompilé (les zombies y suivent l'appât).

## 4. ARMES LOURDES AU MUR + PUISSANCE SELON L'XP (2026-11-20)

Au lancement, le serveur accroche jusqu'à 10 **râteliers** aux murs des bâtiments de Rachid,
au-dessus des socles d'armes posés par `Tools\Bonus`. Ce sont des **cadres en bois** avec une arme lourde
du pack AAAFantasticWeapons et une étiquette qui donne son nom et l'XP nécessaire.

- Approche-toi et appuie sur **E** : tu prends l'arme avec ses munitions, elle passe en main tout de suite.
  Le cadre reste vide 90 s, puis l'arme revient.
- **LANCE-FLAMMES** (débloqué à **250 XP**, niveau 3) : maintiens le clic gauche pour cracher un jet de feu
  de 9 m, avec le son du feu. Il brûle les zombies et les ennemis dans le jet, mais pas à travers les murs.
- **MITRAILLEUSE LOURDE** (dès 0 XP) : rafale rapide de balles jaunes, canons qui tournent.
- Le nombre de munitions s'affiche en bas de l'écran. À 0, on revient à l'arme normale.
  Touche **3** pour reprendre une arme lourde qui a encore des munitions.
- **PUISSANCE** : chaque niveau d'XP ajoute **+10 % de dégâts** à toutes les armes de l'arsenal
  (niveau 6 = +50 %). Le bonus s'affiche à côté du nom de l'arme.

L'XP vient du plugin **ChellesPouvoirs**. Il est lu par son nom, sans lien de compilation : s'il manque,
l'XP vaut 0. Réglages dans `Arsenal.txt` (section ARMES LOURDES). Pour tester en solo, tape dans la console
`Chelles.Arsenal Remplir` puis `Chelles.Arsenal LanceFlammes`.

## Installer

1. Fermer Unreal.
2. Double-cliquer `Plugins\ChellesArmes\Compiler_le_plugin.bat`.
3. Ouvrir le projet. (Une fois) Fichier > Exécuter un script Python > `Tools\Armes\importer_armes_fpsassets.py`.
4. Play.

Journal : lignes `[Armes]` dans `Saved\Logs\FPS_Chelles.log`.
Pour tout enlever : supprimer le dossier `Plugins\ChellesArmes` (rien d'autre n'est modifié).

## Mêmes armes pour tous (22/11/2026)

Dans `Armes.txt`, `Heros=Tous` et `HerosFlash=Tous` : tous les personnages (soldat, Spider-Male, Spider-Femelle, PANTERA, persos des packs) ont le FLASH ÉLECTRIQUE (touche X) et, sans arme en main, tirent depuis leurs mains (clic gauche, O = changer de tir). Les armes du soldat restent celles de la roue TAB. Remettre les anciennes listes (en commentaire juste au-dessus) pour revenir en arrière.
