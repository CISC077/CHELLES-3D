# Habillage des bâtiments de Rachid

Les bâtiments simples de Rachid (volumes des tuiles `Chelles_Tile_*`, cubes posés à la main) sont recouverts
par de vrais modèles de maisons et d'immeubles trouvés dans les packs du Content, choisis parce qu'ils ont
presque le même volume (longueur, largeur, hauteur).

**Unreal doit être FERMÉ** (un seul Unreal, jamais deux) avant chaque double-clic.

| Étape | Fichier | Ce qu'il fait |
|---|---|---|
| 1 | `1_ANALYSER.bat` | Fouille les packs, calcule les correspondances. **Ne modifie rien.** Ouvre le rapport à la fin. |
| 2 | `2_POSER.bat` | Copie Rachid dans `sauvegarde\`, puis pose les modèles (dossier **Habillage** de l'Outliner). |
| ↩ | `3_ANNULER.bat` | Enlève tous les modèles posés par l'étape 2. Le reste n'est pas touché. |

Rapports (dans `Rapport\`) :
- `rapport_habillage.txt` : le résumé (combien de bâtiments habillés, avec quel modèle, l'écart en %).
- `modeles_trouves.txt` : toutes les maisons / immeubles trouvés, pack par pack, avec leur taille.
- `correspondances.json` : ce que l'étape 2 va poser.

## Règles du calcul (modifiables en haut de `habillage_commun.py`)
- `TOLERANCE = 0.25` : 25 % d'écart maximum sur la longueur et la largeur.
- `TOLERANCE_HAUTEUR = 0.35` : 35 % sur la hauteur (la hauteur OSM est souvent estimée).
- `REMPLISSAGE_MIN = 0.78` : les formes en L / en U sont laissées telles quelles.
- Le modèle est posé 3 % plus grand que le volume simple pour bien le cacher, sans collision
  (on garde celle du bâtiment simple, le joueur ne sent pas la différence).
- Un bâtiment où un vrai modèle est déjà posé n'est pas touché.
- `poser_habillage.py` : `DISTANCE_AFFICHAGE_M = 350` : au-delà, on revoit le bâtiment simple (jeu plus fluide).
  Mettre 0 pour toujours afficher les modèles.

Les Blueprints de bâtiments (BP_Hotel, BP_building...) ne sont pas utilisés : seulement les modèles 3D (Static Mesh).
Journal : `habillage.log`.
