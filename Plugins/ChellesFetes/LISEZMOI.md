# ChellesFetes : les fêtes du calendrier

Le plugin regarde la date du PC à chaque partie (carte Rachid) :

| Fête | Dates (Fetes.txt) | Ce qui se passe |
|---|---|---|
| **Halloween** | 25/10 → 02/11 | 40 citrouilles lumineuses sur l'herbe, 8 fantômes qui flottent, 35 % des passants et zombies avec une tête de citrouille, son qui fait peur la nuit |
| **14 juillet** | 13/07 → 15/07 | Feu d'artifice bleu blanc rouge devant le joueur : 70 s de spectacle avec bouquet final, puis 2 min de pause. Le « boum » arrive après l'éclair |
| **Fête de la musique** | 20/06 → 22/06 | Une scène (écran géant, projecteurs colorés, enceintes) sur une place libre près du départ, 12 danseurs, musique qu'on entend jusqu'à 70 m |

Rien n'est modifié dans la carte : tout apparaît au Play et disparaît à la fin.

## Installer (une seule fois, Unreal fermé)
Double-clic sur `Tools\Fetes\INSTALLER_FETES.bat`.

## Essayer sans attendre la date
- `Tools\Fetes\TESTER_HALLOWEEN.bat`, `TESTER_14_JUILLET.bat`, `TESTER_FETE_MUSIQUE.bat`
- remettre la date : `FETES_SELON_LA_DATE.bat` ; tout couper : `COUPER_LES_FETES.bat`
- ou en jeu, console : `Chelles.Fetes Halloween`, `Chelles.Fetes Juillet`, `Chelles.Fetes Musique`, `Chelles.Fetes Auto`, `Chelles.Fetes Feux` (feu d'artifice tout de suite), `Chelles.Fetes PoserScene` (met la scène devant vous), `Chelles.Fetes Info`

## Réglages
Tout est dans `Plugins\ChellesFetes\Fetes.txt` (Bloc-notes, puis Play ou `Chelles.Fetes Relire`).

## Si ça ne marche pas
Journal du jeu : `Saved\Logs\FPS_Chelles.log`, lignes `[Fetes]`. Compilation : `Plugins\ChellesFetes\Journal_compilation.txt`.
