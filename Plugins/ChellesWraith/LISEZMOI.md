# ChellesWraith : Wraith, Dekker et Sci-Fi Soldier à la place du SOLDAT

Le joueur **SOLDAT** (FPS_Game, case "Soldat" de la roue TAB / F1) prend l'apparence de
**Wraith** (*Paragon: Wraith*, `Content/ParagonWraith`) ou de **Dekker** (*Paragon: Dekker*,
`Content/ParagonDekker`) ou du **Sci-Fi Soldier** (`Content/SciFiSoldier`, 5 tenues), au choix avec la touche **K**.

## Ce que fait le plugin
- **Corps** : le soldat n'est plus dessiné, Wraith est posé à sa place avec ses propres
  animations (Wraith_AnimBlueprint). Le soldat est toujours là en dessous : tirs, dégâts,
  armes, touches, roue TAB, multijoueur… rien ne change dans le jeu.
- **Son arme** : Wraith tient son fusil de sniper. En 1re personne, le fusil de Wraith remplace
  l'arme du soldat à l'écran (couteau, épée et grenades restent ceux du soldat).
- **Ses effets** : éclair du fusil de Wraith (P_Wraith_Sniper_MuzzleFlash) et animation de tir.
- **Sa voix** (anglais, celle du pack) : saut, atterrissage, douleur, santé basse, soin, mort,
  arrivée en jeu, changement de skin.
- **Touche K** : skin suivant. Le nom du skin s'affiche en haut de l'écran. Le choix est gardé
  (GameUserSettings.ini, section `[ChellesWraith]`). En multijoueur, chaque joueur
  choisit le sien avec K et les autres le voient.

Chaque héros (ligne `Heros=` de Wraith.txt) a ses animations, sa voix, son arme et son éclair de tir.
Chaque skin (ligne `Skin=`) dit à quel héros il appartient.

Le **Sci-Fi Soldier** est sur le squelette mannequin du soldat : il copie exactement ses gestes
(`Heros=SciFi|Soldat|…`) et garde les armes du soldat (le pack n'a ni arme, ni voix, ni animation).

| Skin (ordre de K) | D'où il vient |
|---|---|
| WRAITH VERT MILITAIRE | skin ODGreen du pack |
| WRAITH ROUGE ET NOIR | skin LunarOps recoloré en rouge par le plugin |
| WRAITH BLANC ET CYAN | skin LunarOps du pack |
| WRAITH CLASSIQUE | Wraith de base du pack |
| DEKKER | Dekker de base du pack (son bâton-arme, sa voix) |
| DEKKER ENERGIE | skin Energized (noir et cyan) |
| DEKKER GLACE | skin Arctic Frost (blanc et bleu) |
| DEKKER MAGMA | skin Magma (rouge et noir) |
| DEKKER SAINT-VALENTIN | skin Valentine (rose) |
| SCI-FI NOIR / ROUGE / DORE / BLEU / VERT | Sci-Fi Soldier, ses 5 tenues (MI_SciFiSoldier 1 à 5) |
| SOLDAT D'ORIGINE | le soldat sans héros |

Pas actif en Spider-Male / Spider-Femelle / PANTERA (la roue TAB habille déjà le soldat).

## Installer
1. Fermez Unreal.
2. Double-cliquez `Plugins\ChellesWraith\Compiler_le_plugin.bat` (1 à 5 minutes).
3. Rouvrez le projet, lancez **Play**, appuyez sur **K**.

## Réglages (Wraith.txt)
Bloc-notes, puis relancer le jeu ou console (touche ²) `Chelles.Wraith Relire`.
- `Actif=Non` : plugin coupé.
- `Skin=…` : ajouter / enlever / renommer des skins, changer la teinte (R,G,B), choisir le héros.
- `Heros=…` : ajouter un autre héros Paragon (son Animation Blueprint, ses voix, son arme).
- `ArmeWraith=Non` : garder les armes du soldat.
- `Voix=0` : héros muets.

## Console (touche ²)
- `Chelles.Wraith` : état.
- `Chelles.Wraith 0` / `1` : soldat d'origine / Wraith.
- `Chelles.Wraith Skin 2` : choisir un skin.
- `Chelles.Wraith Arme 0 0 5`, `ArmeRot 0 90 0`, `ArmeEchelle 0.7` : recaler le fusil en
  1re personne (la ligne à recopier dans Wraith.txt s'affiche).
- `Chelles.Wraith Params` : écrit les couleurs des matériaux dans `Saved\Logs\FPS_Chelles.log`.

Journal : lignes `[Wraith]` dans `Saved\Logs\FPS_Chelles.log`.
