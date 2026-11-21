# Plugin ChellesRaccourcis - touches F1 a F12 (21/11/2026)

Etape 1 de la refonte (Prompt2026.md) : F1-F12 = choix du MODE, avec un menu deroulant en haut de l'ecran.
La roue TAB marche toujours ; a l'etape 3 elle deviendra la roue des ARMES.

## Touches (par defaut)
| Touche | Mode |
|---|---|
| F1 | Soldat |
| F2 | Spider-Male |
| F3 | Spider-Femelle |
| F4 | PANTERA |
| F5 | Motoneige |
| F6 | POLICE |
| F7 | FLASH ECLAIR |
| F8 | FLAMME FIRE |
| F9 | SuperPowers |
| F10 | DYNAMIC COMBAT |
| F11 | HORSE |
| F12 | libre (reservee) |

## Data-Driven
Les touches sont des DONNEES, pas du code : tableau `/Game/Chelles/Donnees/DT_Raccourcis`
(structure `ChellesRaccourciLigne` : Touche, Action, NomAffiche, Couleur, Icone).
- Creer le tableau : `Tools\RaccourcisF\CREER_LA_TABLE.bat` (Unreal ferme, plugin compile).
- Modifier : double-clic sur DT_Raccourcis dans Unreal, changer la ligne, Save.
- Ajouter une touche : bouton Add du tableau. Rien a recompiler.
- Action = nom d'un mode de la roue, Case1..Case4 (armes du soldat), Blaster/Bazooka/Sabre/Grenade/LanceFlammes/Mitrailleuse, ou Reserve.
Tant que le tableau n'existe pas, les lignes F1=... de `Raccourcis.txt` servent de secours.

## Touches deplacees pour liberer F1-F12
| Avant | Apres | Ou |
|---|---|---|
| F1 / F2 (aides du pack vehicules ShowCG / ShowCT) | Ctrl+F1 / Ctrl+F2 | Config\DefaultInput.ini |
| F7 (ecran MODES du Dernier Refuge) | Origine (Home) | ChellesSurvie (a recompiler) |
| F8 / F9 (poser / retirer la police, admin) | Inser / Suppr | ChellesPolice\Police.txt |
| F10 (ranger / rappeler le drone) | Fin (End) | ChellesDrone\Drone.txt |
Anciennes versions gardees a cote avec la fin `.avant_F1F12`.

En DYNAMIC COMBAT, F2 a F6 restent au personnage (roulade, saut, potion, ramasser, cible) :
pour quitter ce mode, F1 (Soldat) ou F7 a F11 marchent.

## Console (touche ²)
- `Chelles.Raccourcis` : liste des touches, roue trouvee, mode joue (journal + menu)
- `Chelles.Raccourcis Relire` : relit Raccourcis.txt et le tableau
- `Chelles.Raccourcis F3` : fait comme si on appuyait sur F3
- `Chelles.Raccourcis 0` / `1` : couper / remettre

Journal : lignes `[Raccourcis]` dans Saved\Logs\FPS_Chelles.log
