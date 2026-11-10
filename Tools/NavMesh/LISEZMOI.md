# NavMesh de la carte Rachid

Le NavMesh, c'est le « plan des endroits où l'on peut marcher » que les IA utilisent
pour trouver leur chemin (contourner les murs, prendre les rues…).
Sans lui, les soldats IA de FPS_Game restent sur place et les passants marchent en ligne droite.

## Ajouter le NavMesh

1. Ouvre Unreal avec la carte **Rachid**.
2. Menu **File > Execute Python Script…** → choisis `Tools\NavMesh\ajouter_navmesh.py`.
3. Une fenêtre affiche la taille de la ville mesurée et l'état des passants → **Oui**.
4. Menu **Build > Build Paths**. Attends que la barre en bas à droite disparaisse.
5. Clique dans la vue 3D puis appuie sur **P** : le sol où l'IA peut marcher devient **vert**.
   (P encore une fois pour le cacher.)
6. **Ctrl+S** pour enregistrer Rachid.

Relancer le script remplace le volume (utile si la ville s'agrandit).

## Retirer le NavMesh

**File > Execute Python Script…** → `Tools\NavMesh\retirer_navmesh.py` → **Oui** → **Ctrl+S**.
Ctrl+Z marche aussi juste après.

## Qui l'utilise ?

| Qui | Change ? |
| --- | --- |
| Soldats IA FPS_Game (AI_Soldier_Bandit…) | Oui : ils pourront se déplacer et poursuivre |
| Passants (ChellesPassants), mode normal | Oui : ils contournent les murs au lieu d'aller tout droit |
| Passants en mode « trottoirs seulement » | Non : ils suivent les trottoirs comme avant |
| Zombies (ChellesZombies) | Non : guidés par leur plugin, sans NavMesh |
| Circulation (voitures) | Non |

## Réglages choisis

- Un seul volume `NavMesh_Chelles` qui couvre les bâtiments, routes et trottoirs + 30 m de marge.
  Le Landscape et la végétation qui s'étendent loin hors de la ville ne sont pas comptés.
- Génération **Static** : calculée une fois dans l'éditeur, enregistrée dans Rachid.
  Pas de « Dynamic » : la ville ne bouge pas, et en Dynamic chaque voiture qui roule
  pourrait faire recalculer le NavMesh pendant le jeu (perte de FPS).
- Journal : `Tools\out\navmesh.log`.
