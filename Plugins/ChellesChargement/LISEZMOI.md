# ChellesChargement — écran de chargement CHELLES 3D (25/11/2026)

Écran aux couleurs du lobby (bleu nuit, bleu roi, jaune, plaques penchées), disposition
inversée : la barre de chargement est **verticale** à gauche.
Il affiche une astuce de touche qui change toutes les 4 secondes.

## Quand il s'affiche
- **Jeu final** (démo, jeu complet) : pendant l'ouverture de la carte (lancement, SOLO,
  rejoindre un serveur). C'est le *MoviePlayer* d'Unreal : il ne marche pas dans l'éditeur.
- **Début de partie**, jeu final **et** bouton Jouer de l'éditeur : l'écran reste par-dessus
  le jeu le temps que les textures arrivent (2,5 s minimum, 10 s maximum), puis disparaît en fondu.

## Modifier les astuces
`Astuces.txt` (Bloc-notes) : une astuce par ligne, `TOUCHE | texte`. Pas besoin de recompiler.
Dans le jeu empaqueté, si le fichier n'est pas copié, les mêmes astuces intégrées sont utilisées.

## Couper
`DebutPartie=0` dans `Astuces.txt` = plus d'écran au bouton Jouer. Pour tout retirer :
supprimer le dossier `Plugins\ChellesChargement` (rien d'autre n'en dépend).

## Compiler
`Compiler_le_plugin.bat` (Unreal fermé), ou `Tools\INSTALLER_SAUVEGARDE_AUTO_ET_CHARGEMENT.bat`.
