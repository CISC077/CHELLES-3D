# Végétation, rivière et lac d'après OpenStreetMap (carte Rachid)

## Une seule chose à faire

1. **Fermez Unreal.**
2. Double-cliquez sur **`INSTALLER_VEGETATION_EAU.bat`**.
3. Attendez « TERMINE » (compilation 1 à 5 min la première fois, puis 5 à 15 min de pose).

Ouvrez ensuite le projet : tout est rangé dans le dossier **Chelles/VegetationEau** du World Outliner.

## Ce qui est posé

| Quoi | Comment |
|---|---|
| Arbres | arbres réels d'OpenStreetMap + alignements + parcs/bois + jardins |
| Haies | haies OSM + haies de jardin le long des rues (avec entrées de garage), pas dans le centre-ville |
| Buissons, fleurs | parcs, espaces verts, jardins |
| Rivière | traverse toute la carte d'un bord à l'autre, reste à plus de 300 m de la Mairie, évite les maisons (elle se resserre entre elles), passe **sous** les rues (garde-corps en bois de chaque côté) |
| Petit lac | forme naturelle, à côté des ruines médiévales ; l'ancien lac rond est caché (pas supprimé) |
| Berges | bordure de pierre, roseaux, pierres, saules |

## Fluidité

- Toutes les plantes sont des **instances** (plugin `ChellesVegetation`, HISM) : des milliers d'arbres coûtent comme quelques objets.
- Disparition au loin : arbres 300 m, haies 150 m, buissons 120 m, fleurs/roseaux 45 m.
- Ombres seulement pour les arbres ; LOD automatiques ajoutés aux modèles qui n'en avaient pas.
- Les anciens arbres/haies posés un par un (`CH_tree_*`, `CH_bush_*`, `CH_hedge_*`) sont regroupés en instances eux aussi (originaux cachés).
- Rien ne touche au NavMesh. L'eau est un matériau opaque simple (pas de réfraction).

## Fichiers

- `vegetation_eau.log` : journal (à envoyer à Claude en cas de souci) ; `compilation.log` si la compilation échoue.
- `carte_controle.bmp` : la carte vue du ciel (gris = bâti, noir = routes, bleu = eau, vert = plantes, cercle rouge = centre-ville).
- `Sauvegardes\<date>\` : copie de `Rachid.umap` avant chaque passage.
- Relancer l'installation remplace seulement ce qu'elle avait posé.

## Annuler

Unreal fermé, double-clic sur **`ANNULER_VEGETATION_EAU.bat`** : tout est enlevé, ce qui avait été caché réapparaît.

## Réglages (en haut de `vegetation_eau.py`)

`RAYON_CENTRE_VILLE_M` (300), `LARGEUR_RIVIERE_M` (12), `LAC_RAYON_M` (16), densités `JARDIN_M2_PAR_ARBRE`…, maxima `MAX`, distances d'affichage `CATEGORIES`, `FAIRE_RIVIERE` / `FAIRE_LAC` / `FAIRE_VEGETATION`.
