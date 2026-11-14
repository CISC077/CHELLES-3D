# Plugin ChellesConduite : SUV stable

Le SUV de la roue TAB (DriveableCar_basic, SUV_BP_child) ne glisse plus sur le côté quand on tourne et
se conduit facilement, comme la motoneige.

## Ce qu'il fait
Quand le SUV apparaît :
- pneus deux fois plus accrocheurs, plus rigides sur le côté ;
- roues avant qui braquent au maximum de 40° (direction plus douce) ;
- amortisseurs plus fermes ;
- centre de gravité descendu de 40 cm.

À chaque image, quand le SUV touche le sol :
- la glisse sur le côté est retirée (la voiture va là où elle pointe) ;
- la vitesse de rotation est limitée (plus de tête-à-queue) ;
- la caisse ne se penche plus et ne se retourne plus ;
- elle est plaquée au sol à grande vitesse.

Aucun fichier du pack n'est modifié : tout est fait en mémoire pendant le jeu et remis à la fin.

## Installer
1. Fermez Unreal.
2. Double-cliquez `Plugins\ChellesConduite\Compiler_le_plugin.bat` et attendez « TERMINE ».
3. Ouvrez le projet, Play, TAB → SUV.

## Régler
`Plugins\ChellesConduite\ConduiteSUV.txt` (Bloc-notes). Console (touche ²) :
- `Chelles.SUV` : état
- `Chelles.SUV 0` / `Chelles.SUV 1` : couper / remettre l'aide
- `Chelles.SUV Relire` : relit le fichier

Journal : lignes `[SUV]` dans `Saved\Logs\FPS_Chelles.log`.

## Retirer
Mettez `Actif=Non` dans ConduiteSUV.txt, ou supprimez le dossier `Plugins\ChellesConduite`.
