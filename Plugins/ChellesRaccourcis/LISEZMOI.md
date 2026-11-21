# Plugin ChellesRaccourcis - armes sur F1 a F12 (21/11/2026)

Etape 1 de la refonte du menu armes (Prompt2026.md).

## Touches
| Touche | Arme |
|---|---|
| F1 a F4 | cases 1 a 4 du soldat (ordre de ramassage) |
| F5 | Blaster |
| F6 | Bazooka |
| F7 | Sabre lumineux |
| F8 | Grenade a zombies |
| F9 | Lance-flammes (arme lourde) |
| F10 | Mitrailleuse (arme lourde) |
| F11, F12 | reservees |

A changer dans `Raccourcis.txt` (Bloc-notes), sans recompiler.
Les touches marchent a pied avec le soldat (ou Spider-Male / PANTERA qui ont ses armes).
En vehicule, a cheval ou en DYNAMIC COMBAT, elles ne font rien.

## Touches deplacees pour liberer F1-F10
| Avant | Apres | Ou |
|---|---|---|
| F1 / F2 (aides du pack vehicules ShowCG / ShowCT) | Ctrl+F1 / Ctrl+F2 | Config\DefaultInput.ini |
| F7 (ecran MODES du Dernier Refuge) | Origine (Home) | ChellesSurvie (a recompiler) |
| F8 / F9 (poser / retirer la police, admin) | Inser / Suppr | ChellesPolice\Police.txt |
| F10 (ranger / rappeler le drone) | Fin (End) | ChellesDrone\Drone.txt |

Les anciens fichiers sont gardes a cote avec la fin `.avant_F1F12`.

## Console (touche ²)
- `Chelles.Raccourcis` : liste des touches + ce que le plugin a trouve sur le soldat
- `Chelles.Raccourcis Relire` : relit Raccourcis.txt
- `Chelles.Raccourcis F3` : fait comme si on appuyait sur F3
- `Chelles.Raccourcis 0` / `1` : couper / remettre

Journal : lignes `[Raccourcis]` dans Saved\Logs\FPS_Chelles.log
