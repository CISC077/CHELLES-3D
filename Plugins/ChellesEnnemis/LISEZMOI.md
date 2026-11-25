# Plugin ChellesEnnemis

Les 5 dossiers `Content\FPS_Game\Blueprints\Bonus\AI\` (Charger, Other, Patrol, Soldier, Zombie) sont les ennemis des joueurs, en solo comme en multijoueur (c'est le serveur qui décide de tout).

## Ce que fait le plugin

1. **À chaque lancement du jeu**, 10 ennemis de chaque dossier (soit 50) sont posés au hasard dans la carte, au niveau de la rue, loin des joueurs. Un ennemi tué est remplacé 90 secondes plus tard par un autre du même dossier. Ceux déjà posés dans la carte restent en plus.
2. **Dossier Other** : tout personnage (Blueprint) déposé dans `Content\FPS_Game\Blueprints\Bonus\AI\Other` devient tout seul un ennemi des joueurs. Le plugin le dirige : il repère le joueur le plus proche (40 m), le poursuit, le frappe (15 % de vie par coup) et tombe quand on le tue (4 balles dans le corps, ou 1 dans la tête). Il cherche lui-même une animation d'attaque dans le Blueprint.
3. **Vêtements** : les ennemis qui ont encore le mannequin gris (SK_Mannequin) sont habillés pendant le jeu : bandits de couleur pour Soldier, Charger et Patrol (mêmes couleurs que dans la carte), vrai zombie DecayedZombieB pour Zombie. Les fichiers du pack ne sont pas modifiés.

## Exemple : ajouter un nouveau zombie

1. Mettez le pack (ex. `ZOMBIE 2`) dans `Content` avec l'Explorateur ou le Launcher.
2. Dans Unreal, faites un **Blueprint enfant** du personnage du pack (clic droit sur le Blueprint > *Create Child Blueprint Class*), par exemple `BP_ZOMBIE 3 Child`.
3. **Glissez ce Blueprint enfant dans le dossier `FPS_Game\Blueprints\Bonus\AI\Other`** (choisissez *Move Here*).
4. Lancez Play : 10 de ces zombies sont dans la ville et attaquent les joueurs.

Un Blueprint qui n'est pas un personnage (tonneau, feu...) est ignoré : `BP_RedBarrel`, `BP_FireHazard` et `BP_FireDamage` restent ce qu'ils sont.

## Réglages

Tout se règle dans `Plugins\ChellesEnnemis\Ennemis.txt` avec le Bloc-notes (nombre par dossier, réapparition, vie, dégâts, couleurs des tenues...). Pas besoin de recompiler : relancez le jeu.

## Pour tester

Console du jeu (touche ²) :

- `Chelles.Ennemis` : affiche les ennemis trouvés dans chaque dossier et combien sont en vie.
- `Chelles.EnnemisPlus` : pose encore 10 ennemis de chaque dossier.

Le journal `Saved\Logs\FPS_Chelles.log` contient les lignes `[Ennemis]`.

## Installation

Unreal fermé, double-cliquez sur `Tools\EnnemisAuto\INSTALLER_ENNEMIS.bat`.

Pour le serveur multijoueur du VPS, il faudra refaire la compilation du serveur Linux (Tools\ServeurVPS) pour qu'il ait aussi ce plugin.


## 5 bots en combinaison (25/11/2026)

A chaque partie, le serveur (ou le jeu solo) pose 5 bots ennemis en combinaison Sci-Fi au hasard dans la carte,
loin des joueurs. Ce sont des soldats FPS_Game (AI_Soldier) habilles de la combinaison de leur couleur
(memes fichiers que la touche K du joueur : rien n'est modifie dans le pack SciFiSoldier).

| Bot | Couleur | Arme | Pouvoir |
|---|---|---|---|
| SPECTRE | noir | rafales laser | teleportation dans le dos du joueur |
| BRASIER | rouge | lance-flammes | pluie de boules de feu |
| FOUDRE | jaune | fusil a eclairs | foudre du ciel (1 s pour s'ecarter) |
| GIVRE | bleu | canon a glace (ralentit) | gel du joueur 2,5 s |
| TITAN | kaki | lance-roquettes | pique du ciel + onde de choc |

- Ils volent tous (flammes sous les pieds), tournent autour du joueur en l'air puis se reposent.
- Vie x3 par rapport a un soldat, reviennent 2 minutes apres leur mort.
- Optimise : un bot sans joueur a moins de 150 m ne calcule plus rien.
- Reglages : `Bots.txt` (Bloc-notes, pas besoin de recompiler).
- Console (touche ²) : `Chelles.Bots` (liste), `Chelles.Bots Plus` (encore 5 bots), `Chelles.Bots Relire`.
- Code : `Source/ChellesEnnemis/Public/BotsCombinaison.h` et `Private/BotsCombinaison.cpp`.
