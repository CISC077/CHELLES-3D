# ChellesInventaire : menu bibliothèque d'armes cyberpunk

Le menu des armes de FPS_Game (`W_WeaponInventory_UI` et `W_WeaponInventory_UI_Skin_B`) :
- les cadres blancs deviennent cyan néon, les textes blancs cyan avec une lueur magenta,
  les fonds violet sombre, les boutons magenta quand la souris passe dessus ;
- le bouton QUITTER (`CloseInventory_BTN`) ferme vraiment le menu et rend la main au jeu.

Aucun fichier du pack n'est modifié : tout se fait pendant le jeu.

## Installer
1. Fermer Unreal.
2. Double-cliquer sur `Compiler_le_plugin.bat` (1 à 5 minutes).
3. Ouvrir le projet, lancer le jeu, ouvrir le menu des armes.

## Régler
`Inventaire.txt` (Bloc-notes) : couleurs en Rouge,Vert,Bleu de 0 à 255, `Actif=Non` pour revenir au menu d'origine.
Console (touche ²) : `Chelles.Inventaire Relire` recharge les couleurs sans relancer, `Chelles.Inventaire Fermer` ferme le menu.

## Désinstaller
Supprimer le dossier `Plugins\ChellesInventaire`.
