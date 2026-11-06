# Trottoirs le long des routes (CHELLES 3D, Unreal 4.27)

`ajouter_trottoirs.py` pose des trottoirs le long des routes OpenStreetMap du quartier
(les acteurs `Chelles_Tile_Road_*` créés par `chelles_complet.py`).

- Routes : un trottoir de **chaque côté**, 2 m de large, bordure de 15 cm.
- Ronds-points : trottoir **seulement sur le bord extérieur**, rien sur l'îlot central.
- Carrefours : le trottoir s'arrête au bord de la route qui croise (passage libre).
- Matériau : `Road_Inst`, trouvé automatiquement dans le Content. Le script crée
  `/Game/Chelles/Trottoirs/MI_Trottoir_Road`, une copie « visible des deux côtés » de
  `Road_Inst` ; `Road_Inst` lui-même n'est pas modifié.

## Utilisation

1. Ouvrir le niveau du quartier (celui qui contient les routes `Chelles_Tile_Road_*`).
2. **File > Execute Python Script...** > `Tools\Trottoirs\ajouter_trottoirs.py`.
3. Journal : `Tools\out\ajouter_trottoirs.log`.

Avant toute modification, le fichier du niveau est copié dans
`Saved\Sauvegardes\trottoirs_<date>\`. Le script peut être relancé : il remplace ses propres
trottoirs. Pour les enlever : `retirer_trottoirs.py`.

## Ce qui n'est pas touché

Routes, bâtiments, Landscape, décor, circulation (`ChellesCirculation`) et passants
(`ChellesPassants`). Les voitures suivent l'axe des rues, où il n'y a jamais de trottoir ; les
passants et le joueur montent la bordure de 15 cm sans problème (le NavMesh se recalcule tout seul).

## Réglages (en haut du script)

`LARGEUR_TROTTOIR` (2 m), `HAUTEUR_TROTTOIR` (0,15 m), `UV_METRES` (taille de la texture, 2 m).
