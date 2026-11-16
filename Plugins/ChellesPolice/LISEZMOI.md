# Plugin ChellesPolice (CHELLES 3D)

Voitures de police du pack **Sports and Police Cars - Drivable** (`Content/SportVehiclesPack`).
Elles remplacent le SUV de la roue TAB (demande du 16/11/2026).

## Installation (une seule fois)
1. Fermez Unreal.
2. Double-cliquez `Tools\Police\INSTALLER_POLICE.bat`.
   - il range le SUV dans `_Quarantaine` (rien n'est supprime), apres avoir verifie que rien ne s'en sert ;
   - il compile ChellesPolice, ChellesRoue et ChellesCirculation ;
   - il ouvre Unreal, cree `BP_Police_Drivable` et le referme.
3. Pour revenir en arriere : `Tools\Police\REMETTRE_LE_SUV.bat`.

## En jeu
| Touche | Action |
|---|---|
| TAB > POLICE | conduire une voiture de police |
| F8 | poser une voiture de police la ou l'on vise (administrateur) |
| F9 | retirer la voiture visee (administrateur) |
| E | monter dans une voiture de police proche / descendre |
| Z S Q D | gaz, frein / marche arriere, volant |
| Espace | frein a main |
| L | sirene et gyrophares |
| H | klaxon |

- **Administrateur** : le joueur en solo, l'hote en multijoueur. Sur le serveur du VPS : les pseudos de
  `Administrateurs=` dans `Police.txt` (sinon le premier joueur connecte).
- Les voitures posees sont gardees dans `Police_Positions.txt` et reviennent a chaque partie.
- Tous les joueurs peuvent conduire une voiture libre (en multijoueur : monter depuis le Soldat).
- Quand une voiture de police roule (plus de 10 km/h), les voitures de la circulation **devant elle et dans
  son sens** se rangent a droite ; celles d'en face continuent (plugin ChellesCirculation, `DegagerDevant`).

## Reglages
`Police.txt` (Bloc-notes), sans recompiler. Console (touche ²) : `Chelles.Police Info`, `Chelles.Police Liste`,
`Chelles.Police Relire`.
