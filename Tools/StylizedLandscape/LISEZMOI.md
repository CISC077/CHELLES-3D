# Sol « Stylized Landscape » + décor de la rotonde (carte Rachid)

## Avant
Lancer `Tools\Sauvegarde\SAUVEGARDER.bat` (conseillé, gros changement visuel).

## Installer
1. Ouvrir le projet dans Unreal 4.27 (n'importe quelle carte).
2. **File > Execute Python Script...** > `Tools\StylizedLandscape\installer_stylized_landscape.py`
3. Répondre **Oui**. Le script :
   - enregistre la carte ouverte et copie `Rachid.umap` dans `Tools\StylizedLandscape\sauvegarde\` ;
   - pose le sol du pack (herbe vert vif sur le plat, roche dans les pentes) sur le Landscape et les dalles `Chelles_Tile_Ground` ;
   - ouvre la démo du pack, copie la rotonde couverte de lierre et tout ce qui l'entoure (40 m) ;
   - rouvre Rachid, cherche un terrain libre et plat (aucune route, bâtiment, trottoir…) près du départ du joueur, y pose le décor + un petit lac entouré de pierres ;
   - fait pousser l'herbe haute et les fleurs du pack **uniquement** dans cette zone ;
   - place la caméra de l'éditeur sur le décor et enregistre.
4. Si l'éclairage paraît bizarre sur le décor : **Build > Build Lighting Only**.

Le décor est dans l'Outliner : dossier **Chelles > DecorStylized**.

## Réglages (en haut du script)
- `ZONE_CENTRE = (x, y)` pour choisir l'endroit soi-même (en cm, coordonnées Unreal).
- `RAYON_COPIE_M` : taille du morceau de démo copié (40 m).
- `MAX_PLANTES` : nombre max d'arbres/buissons/fleurs (900).
- `FAIRE_DALLES_SOL = False` pour laisser les dalles de la ville en sol cartoon.
- Couleurs du sol : Content Browser > Chelles > StylizedLandscape > `MI_SolStylized`.

## Herbe courte animée
Par défaut le script met aussi une herbe de ~10 cm qui bouge au vent sur tout le Landscape, sauf sous les routes, bâtiments, trottoirs et le décor (masque calculé vu du ciel, ~1 minute avec une barre de progression). Réglages : `HAUTEUR_HERBE_CM`, `DENSITE_HERBE`, `DISTANCE_HERBE_M`, ou `FAIRE_HERBE_PARTOUT = False`.

## Annuler
**File > Execute Python Script...** > `annuler_stylized_landscape.py` : ancien sol remis (journal `journal_stylized.json`), objets ajoutés retirés. Rien d'autre n'est touché.

## En cas d'erreur
Lire `Tools\out\stylized_landscape.log`.
