# Agent Terrain (25/11/2026)

## Ce que Rachid doit faire (une seule fois)
1. Fermer Unreal.
2. Double-cliquer `Tools\Terrain\OPTIMISER_TERRAIN.bat`.
3. Taper `O` puis Entrée (retirer les objets en double), attendre « TERMINE ».

## Ce qui change dans le jeu
- **MENU > OPTIONS > AVANCÉS** (nouvel onglet) : 22 réglages un par un, enregistrés tout seuls :
  ombres, distance des ombres, reflets, eau et verre, effets / fluides, brouillard 3D, ombres douces,
  post-process, halo des lumières, flou de mouvement, flou de fond, distance de vue, herbe / plantes,
  détail des objets, détail des persos, textures, matériaux, anti-crénelage, reflets du soleil,
  netteté de l'image (50 à 100 %), compteur FPS.
  `AUTO` = suit la QUALITÉ BAS / MOYEN / HAUT de l'onglet GRAPHISMES. Changer la QUALITÉ remet tout en AUTO.
  Bouton **DÉTECTER MON PC** : Unreal teste la machine et choisit les bons réglages.
- **COMPTEUR FPS** : en haut à gauche (vert ≥ 50, jaune ≥ 30, rouge en dessous).
- **RACCOURCIS CLAVIER** (onglet CONTRÔLES) : en plus des touches du kit FPS, les touches des modes CHELLES
  (POUVOIR P, DANSER J, ARMEMENTS I, ROUE TAB, UTILISER E, VÉHICULE F, APPARENCE K, TIR SPÉCIAL O,
  FLASH X, CONSOLE ²...). Clic = changer, clic droit = touche d'origine. Une touche déjà prise est refusée.

## Objets en double
Le script `doublons_rachid.py` retire les acteurs identiques posés au même endroit (même objet, même
rotation, même taille). Il ne touche pas à ce que pose le fil végétation / rivière / lac
(tags `ChellesVegEau`, `ChellesVegEauCache`, CH_tree/bush/hedge, Stylized_Lac...), ni aux objets cachés.
- Copie de Rachid avant : `_Sauvegardes\avant_doublons_<date>` — pour revenir : `RESTAURER_DOUBLONS.bat`.
- Liste des objets retirés : `Tools\Terrain\Rapport\doublons_retires_<date>.json`.
- Fichiers en double dans Content (même nom + même taille) : `Tools\Terrain\Rapport\doublons_fichiers.txt`
  (rapport seulement, rien n'est supprimé).
- Pour qu'un objet ne soit jamais retiré : lui ajouter le tag `ChellesGarder`.
