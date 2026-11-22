# ChellesWraith : Wraith (Paragon) à la place du SOLDAT

Le joueur **SOLDAT** (FPS_Game, case "Soldat" de la roue TAB / F1) prend l'apparence de
**Wraith** du pack gratuit *Paragon: Wraith* (déjà dans `Content/ParagonWraith`).

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
  (GameUserSettings.ini, section `[ChellesWraith]`). En multijoueur, tout le monde voit le même skin.

| Skin | D'où il vient |
|---|---|
| VERT MILITAIRE | skin ODGreen du pack |
| ROUGE ET NOIR | skin LunarOps recoloré en rouge par le plugin |
| BLANC ET CYAN | skin LunarOps du pack |
| WRAITH CLASSIQUE | Wraith de base du pack |
| SOLDAT D'ORIGINE | le soldat sans Wraith |

Pas actif en Spider-Male / Spider-Femelle / PANTERA (la roue TAB habille déjà le soldat).

## Installer
1. Fermez Unreal.
2. Double-cliquez `Plugins\ChellesWraith\Compiler_le_plugin.bat` (1 à 5 minutes).
3. Rouvrez le projet, lancez **Play**, appuyez sur **K**.

## Réglages (Wraith.txt)
Bloc-notes, puis relancer le jeu ou console (touche ²) `Chelles.Wraith Relire`.
- `Actif=Non` : plugin coupé.
- `Skin=…` : ajouter / enlever / renommer des skins, changer la teinte (R,G,B).
- `ArmeWraith=Non` : garder les armes du soldat.
- `Voix=0` : Wraith muet.

## Console (touche ²)
- `Chelles.Wraith` : état.
- `Chelles.Wraith 0` / `1` : soldat d'origine / Wraith.
- `Chelles.Wraith Skin 2` : choisir un skin.
- `Chelles.Wraith Arme 0 0 5`, `ArmeRot 0 90 0`, `ArmeEchelle 0.7` : recaler le fusil en
  1re personne (la ligne à recopier dans Wraith.txt s'affiche).
- `Chelles.Wraith Params` : écrit les couleurs des matériaux dans `Saved\Logs\FPS_Chelles.log`.

Journal : lignes `[Wraith]` dans `Saved\Logs\FPS_Chelles.log`.
