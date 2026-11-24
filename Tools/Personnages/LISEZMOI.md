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

## Preparer un nouveau personnage (Mixamo)

- Telecharger le personnage **With Skin**, et les animations avec **In Place** coche.
- Les mettre dans `Bureau\<NOM> 3D\Animations` : le corps (fichier avec squelette) + `Walking.fbx`, `Fast Run.fbx`,
  `Flying.fbx`, `Crouching.fbx`, une danse, un saut sur place, et si possible un **Idle**.
- Ne pas l'importer soi-meme dans Unreal : le script s'en charge.

## Reglages (Plugins\ChellesWraith\Wraith.txt, lignes Heros=Lena / Heros=Sofia)

- Avant-derniere valeur = **taille** (reglee par le script). Derniere = **rotation** : `180` si elle marche a reculons.
- En jeu : touche ² puis `Chelles.Wraith Relire` apres une modification, ou `Chelles.Wraith Skin SOFIA`.
