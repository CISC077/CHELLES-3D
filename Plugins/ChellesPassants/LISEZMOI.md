# Passants de Chelles (plugin)

Ce plugin ajoute une quarantaine de passants qui se promènent dans la ville.
Il est **entièrement séparé** du reste du projet : il ne modifie aucun niveau, aucun
Blueprint, aucun réglage, et ne touche pas à `Tools\chelles_complet.py`.
Tant que vous ne posez pas l'acteur « Passants Spawner » dans un niveau, il ne se passe rien.

## Ce qu'il faut avoir

- Unreal Engine 4.27
- **Visual Studio 2019** avec « Développement Desktop en C++ » (Unreal en a besoin
  une seule fois, pour compiler le plugin).

## Installation (une seule fois)

1. Fermez Unreal.
2. Copiez le dossier `Plugins\ChellesPassants` dans
   `C:\Users\CISCO\Documents\Unreal Projects\CHELLES 3D\Plugins\ChellesPassants`
   (créez le dossier `Plugins` s'il n'existe pas).
3. Double-cliquez sur `Plugins\ChellesPassants\Compiler_le_plugin.bat`. Il compile le
   plugin (1 à 5 minutes) et affiche « TERMINÉ ». Il ne modifie pas votre projet : il
   crée seulement le dossier `Binaries` à l'intérieur du plugin.
4. Ouvrez le projet. Pour vérifier : *Edit > Plugins*, catégorie « Chelles 3D » :
   « Chelles Passants » doit être coché.

## Mettre les passants dans la ville

1. **Le NavMesh (les zones où l'on peut marcher)** : panneau *Modes* > *Volumes* >
   faites glisser un **Nav Mesh Bounds Volume** dans le niveau, puis agrandissez-le
   (échelle) pour qu'il couvre les rues. Appuyez sur **P** : les zones vertes sont celles
   où les passants pourront marcher. Sans NavMesh ils marchent quand même, mais en
   ligne droite et peuvent buter contre les murs.
2. Dans le *Content Browser*, cochez *View Options* > **Show Plugin Content** et
   **Show C++ Classes**, ouvrez `C++ Classes > ChellesPassants`, et faites glisser
   **Passants Spawner (Chelles)** dans la rue, au niveau du sol.
3. La grande sphère verte montre la zone de promenade (200 m de rayon par défaut).
4. Cliquez sur **Play**. En haut à gauche, un message vert indique combien de passants
   sont apparus et combien de personnages différents ont été trouvés.

## Les personnages (avec leurs vêtements)

Par défaut, le Spawner **cherche tout seul vos personnages dans le dossier Content** :
il garde les Skeletal Mesh de taille humaine (1,40 m à 2,20 m) qui ont une animation de
marche (« Walk ») sur le même squelette, et une animation d'attente (« Idle ») si elle
existe. Chaque passant prend un personnage au hasard, avec ses propres vêtements et
textures. Le mannequin gris d'Unreal (sans vêtements) est ignoré s'il y a d'autres
personnages.

La liste des personnages trouvés est écrite dans *Window > Developer Tools > Output Log*
(cherchez « LogPassants »).

Si le choix automatique ne convient pas, sélectionnez le Spawner et, dans *Details* >
*Passants > Personnages* :
- **Personnages** : ajoutez vous-même les Skeletal Mesh voulus ;
- **Animation Marche** / **Animation Repos** : les animations à utiliser
  (elles doivent être du même squelette que les personnages) ;
- ou **Animation Blueprint** : l'Animation Blueprint de vos personnages.

## Trottoirs seulement

Par défaut (case **Trottoirs Seulement** cochée, rubrique *Passants > Trottoirs*), les passants
n'apparaissent et ne marchent **que sur les trottoirs** : jamais sur les routes, l'herbe, les toits
ou les jumpers. Un trottoir est reconnu à son nom (acteurs `Chelles_Trottoir_...` ou meshes
`SM_Trottoir_...`, réglage **Mot Cle Trottoir**). Ce mode n'a pas besoin de NavMesh et ne change
rien pour les autres personnages du jeu. Pour l'instant, les passants ne traversent pas les rues :
chacun se promène sur sa portion de trottoir.

Décochez la case pour revenir à l'ancien comportement (NavMesh ou marche libre).

## Mettre à jour la liste des personnages

Après avoir ajouté des personnages dans Content : sélectionnez chaque Passants Spawner, cliquez sur
**Remplir la liste des personnages** (panneau Details), puis enregistrez le niveau. Sont ignorés :
les mannequins gris, les zombies, les corps seuls ou en morceaux, les doublons (_Mobile, _FPS, _Vol,
_Base) et les dossiers listés dans **Dossiers Ignores**.

## Réglages utiles (panneau Details du Spawner)

| Réglage | Par défaut | Rôle |
|---|---|---|
| Actif | coché | décocher pour couper les passants sans rien supprimer |
| Nombre De Passants | 40 | nombre de passants |
| Rayon De Zone | 20000 (200 m) | taille de la zone de promenade |
| Vitesse Min / Max | 100 / 160 | vitesse de marche (cm/s) |
| Pause Min / Max | 1 / 6 | arrêt entre deux trajets (secondes) |

On peut poser plusieurs Spawners (un par quartier, par exemple).

## Tout retirer

1. Supprimez l'acteur « Passants Spawner » du niveau (et le Nav Mesh Bounds Volume si
   vous l'aviez ajouté uniquement pour les passants), puis enregistrez.
2. Fermez Unreal et supprimez le dossier `Plugins\ChellesPassants`.

Le projet revient exactement comme avant.
