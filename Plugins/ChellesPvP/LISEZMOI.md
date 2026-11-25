# ChellesPvP : combats entre joueurs avec kit de survie

## Ce que fait le plugin
- Le PvP se joue en **manches** (10 min par défaut), suivies de 12 s de trêve avec le tableau des scores.
- Un joueur qui reçoit **5 coups** d'autres joueurs (tirs, explosions, coups de couteau) tombe **À TERRE** :
  il ne meurt pas, il ne peut plus bouger ni tirer, mais il peut regarder autour de lui.
- Un ami lui **jette un kit de survie** avec la touche **Y** : en le visant, le kit part tout seul en cloche
  vers lui. Le kit qui tombe à moins de 2,5 m le **réanime automatiquement** (50 % de vie).
- Personne ne l'aide en 30 s : il est **éliminé** (mort normale de FPS_Game, puis réapparition).
- Chaque joueur a 2 kits de survie par manche (5 au maximum).
- Bandeau en haut de l'écran : manche, temps restant, coups reçus, kits restants.
- Scores : éliminations, mises à terre, réanimations. Le gagnant est annoncé en fin de manche.
- Pas de PvP pendant le Dernier Refuge (ChellesSurvie), ni si le PVP est coupé dans le menu de l'hôte.

Tout se règle dans **PvP.txt** (Bloc-notes). Le serveur décide de tout : tout est répliqué en multijoueur.

## Compiler
Fermez Unreal, double-cliquez **Compiler_le_plugin.bat**, attendez « TERMINÉ », rouvrez le projet.

## Tester tout seul ou à deux
- Play > Number of Players = 2, Net Mode = Play As Listen Server.
- Console (touche ²) : `Chelles.PvP` (état), `Chelles.PvPATerre` (l'hôte se met à terre),
  `Chelles.PvPKits 3` (3 kits à chacun), `Chelles.PvPManche` (relit PvP.txt et relance une manche).
- Pour tester seul la manche et le bandeau : `JoueursMinimum = 1` dans PvP.txt.

## Pour les autres plugins
- Tout acteur portant le tag `ChellesKitSurvie` qui arrive près d'un joueur à terre le réanime.
- `APvPChelles::AjouterKitsSurvie(APawn* Joueur, int32 Nombre)` (UFUNCTION, serveur) : donne des kits,
  par exemple depuis un coffre de butin. `EstATerre(APawn*)` dit si un joueur est à terre.
- Le joueur à terre porte le tag `ChellesHorsJeu` (comme dans ChellesSurvie).

## Comment ça marche (technique)
Le serveur écoute les dégâts des personnages FPS_Game. Pour un coup venu d'un autre joueur, il met la vie
très haut juste avant le calcul de FPS_Game (OnTakePointDamage / OnTakeRadialDamage passent avant
l'événement AnyDamage du Blueprint), puis remet la vie d'avant juste après : FPS_Game ne tue jamais par un
coup PvP, c'est le plugin qui compte les coups. Aucun Blueprint ni asset n'est modifié.
