# Invasion zombies (CHELLES 3D)

Toutes les **2 heures de jeu**, une sirène retentit et le message rouge sang
**ATTENTION PURGE ZOMBIE INVASION** dégouline et défile de droite à gauche au
centre de l'écran pendant 30 secondes. Des hordes de zombies apparaissent à
plusieurs endroits autour des joueurs (entre 30 et 70 m) et les poursuivent.

- Un coup de zombie retire **20 %** de la vie du joueur (la vie de FPS_Game).
- Une balle **dans la tête** tue le zombie tout de suite.
- Une balle **dans le corps** lui retire **33,4 %** : 3 balles et il tombe.
- Après 5 minutes, les zombies restants s'effondrent ("FIN DE LA PURGE").
  Si vous les tuez tous avant : "INVASION REPOUSSÉE !".
- En haut de l'écran : nombre de zombies restants et temps avant la fin.

Solo et multijoueur : c'est le serveur qui décide (horaire, zombies, dégâts).

## Installation (une seule fois)

1. Fermez Unreal.
2. Double-cliquez `Compiler_le_plugin.bat` (1 à 5 minutes).
3. Ouvrez le projet, lancez Play.

## Tester tout de suite

En jeu, ouvrez la console (touche **²**) et tapez :

- `Chelles.Zombies` : lance une invasion maintenant
- `Chelles.ZombiesFin` : l'arrête

## Réglages

Ouvrez `Zombies.txt` avec le Bloc-notes (intervalle, nombre de zombies,
dégâts, vitesse, volume de la sirène...). Relancez le jeu, pas besoin de
recompiler.

## Ce qui est utilisé (rien n'est modifié)

- IA et animations : `FPS_Game/Blueprints/Bonus/AI/Zombie/BP_AI_CharZombie`
  (marche zombie + `ZombieAttackMontage`).
- Peau : `DecayedZombieB/Mesh/SK_DecayedZombie_B_wPants` (mêmes os que le
  mannequin, elle suit les animations).
- Sirène : fabriquée par le code (aucun son à importer).

Les réglages de vie/dégâts/vitesse sont appliqués seulement aux zombies créés
pendant l'invasion, jamais aux fichiers du projet.

Journal : `Saved\Logs\FPS_Chelles.log`, lignes `[Zombies]`.

## Utilisé par les modes de survie

Le plugin **ChellesSurvie** (Dernier Refuge) commande les vagues de ce
plugin. Pour lui, l'invasion a reçu : des vagues commandées
(`LancerVagueCommandee`), des événements (zombie tué, zombie touché, coup
sur un joueur) et une pause des invasions automatiques. Le jeu libre ne
change pas. Après une mise à jour, recompilez **ChellesZombies puis
ChellesSurvie**.
