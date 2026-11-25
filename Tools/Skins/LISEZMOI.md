# Skins et habillage (agent 3, 25/11/2026)

**A lancer : `REPARER_SKINS.bat`** (Unreal fermé, double-clic, attendre « TERMINE »).

Ce qu'il fait :
1. Recompile **ChellesWraith** (touche K, LENA/SOFIA/LUCIA, héros Paragon) et **ChellesMenu** (menu HABILLAGE).
2. Lance `verifier_skins.py` dans Unreal (sans ouvrir l'éditeur) : pour chaque ligne `Skin=` de
   `Plugins\ChellesWraith\Wraith.txt`, vérifie le corps, son squelette, ses matériaux, que les animations
   ont le même squelette (sinon T-pose), ajoute des LOD à LENA/SOFIA/LUCIA s'il n'y en a pas, et supprime
   les corps en double de `Content\Personnages\LUCIA` (s'ils ne servent nulle part).
3. Ouvre `RAPPORT_SKINS.txt` : ce qui a été réparé, ce qui reste à faire.

Changements dans le jeu :
- Les skins se chargent **en arrière-plan** : la touche K et le choix dans HABILLAGE ne figent plus le jeu.
  Tant que le skin n'est pas prêt, on garde l'apparence d'avant (message « chargement... »).
- Les Animation Blueprint Paragon ne sont plus tous chargés au démarrage (seulement le héros choisi) :
  démarrage plus rapide, moins de mémoire. Les skins plus utilisés sont libérés.
- Corps introuvable : soldat d'origine, un seul message, et K saute ce skin (avant : recherche à chaque image).
- Plus d'image en T-pose quand le corps apparaît ; message dans le journal si un squelette ne correspond pas.
- Aperçu HABILLAGE : le matériau d'un perso ne reste plus sur le suivant ; les 3 derniers persos vus restent
  en mémoire tant que le menu est ouvert (plus d'à-coup en revenant dessus).

Annuler : `ANNULER_SKINS.bat` (remet les sources `*.avant_skins`).
