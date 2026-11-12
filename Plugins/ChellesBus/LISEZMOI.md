# Bus pendant l'invasion zombie (CHELLES 3D)

Pendant chaque invasion zombie (plugin ChellesZombies) :

1. **La route est vide** : toutes les voitures de la circulation disparaissent. Elles reviennent à la fin de l'invasion.
2. **Un bus `BP_Bus_sity`** (pack Bus_sity) roule sur les routes et fait le tour de **tous les arrêts de bus** posés dans la carte.
   Son trajet est calculé tout seul à partir de la position des arrêts (plus court chemin sur les routes de la circulation, à droite de la chaussée).
3. **Portes** : fermées quand le bus arrive. À l'arrêt, approchez-vous d'une porte (dehors ou dedans) et appuyez sur **E** pour l'ouvrir ou la fermer.
4. **Attente à chaque arrêt** : **20 s** si le joueur est dehors, **10 s** si un joueur est dans le bus. Le chauffeur ferme les portes encore ouvertes avant de repartir.
5. Le bus s'arrête si un joueur est juste devant lui.
6. À la fin de l'invasion, le bus disparaît (s'il y a des passagers, il les dépose d'abord au prochain arrêt).

## Installation (une seule fois)

1. Fermez Unreal.
2. Double-cliquez sur `Tools\Bus\INSTALLER_BUS.bat` : il compile ce plugin et crée le Blueprint **`Content/Chelles/Bus/BP_Arret_Bus`**.
3. Ouvrez Unreal, glissez **BP_Arret_Bus** dans la carte **Rachid**, sur le trottoir au bord d'une route.
   La flèche bleue de l'arrêt doit pointer vers la chaussée. Posez-en au moins 2.
4. Dans les détails de chaque arrêt : **Nom Arret** (texte du panneau), **Ordre** (1, 2, 3… pour imposer l'ordre de passage ; 0 = automatique).

> Le nom de l'asset est `BP_Arret_Bus` sans accent : les accents dans les noms de fichiers Unreal posent des problèmes (empaquetage du jeu, Git). Le panneau affiche bien « Arrêt ».

## Tester sans attendre l'invasion

En jeu, touche **²** (console) : `Chelles.Bus` (route vide + bus tout de suite), `Chelles.BusFin` (fin du test).
Ou `Chelles.Zombies` pour lancer une vraie invasion.

## Réglages : `Plugins\ChellesBus\Bus.txt`

Vitesse, temps d'attente, touche des portes, hauteur du plancher, etc. Pas besoin de recompiler.

## En cas de souci

Le journal `Saved\Logs\FPS_Chelles.log` contient les lignes `[Bus]` (arrêts trouvés, trajet, portes…).

- *« Arret ignore : trop loin d'une route »* : rapprochez l'arrêt de la route (ou augmentez `DistanceMaxArretRouteMetres`).
- On flotte ou on s'enfonce dans le bus : changez `HauteurPlancher`.
- Le bus roule de travers : `CorrectionOrientation=90` (ou -90, 180).

Le bus n'utilise plus la physique de véhicule du pack : il est posé sur la route à chaque image (plus fiable sur les routes de Chelles).
Des murs invisibles remplacent sa carrosserie pour les collisions, avec une ouverture à chaque porte.
