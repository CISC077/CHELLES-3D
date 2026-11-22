# Plugin ChellesRaccourcis - touches F1 a F12 + roue des armes TAB (21/11/2026)

Refonte "menu armes" (Prompt2026.md) :
- etape 1 : F1-F12 = choix du MODE, avec un menu deroulant en haut de l'ecran ;
- etapes 2 et 3 : TAB = roue des ARMES facon GTA, liste des armes dans le tableau DT_Armes.

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

## Roue des armes (maintenir TAB)
- Maintenir TAB : la roue apparait (gris-bleu transparent), le temps ralentit en solo.
- Viser une arme a la souris, relacher TAB (ou clic gauche) pour la prendre. Clic droit : annuler.
- Au centre : nom de l'arme et munitions (chargeur / reserve). L'arme en main a un lisere blanc.
- En bas a droite : le personnage joue (mode choisi avec F1-F12).
- Armes pas encore ramassees : grisees (`ToutesLesArmes=Non` pour ne montrer que les armes ramassees,
  `DonnerLesArmes=Oui` pour les recevoir en les choisissant, pratique pour tester en solo).
- L'ancienne roue des modes (ChellesRoue) ne s'ouvre plus sur TAB (sa liaison TAB est retiree pendant le jeu,
  le plugin ChellesRoue n'est pas modifie). Les modes sont sur F1-F12.

Les armes sont des DONNEES : tableau `/Game/Chelles/Donnees/DT_Armes` (structure `ChellesArmeLigne`) :
| Colonne | Role |
|---|---|
| Nom | texte de la case (vide = nom de l'arme, WeaponName) |
| ClasseArme | Blueprint de l'arme FPS_Game (BPC_WeaponPistol...), vide pour l'arsenal |
| Arsenal | Blaster, Bazooka, Sabre, Grenade, LanceFlammes, Mitrailleuse (armes ChellesArmes) |
| Icone | image de la case (vide = WeaponImage de l'arme, ou un petit dessin) |
| Couleur | couleur de la bande (transparente = gris-bleu) |
| Categorie | Pistolet, Fusil, Pompe, Precision, Lourde, Melee, Energie, Lancer (choisit le dessin) |
| bDonner | coche = choisir l'arme la donne si elle n'est pas ramassee (solo / hote) |
- Creer : `Tools\RaccourcisF\CREER_LA_TABLE.bat` (cree DT_Raccourcis ET DT_Armes, depuis Armes.csv).
- Ajouter une arme : bouton Add du tableau DT_Armes, remplir la ligne, Save. Rien a recompiler.
- Une arme ramassee qui n'est dans aucune ligne apparait quand meme dans la roue.
Tant que DT_Armes n'existe pas, une liste par defaut (la meme que Armes.csv) est utilisee.

Console : `Chelles.RoueArmes` (liste), `Chelles.RoueArmes Relire`, `Chelles.RoueArmes 0` / `1`.
Journal : lignes `[RoueArmes]`.
