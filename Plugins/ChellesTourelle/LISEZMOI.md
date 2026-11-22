# Plugin ChellesTourelle - tourelle BP_Turet2026

## Ce que ça fait
- **BP_Turet2026** (dossier `Content\ScifiGun`) : glissez-la dans la carte, elle tourne toute seule vers
  l'ennemi le plus proche (30 m) et lui tire dessus : bandits FPS_Game, ennemis ChellesEnnemis, zombies de l'invasion.
  Elle ne vise jamais les joueurs, passants, policiers ni marchands.
- **Cadeau 50 minutes** : chaque joueur qui a joué 50 minutes (temps total, gardé sur son PC) reçoit la tourelle.
  Un message s'affiche : « CADEAU ! ... la TOURELLE TURET 2026 est dans votre inventaire (touche I) ».
- **Inventaire I** : quand le menu des armes est ouvert, une case néon **TOURELLE TURET 2026** s'affiche à droite.
  - avant 50 min : temps restant + barre de progression ;
  - **POSER** : la tourelle est posée 2,5 m devant vous et tire sur les ennemis ;
  - **RAMASSER** : elle revient dans l'inventaire.
- Multijoueur : le serveur pose les tourelles et applique les dégâts, tout le monde les voit tirer.

## Installation (une fois)
Unreal FERMÉ, double-clic sur `Tools\Tourelle\INSTALLER_TOURELLE.bat` :
1. compile le plugin ;
2. ajoute `/Game/ScifiGun` à la liste d'export (DefaultGame.ini, copie faite avant) ;
3. crée `BP_Turet2026` dans ScifiGun.

## Réglages
`Plugins\ChellesTourelle\Tourelle.txt` (Bloc-notes, sans recompiler) : portée, cadence, dégâts,
minutes avant le cadeau, nombre de tourelles par joueur, place de la case...
Dans BP_Turet2026 > Class Defaults > Tourelle : modèle, Rotation Modele (180 si elle tire de dos), hauteur de l'axe, bout du canon.

## Pour tester
Console (touche ²) :
- `Chelles.Tourelle Temps 50` : fait comme si vous aviez joué 50 minutes (le cadeau arrive) ;
- `Chelles.Tourelle Poser` / `Chelles.Tourelle Ramasser` ;
- `Chelles.Tourelle Info` : état dans le journal (lignes `[Tourelle]`) ;
- `Chelles.Tourelle Temps 0` : remet le temps à zéro.

Aucune touche du clavier n'est prise (ToucheRapide vide par défaut).
