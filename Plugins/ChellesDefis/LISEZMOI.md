# ChellesDefis : le défi de la semaine dans le jeu

Le défi est choisi par le VPS (service `chelles3d-defis`, port 8082) et annoncé chaque lundi à 18 h sur le Discord.
Ce plugin le demande au serveur au début de chaque partie et l'affiche en haut de l'écran pendant 14 secondes,
dès que le joueur est dans le quartier (menus fermés).

- Défis du DERNIER REFUGE : le serveur vérifie avec les parties envoyées au classement (pseudo du chat global, touche B).
  Quand le défi est réussi, le jeu affiche **DÉFI DE LA SEMAINE RÉUSSI !** et ajoute l'XP aux pouvoirs (une fois par semaine et par PC).
- Défis « libre » : le bandeau invite à poster sa capture sur le Discord.
- Le jeu redemande au serveur toutes les 5 minutes (pour voir si le défi vient d'être réussi).

## Compiler (une seule fois)
Fermez Unreal, puis double-cliquez sur :
`C:\Users\CISCO\Documents\Unreal Projects\CHELLES 3D\Plugins\ChellesDefis\Compiler_le_plugin.bat`

## Réglages
`C:\Users\CISCO\Documents\Unreal Projects\CHELLES 3D\Plugins\ChellesDefis\Defis.txt` (Actif, Adresse, durée du bandeau…).

## Console (touche ²)
- `Chelles.Defi` : redemande le défi et remontre le bandeau.
- `Chelles.DefiEtat` : écrit le défi et ton résultat dans le journal (Output Log).

Aucune touche du clavier n'est utilisée. Le plugin ne modifie aucun autre plugin : l'XP est ajoutée à `XPLocale`
de ChellesPouvoirs par son nom, comme le font les Missions.
