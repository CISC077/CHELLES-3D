# Ligne de train autour de la carte (CHELLES 3D)

## Installer (une seule fois)
1. Fermez Unreal.
2. Double-cliquez sur `Tools\Train\INSTALLER_TRAIN.bat` (1 à 5 minutes).
3. Ouvrez le projet, carte **Rachid**, et cliquez sur **Play**.

Rien à poser dans la carte : au lancement du jeu, le train et ses panneaux d'arrêt
apparaissent tout seuls. La première fois, le jeu cherche les adresses sur Internet
(Base Adresse Nationale) ; le résultat est gardé dans `Plugins\ChellesTrain\ArretsTrouves.txt`.

## Ce que fait le train
- Le train du pack **Intercity_Train** (2 locomotives) roule au milieu de la rue, sur des rails,
  et fait le tour des arrêts en boucle, dans le sens des aiguilles d'une montre.
- À chaque arrêt : portes ouvertes **30 secondes** côté trottoir, un marchepied sort, puis
  « Attention à la fermeture des portes », et départ.
- Les voitures de la circulation qui sont sur son passage disparaissent, puis reviennent.
- Près d'un panneau TRAIN : « prochain train dans … ». Dans le train : « Prochain arrêt : … ».
- Si un joueur est sur la voie devant le train, il freine.

## Changer les arrêts, la vitesse, le nombre de voitures
Ouvrez `Plugins\ChellesTrain\Train.txt` avec le Bloc-notes, modifiez, enregistrez, relancez Play
(pas besoin de recompiler). Exemples :
- enlever un arrêt : mettez un `#` au début de sa ligne ;
- portes ouvertes 1 min 30 : `AttentePortesOuvertesSecondes=90` ;
- garder les 9 arrêts séparés : `RegrouperArretsProchesMetres=0` ;
- train plus long : `NombreDeVoitures=3`.

## Tester vite (console du jeu, touche ²)
- `Chelles.TrainVoir` : vous emmène au prochain arrêt.
- `Chelles.Train` : écrit l'état de la ligne dans le journal (Output Log, lignes `[Train]`).

## Si quelque chose cloche
Dans Unreal : Window > Developer Tools > Output Log, tapez `[Train]` dans la recherche
et envoyez les lignes à Claude.
- Le train roule à reculons : `InverserLeSens=Oui`.
- Il s'enfonce dans la route / il flotte : `HauteurRails=` (en cm).
- Un arrêt est mal placé : effacez sa ligne dans `ArretsTrouves.txt`, ou donnez sa position à la main
  dans Train.txt : `Arret=Nom | adresse | X | Y`.

## Enlever le train
`Actif=Non` dans Train.txt (ou retirer le dossier Plugins\ChellesTrain en le mettant en quarantaine).
