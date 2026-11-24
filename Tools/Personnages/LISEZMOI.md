# Personnages jouables de MERIDJA : LENA, SOFIA, LUCIA... (24/11/2026)

Ils se choisissent dans **JOUER > HABILLAGE** (2e rangee de boutons : LENA, SOFIA, LUCIA).
Toutes les armes du soldat (TAB) et tous les pouvoirs (ESPACE en l'air = vol, X = flash, O = changer de tir).

## A faire (une fois, et a chaque nouveau personnage)

1. Ouvre le projet dans Unreal.
2. Menu **File > Execute Python Script...** et choisis `Tools\Personnages\creer_personnages.py`.
   Il importe chaque personnage **avec son squelette** depuis le Bureau (`LENA3D\Animations`, `SOFIA 3D\Animations`),
   ses animations, et regle sa taille. Ce qui est deja fait est garde : on peut le relancer.
   Une fenetre recapitulative s'affiche a la fin.
3. Ferme Unreal, puis double-clic sur `Plugins\ChellesWraith\Compiler_le_plugin.bat` et sur
   `Plugins\ChellesMenu\Compiler_le_plugin.bat` (attendre TERMINE a chaque fois).
4. Rouvre le projet, Play, **JOUER > HABILLAGE > LENA** ou **SOFIA**.

## Si un personnage est tout blanc / gris, ou reste les bras en croix
Relancer ce script. Depuis le 24/11 au soir il :
- met les vraies couleurs (texture_Mat du dossier du perso, ou M_<Nom> cree avec Textures\texture_diffuse.png
  et texture_normal.png du dossier du Bureau) et les enregistre tout de suite ;
- refait le corps s'il n'a pas de squelette (Unreal avait plante, memoire pleine, avant de l'enregistrer) ;
- n'importe plus que l'animation (avant : une copie du corps de 130 Mo par animation, d'ou le plantage).

## Deux passages (memoire)
1er passage : imports (corps, couleurs, animations). Le message de fin demande alors de RELANCER le script.
2e passage (apres avoir rouvert Unreal) : rien a importer, donc il allege les personnages (3 versions simplifiees
LOD pour la fluidite, de loin) et fait le menage des doublons, un par un en vidant la memoire.

## Preparer un nouveau personnage (Mixamo)

- Telecharger le personnage **With Skin**, et les animations avec **In Place** coche.
- Les mettre dans `Bureau\<NOM> 3D\Animations` : le corps (fichier avec squelette) + `Walking.fbx`, `Fast Run.fbx`,
  `Flying.fbx`, `Crouching.fbx`, une danse, un saut sur place, et si possible un **Idle**.
- Ne pas l'importer soi-meme dans Unreal : le script s'en charge.

## Reglages (Plugins\ChellesWraith\Wraith.txt, lignes Heros=Lena / Heros=Sofia)

- Avant-derniere valeur = **taille** (reglee par le script). Derniere = **rotation** : `180` si elle marche a reculons.
- En jeu : touche ² puis `Chelles.Wraith Relire` apres une modification, ou `Chelles.Wraith Skin SOFIA`.

## Danses

**T** : LENA, SOFIA ou LUCIA enchainent leurs danses (bouger pour arreter, W pour changer de camera et la voir).
LENA : hip-hop, rumba, twist, uprock, flair, twerk. SOFIA : rumba, northern soul, flair. LUCIA : hip-hop.
Pour en ajouter : animation Mixamo dans `Bureau\<NOM> 3D\Animations`, une ligne dans `creer_personnages.py`
et son chemin dans la ligne `Danses=` de `Plugins\ChellesWraith\Wraith.txt`.
