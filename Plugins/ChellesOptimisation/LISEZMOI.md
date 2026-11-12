# Chelles Optimisation

Plugin séparé : il ne modifie aucun fichier du projet, aucune map, aucun autre plugin.
Tout se fait en mémoire au moment où on clique sur Play (ou au lancement du jeu).

## Ce qu'il fait

1. **Neige seulement autour du joueur**
   - La grande couche « lointaine » (flocons jusqu'à 150 m autour et 400 m de haut) est coupée.
   - Seule la couche qui suit la caméra reste, agrandie de 40 m à 60 m, avec la même densité de flocons.
   - Le réglage nuit / qualité du plugin ChellesNeige continue de marcher.

2. **Fleurs et herbe seulement près du joueur**
   - Foliage peint (fleurs, herbe, petites plantes de moins de 3 m) : affiché jusqu'à 40 m, avec un fondu à partir de 30 m.
   - Herbe automatique du Landscape : pareil.
   - Plantes posées une par une (nom contenant flower, plant, grass, bush…) : affichées jusqu'à 40 m.
   - Les arbres ne changent pas.

3. **PNJ au-delà de 100 m**
   - Passants, ennemis (bandits), zombies et autres personnages non joueurs : invisibles au-delà de 100 m.
   - Leur animation s'arrête. Le serveur (ou le jeu en solo) met aussi en pause leur déplacement et leur IA
     quand ils sont à plus de 100 m de **tous** les joueurs.
   - Ils reviennent tout seuls dès qu'on se rapproche.
   - Les zombies ne sont jamais mis en pause (ils doivent pouvoir vous rejoindre), ils sont seulement cachés de loin.
   - Les passants gardent leur propre pause, réglée à la même distance.
   - Les joueurs (vous, les autres joueurs en multi, le cheval ou la motoneige que vous pilotez) ne sont jamais touchés.

## Multijoueur

Chaque machine cache ce qui est loin de **sa** caméra : rien n'est envoyé sur le réseau.
Seul le serveur décide des pauses d'IA, en regardant la distance à tous les joueurs.

## Réglages

`Optimisation.txt` (Bloc-notes) : distances, et 1/0 pour activer chaque partie. Relu à chaque Play.
`Actif=0` désactive tout le plugin.

## Installation

1. Fermez Unreal.
2. Double-cliquez sur `Compiler_le_plugin.bat` (1 à 5 minutes).
3. Rouvrez le projet, lancez Play sur Rachid.
4. Dans `Saved\Logs\FPS_Chelles.log`, les lignes `[Optim]` disent ce qui a été fait.

Pour retirer le plugin : supprimez le dossier `Plugins\ChellesOptimisation` (Unreal fermé).
