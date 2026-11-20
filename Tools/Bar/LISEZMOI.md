# Bar américain dans Rachid (pack AmericanBar)

Double-clic sur **INSTALLER_LE_BAR.bat**, avec Unreal **fermé**. Unreal s'ouvre, construit le bar, enregistre Rachid et se referme. Le compte rendu s'affiche à la fin. Il est aussi dans `bar.log`.

## Ce que fait le script

- Il fait d'abord une copie de Rachid dans `Tools\Bar\sauvegarde\`.
- Il cherche une **place libre et plate de 14 × 10 m**, collée à un mur de bâtiment, à 25 m ou plus du départ du joueur. La place ne doit pas être sur une route et rien ne doit se trouver au-dessus.
- Il y construit **une salle en briques** :
  - une porte ouverte et deux vitrines sur la rue ;
  - une enseigne rose **CHELLES BAR**.
- Il la meuble avec le pack AmericanBar :
  - un comptoir avec bouteilles et verres, et des étagères à bouteilles éclairées ;
  - 6 tabourets ;
  - **2 billards** (vert et rouge) avec les 16 boules et une queue sur chacun, et une lampe de billard au-dessus ;
  - 2 petites tables avec tabourets, verres et petite lampe ;
  - des lampes murales, de la déco et une porte de toilettes.
- Les lumières sont « mobiles » et sans ombres : pas besoin de recalculer l'éclairage, et c'est léger pour la fluidité.

Tout est rangé dans le dossier **Bar** de l'Outliner, avec l'étiquette `ChellesBar`. Le comptoir porte en plus l'étiquette `ChellesBarComptoir`. Rien d'autre dans la carte n'est touché.

## Réglages (en haut de `installer_bar.py`, avec le Bloc-notes)

- `TOURNER_COMPTOIR = True` : à mettre si le comptoir est posé à l'envers, côté serveur vers la salle. Relancez ensuite.
- `POSITION_MANUELLE = (X, Y, Z, Yaw)` : pour choisir la place vous-même.
  1. Dans l'éditeur, posez un objet au milieu de la future porte.
  2. Copiez sa Location X, Y et Z.
  3. Pour le Yaw, donnez la direction « de la porte vers le fond du bar ».
- `LARGEUR`, `PROFONDEUR`, `HAUTEUR` : la taille de la salle, en mètres.

Si vous relancez le script, le bar est reconstruit **au même endroit**.

## Annuler

Double-clic sur **RETIRER_LE_BAR.bat** (Unreal fermé). Seuls les objets du bar sont enlevés.
Autre solution : remettre une copie de `sauvegarde\` à la place de `Content\Maps\Rachid.umap`.

## Si aucune place n'est trouvée

Envoyez `bar.log` à Claude, ou choisissez la place à la main avec `POSITION_MANUELLE`.
