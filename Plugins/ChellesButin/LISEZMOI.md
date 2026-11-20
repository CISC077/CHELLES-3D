# Chelles Butin (plugin ChellesButin)

Ce plugin mélange des idées de Fortnite, Apex Legends et Call of Duty. Il s'ajoute tout seul au Play : rien à poser dans la carte, aucun Blueprint ni autre plugin modifié.

## Coffres de butin (Fortnite / Apex)

Des coffres apparaissent autour de chaque joueur, entre 25 et 120 m, avec une **colonne de lumière** qu'on voit de loin.

| Couleur | Coffre |
|---|---|
| Bleu | COFFRE |
| Violet | RARE |
| Or | LÉGENDAIRE |

Approche-toi et appuie sur **E** pour l'ouvrir. Tu gagnes, selon le coffre :
- de l'**XP**, comptée par ChellesPouvoirs ;
- la **vie au maximum** ;
- des munitions de **mitrailleuse lourde** ou de **lance-flammes**, pour les armes lourdes de ChellesArmes. Ensuite, touche 3 pour les prendre en main.

Le coffre ouvert montre ses pièces d'or puis disparaît. Les coffres trop loin de tout le monde sont retirés, pour garder le jeu fluide.

## Ping (Apex)

Vise et appuie sur **J** : un repère coloré, avec ton nom, apparaît pour **tous les joueurs** pendant 8 s :
- ICI (bleu) ;
- ENNEMI ! (rouge) ;
- BUTIN (or) ;
- VÉHICULE (vert).

## Séries d'éliminations (Call of Duty)

Compte les ennemis et zombies éliminés à la suite **sans mourir** :

| Série | Récompense |
|---|---|
| 3 | **RADAR** : les ennemis proches sont marqués en rouge à travers les murs, pendant 20 s. |
| 5 | **FRAPPE AÉRIENNE** : vise un endroit et appuie sur la touche **4**. 6 bombes tombent et projettent les ennemis. Ni toi ni les autres joueurs n'êtes touchés si le PVP est coupé. |
| 7 | **+150 XP** et radar. |

## Réglages

Tout se règle dans `Butin.txt` (Bloc-notes, sans recompiler) : touches, distances, chances, XP, munitions, modèles du coffre.

## Console (touche ²)

- `Chelles.Butin` : l'état ;
- `Chelles.Butin Relire` : relit Butin.txt ;
- `Chelles.Butin Coffre` : un coffre légendaire devant toi, pour tester ;
- `Chelles.Butin Serie 5` : ajoute 5 éliminations, pour tester la frappe aérienne.

## Installer

Ferme Unreal, puis double-clique sur `Compiler_le_plugin.bat` dans ce dossier. Si une erreur s'affiche, envoie `Journal_compilation.txt` à Claude.

Le modèle du coffre vient du pack **Low_Poly_Viking_World** (Treasure_Chest et Currency). Les dossiers sont ajoutés à l'export dans `Config\DefaultGame.ini`. Sans ce pack, une simple boîte colorée est affichée.
