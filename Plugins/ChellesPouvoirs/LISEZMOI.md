# Chelles Pouvoirs et XP

Le joueur gagne de l'XP en jouant et débloque des pouvoirs. Rien à poser dans la carte : le plugin s'ajoute tout seul au lancement (solo et multijoueur).

## Installer

1. Fermer Unreal.
2. Double-cliquer `Compiler_le_plugin.bat` dans ce dossier (1 à 5 minutes).
3. Ouvrir le projet et lancer la carte Rachid.

## XP (rythme rapide)

10 XP par minute de jeu (pas en pause, pas dans le menu ni le chat), +50 XP à chaque heure complète.
L'XP est sauvegardée sur le PC du joueur : `Saved\SaveGames\ChellesXP.sav`.

| Niveau | Titre | XP | Temps de jeu | Pouvoir débloqué |
|---|---|---|---|---|
| 1 | Habitant | 0 | départ | — |
| 2 | Voisin | 100 | 10 min | ENVOL : voler 20 s (ESPACE monte, CTRL ou C descend), recharge 45 s |
| 3 | Gardien du quartier | 250 | 25 min | BOULE DE FEU : explose à l'impact, recharge 3 s |
| 4 | Fantôme de Chelles | 500 | 50 min | EFFET VERRE : invisible pour les autres 10 s, recharge 60 s |
| 5 | Caméléon | 800 | 1 h 20 | CAMÉLÉON : prendre l'apparence d'un autre personnage (J encore = redevenir soi) |
| 6 | Légende de Chelles | 1200 | 2 h | GEL : geler 4 s le joueur ou le passant visé (30 m), recharge 30 s |

Pour changer les paliers ou les durées : `Source\ChellesPouvoirs\Public\PouvoirsTypes.h`, puis recompiler.

## Touches

- **J** : utiliser le pouvoir choisi
- **Y** : passer au pouvoir suivant

## Tester sans attendre 2 heures

En jeu, ouvrir la console (touche **²** sous Échap) et taper `Chelles.XP 1200` puis Entrée : tout est débloqué. `Chelles.XP -1200` pour revenir en arrière.

## Multijoueur

C'est le serveur (celui qui héberge) qui lance les boules de feu et décide du gel, tout le monde voit la même chose. Si l'hôte a coupé le PVP dans les options, les boules de feu ne blessent pas les autres joueurs et le gel ne marche que sur les passants.
