# Circulation de Chelles (plugin)

Ce plugin fait rouler des voitures dans les rues de la ville, comme le plugin
« Chelles Passants » fait marcher des piétons. Il est **entièrement séparé** du reste du
projet : il ne modifie aucun niveau, aucun Blueprint, aucun réglage, et ne touche pas aux
scripts du dossier `Tools`. Tant que vous ne posez pas l'acteur « Circulation Spawner »
dans un niveau, il ne se passe rien.

## Ce que font les voitures

- Elles suivent les **vraies rues de Chelles** : le plugin lit le fichier OpenStreetMap qui
  a servi à construire la ville (`Tools/out/osm_ways.json`), avec les mêmes types de rues
  et le même repère que `chelles_complet.py`. Elles roulent donc pile sur les routes 3D.
- Elles **roulent à droite** sur les rues à double sens et respectent les **sens uniques**.
- Elles respectent la **limitation de vitesse** de chaque rue (50, 30, 20 km/h…).
- Elles **ralentissent dans les virages** et prennent des virages arrondis.
- Elles gardent leurs **distances** avec la voiture de devant (et font la queue).
- Aux **carrefours**, une seule direction passe à la fois : les voitures d'une même rue
  passent à la suite, celles de la rue d'en face attendent leur tour.
- Elles **s'arrêtent devant les piétons et devant le joueur** (et klaxonnent si vous
  ajoutez un son).
- Dans une impasse, elles font **demi-tour**.
- Les voitures trop loin du joueur sont **ramenées près de lui**, toujours hors de sa vue :
  la ville paraît animée là où vous êtes, sans avoir besoin de 500 voitures.
- Le joueur ne traverse pas les voitures, et les balles les touchent.

## Installation (une seule fois)

1. Fermez Unreal.
2. Double-cliquez sur `Plugins\ChellesCirculation\Compiler_le_plugin.bat` (1 à 5 minutes,
   jusqu'à « TERMINÉ »). Il crée seulement le dossier `Binaries` du plugin.
3. Ouvrez le projet. Pour vérifier : *Edit > Plugins*, catégorie « Chelles 3D » :
   « Chelles Circulation » doit être coché.

## Mettre la circulation dans la ville

1. Dans le *Content Browser*, cochez *View Options* > **Show Plugin Content** et
   **Show C++ Classes**, ouvrez `C++ Classes > ChellesCirculation`, et faites glisser
   **Circulation Spawner (Chelles)** n'importe où sur une route.
2. Sélectionnez-le. Dans *Details*, catégorie **Circulation > Reseau**, cliquez sur
   **Construire le reseau routier**. Les rues apparaissent 30 secondes en lignes de couleur
   (vert = double sens, orange = sens unique, points rouges = carrefours). Vérifiez qu'elles
   sont bien sur les routes.
3. Catégorie **Circulation > Vehicules**, cliquez sur **Remplir la liste des voitures** :
   le plugin cherche vos voitures dans le dossier Content (par exemple `SM_Hatchback`,
   `SM_SUV`, `SM_Pickup`, `SM_SportsCar`, `SM_Truck_Box` du Vehicle Variety Pack).
4. **Enregistrez le niveau** (le réseau et la liste des voitures sont enregistrés avec lui).
5. Cliquez sur **Play**. En haut à gauche, un message vert indique combien de voitures
   roulent.

Les étapes 2 et 3 sont facultatives pour essayer (le plugin le fait tout seul au lancement),
mais **obligatoires pour un jeu packagé** : seuls les assets référencés par le niveau sont
inclus dans le jeu final, et le dossier `Tools` n'est pas copié.

## Si une voiture roule de travers ou à reculons

Dans *Details* > *Circulation > Vehicules* > **Modeles**, ouvrez la voiture concernée et
mettez **Correction Orientation** à `90`, `-90` ou `180`. Vous pouvez aussi baisser sa
**Frequence** (0 = jamais) ou supprimer la ligne.

## Réglages utiles (panneau Details du Spawner)

| Réglage | Par défaut | Rôle |
|---|---|---|
| Actif | coché | décocher pour couper la circulation sans rien supprimer |
| Nombre De Voitures | 50 | nombre de voitures |
| Multiplicateur De Vitesse | 1 | 1 = code de la route ; 1,3 = conducteurs pressés |
| Distance Affichage | 25000 (250 m) | au-delà, les voitures ne sont plus dessinées |
| Suivre Le Joueur | coché | garde les voitures autour du joueur |
| Rayon Autour Du Joueur | 30000 (300 m) | taille de cette zone |
| S'Arreter Pour Les Pietons | coché | freinage devant les passants et le joueur |
| Klaxon | aucun | son joué quand un piéton bloque une voiture |
| Couleurs Aleatoires | coché | couleur de carrosserie au hasard (si le matériau le permet) |
| Afficher Le Reseau | décoché | dessine les rues pendant le jeu (pour vérifier) |

### Routes ajoutées à la main (facultatif)

Pour une rue absente d'OpenStreetMap (parking, chemin privé…) : posez un acteur avec un
**Spline Component** (par exemple le `BP_Spline` du dossier Content), dessinez la route, puis
ajoutez cet acteur dans **Routes Manuelles**. Double sens par défaut ; ajoutez le *Tag*
`SensUnique` à l'acteur pour un sens unique (dans le sens de la spline). Les extrémités se
raccordent toutes seules aux rues proches (moins de 1,5 m). Cliquez ensuite à nouveau sur
**Construire le reseau routier**.

### Si vous avez déplacé la ville ou relancé `chelles_complet.py`

Recliquez sur **Construire le reseau routier** puis enregistrez. Si la ville a été déplacée
dans le niveau, indiquez le décalage dans **Decalage De La Carte** (réglages avancés).

## Tout retirer

1. Supprimez l'acteur « Circulation Spawner » du niveau, puis enregistrez.
2. Fermez Unreal et supprimez le dossier `Plugins\ChellesCirculation`.

Le projet revient exactement comme avant.

## Feux tricolores (26/11/2026)

- Aux plus grands carrefours (une rue à 50 km/h, 3 rues ou plus, pas les ronds-points),
  le plugin pose un **feu par rue qui arrive** : poteau sur le trottoir de droite, à la
  ligne d'arrêt, avec rouge en haut, orange, vert en bas.
- La grande rue et la rue d'en face passent au vert ensemble, puis les rues en travers.
  Entre les deux, 2 secondes de rouge partout pour que le carrefour se vide.
- Les voitures **s'arrêtent au rouge**, et à l'orange si elles peuvent freiner sans piler.
  Les voitures en file derrière attendent aussi. Une voiture arrêtée au feu n'est jamais
  « recyclée » ailleurs.
- Réglages dans `Circulation.txt` (pas besoin de recompiler) : `FeuxTricolores=0` pour les
  couper, `FeuxNombreMaximum`, `FeuxSeulementGrandesRues`, `FeuxDistanceMinimum`,
  `FeuxDureeVert`, `FeuxDureeOrange`, `FeuxDureeRougePartout`.
- Le journal du jeu dit combien de carrefours ont des feux (ligne « Feux tricolores : »).
