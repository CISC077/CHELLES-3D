# LENA jouable (CHELLES 3D, 24/11/2026)

LENA = ton personnage (Content\Personnages\LENA). Elle se choisit dans **JOUER > HABILLAGE > LENA**.
Elle a les armes du soldat (TAB = roue des armes) et tous les pouvoirs (ESPACE en l'air = vol, X = flash, O = changer de tir).

## A faire une seule fois

1. Ouvre le projet dans Unreal.
2. Menu **File > Execute Python Script...** et choisis `Tools\Lena\creer_lena.py`.
   Le script importe LENA **avec son squelette** (fichier Mixamo `Bureau\LENA3D\Animations\base_basic_shaded.fbx`)
   et ses animations (marche, course, vol, accroupi, danses) dans `/Game/Personnages/LENA/Joueur`.
   Une fenetre "LENA est prete" s'affiche a la fin.
3. Ferme Unreal.
4. Double-clic sur `Plugins\ChellesWraith\Compiler_le_plugin.bat`, attends TERMINE.
5. Double-clic sur `Plugins\ChellesMenu\Compiler_le_plugin.bat`, attends TERMINE.
6. Rouvre le projet, Play, **JOUER > HABILLAGE > LENA**.

## Pourquoi un nouvel import ?

Les modeles deja dans `Content\Personnages\LENA` (base, base_basic_pbr, base_basic_shaded) ont ete importes en
**Static Mesh** (objet fixe, sans os) : ils ne peuvent pas marcher. Le fichier du dossier `Animations` (Mixamo)
contient le squelette : c'est lui que le script importe. Les anciens fichiers ne sont pas touches.

## Reglages (Plugins\ChellesWraith\Wraith.txt, ligne Heros=Lena)

- Avant-derniere valeur = **taille** (le script la regle pour 175 cm).
- Derniere valeur = **rotation** : mets `180` si LENA marche a reculons, `90` ou `-90` si elle marche de cote.
- Apres modification, en jeu : touche ² puis `Chelles.Wraith Relire`.
- Console : `Chelles.Wraith Skin LENA` pour la mettre directement.

Fichiers d'origine sauvegardes a cote, avec `.avant_lena` a la fin du nom.
