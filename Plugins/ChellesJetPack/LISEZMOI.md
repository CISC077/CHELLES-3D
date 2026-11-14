# JetPacks à ramasser (CHELLES 3D)

5 JetPacks de couleur (rouge, bleu, vert, jaune, violet) posés dans Rachid. Le joueur qui marche dessus le met sur son dos et peut voler.

## Installer (une fois)

1. Fermez Unreal.
2. Double-cliquez sur `Tools\JetPack\INSTALLER_JETPACKS.bat`.
   - il compile le plugin `Plugins\ChellesJetPack` (1 à 5 minutes, seulement la 1re fois) ;
   - il ouvre Unreal, crée les Blueprints, pose les JetPacks dans Rachid, enregistre et referme.
3. Le journal s'affiche à la fin (`Tools\JetPack\jetpack.log`).

## Les Blueprints (dossier `Content/FPS_Game/Blueprints/Bonus`)

| Blueprint | Rôle |
|---|---|
| `BP_JetPack` | le modèle commun : 3D, flammes, bruit, vitesse, touches |
| `BP_JetPack_Rouge` / `_Bleu` / `_Vert` / `_Jaune` / `_Violet` | même JetPack, seule la couleur change |

- **En poser d'autres** : glissez un `BP_JetPack_*` depuis le dossier Bonus dans la carte.
- **Changer une couleur** : double-clic sur le Blueprint > Class Defaults > `JetPack | Apparence` > `Couleur`.
- **Nouvelle couleur** : clic droit sur `BP_JetPack` > Create Child Blueprint Class, puis réglez `Couleur` et `Nom Couleur`.
- **Plus rapide / moins haut** : dans `BP_JetPack`, `JetPack | Vol` (Vitesse Vol, Vitesse Turbo, Plafond Vol).

## En jeu

| Touche | Action |
|---|---|
| marcher dessus | ramasser |
| ESPACE | décoller / monter |
| CTRL ou C | descendre (près du sol = atterrir) |
| Z Q S D | avancer, comme d'habitude |
| MAJ | turbo |
| N | lâcher le JetPack |

- Manette : A = monter, B = descendre, clic gauche du stick = turbo.
- Un seul JetPack à la fois par joueur.
- Mort ou partie quittée : le JetPack tombe sur place, puis revient à son point de départ au bout de 90 s si personne ne le reprend.
- Changement de personnage avec la roue (Tab) : le JetPack suit le joueur.
- En véhicule : le JetPack reste sur le dos, sans voler.
- Multijoueur : le serveur décide qui le prend, tout le monde voit le JetPack sur le dos, les flammes et entend le moteur.

## Annuler

`RETIRER_JETPACKS.bat` (Unreal fermé) enlève les JetPacks posés par le script. Les Blueprints restent.
Copie de Rachid avant chaque passage : `Tools\JetPack\sauvegarde\`.
