# Modes de survie (CHELLES 3D)

Premier mode : **DERNIER REFUGE**. Les joueurs défendent le parc de la Mairie
contre des vagues de zombies sans fin, de plus en plus dures. Solo et
multijoueur (1 à 8 joueurs, c'est l'hôte qui décide).

- Chaque balle qui touche un zombie rapporte des **points**, un zombie tué
  plus encore, une balle dans la tête encore plus.
- Les points s'achètent aux **points d'achat** (touche **E**) :
  - bloc **cyan** = une arme (elle apparaît devant le bloc, ramassez-la avec E) ;
  - bloc **orange** = munitions pleines pour toutes vos armes ;
  - mur **rouge** = une barrière : payez pour l'ouvrir et passer.
- Un coup qui devrait vous tuer vous met **à terre** pendant 30 secondes.
  Un coéquipier vous relève en restant appuyé sur **E** près de vous (3 s).
  Sinon vous êtes **mort jusqu'à la vague suivante**.
- Plus personne debout : **LE REFUGE EST TOMBÉ**, tableau des scores, puis
  retour au jeu libre.
- Chaque zombie tué donne aussi de l'**XP** pour les pouvoirs (dans tous les modes).

Les autres modes (Rivalité de quartier, Traversée de Chelles, Contagion)
sont affichés "Bientôt" dans l'écran MODES.

## Installation (une seule fois)

Ce plugin pilote les zombies du plugin **ChellesZombies**, qui a reçu un
petit ajout. Il faut donc compiler **les deux** :

1. Fermez Unreal.
2. Double-cliquez `Plugins\ChellesZombies\Compiler_le_plugin.bat` et attendez la fin.
3. Double-cliquez `Plugins\ChellesSurvie\Compiler_le_plugin.bat` et attendez la fin.
4. Ouvrez le projet, lancez Play.

## Lancer le mode

Deux secondes après le début du jeu (après le menu SOLO / Multi-Player),
l'écran **MODES** s'ouvre une fois tout seul. Cliquez **DERNIER REFUGE**.
Vous pouvez choisir la vague de départ avec **−** et **+**.

Ensuite, **F7** rouvre l'écran MODES à tout moment (JEU LIBRE = arrêter).

En multijoueur, seul l'hôte choisit le mode : les autres voient l'écran
mais les boutons sont grisés.

## Placer la zone au parc de la Mairie (une seule fois)

Au premier lancement, si rien n'est enregistré, la zone se met là où se
trouve l'hôte. Pour la mettre au bon endroit :

1. En jeu, marchez jusqu'au milieu du parc de la Mairie.
2. Ouvrez la console (touche **²**) et tapez `Chelles.RefugeIci` puis Entrée.

Pour placer vos propres points d'achat (sinon 4 points sont posés
automatiquement autour du centre) : placez-vous, regardez vers l'endroit
voulu (le point se met 1,5 m devant vous) et tapez par exemple :

- `Chelles.StandIci Arme Fusil` (noms des armes dans `Refuge.txt`)
- `Chelles.StandIci Munitions`
- `Chelles.StandIci Barriere 750` (un mur en travers de la rue, prix 750)
- `Chelles.StandRetirer` retire le point le plus proche (moins de 5 m)

Tout est enregistré dans `Refuge_Positions.txt` (modifiable au Bloc-notes).

## Commandes pour tester (console, touche ²)

- `Chelles.Modes` : ouvre l'écran MODES (comme F7)
- `Chelles.Points 5000` : donne 5000 points
- `Chelles.Vague 10` : saute à la vague 10
- `Chelles.RefugeFin` : arrête le mode, retour au jeu libre

## Réglages (Bloc-notes, pas besoin de recompiler)

- `Vagues.txt` : nombre de zombies, vie, vitesse, costauds, vagues de coureurs, pause.
- `Refuge.txt` : points, prix, liste des armes, durée à terre, distance d'apparition.

Relancez le mode (F7) après avoir enregistré.

## Bon à savoir

- Pendant le Dernier Refuge, l'invasion automatique toutes les 2 h est en
  pause. Elle reprend en jeu libre.
- Jamais plus de `NombreMaxZombies` (fichier `Zombies.txt`) zombies en
  même temps : les autres arrivent quand il y a de la place.
- Les points du mode sont à part de l'argent de FPS_Game.
- Rien n'est modifié dans les Blueprints ni les assets du projet.

## Tests automatiques

Dans Unreal : **Window > Test Automation**, cochez **Chelles > Survie**,
puis **Start Tests**. Ils vérifient les calculs des vagues, des points et
la lecture des fichiers de réglages.
