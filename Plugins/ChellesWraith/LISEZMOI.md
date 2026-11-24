# ChellesWraith : Wraith, Dekker, Sci-Fi Soldier, Aurora et Boris à la place du SOLDAT

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

**Aurora** et **Boris** ont leurs animations, leur voix (anglais) et leur animation de mort, mais pas
d'arme à feu dans leur pack : en 1re personne ils gardent les armes du soldat. Comme tous les persos,
ils ont tous les pouvoirs (J / Y), le vol (ESPACE en l'air) et le flash X (plugin ChellesPouvoirs / ChellesArmes).

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
| AURORA | Aurora de base (*Paragon: Aurora*, `Content/ParagonAurora`) |
| AURORA COEUR GIVRE / IMPERATRICE GLACIALE / CRISTAL DE LUNE / PRINTEMPS | les 4 skins du pack Aurora (FrozenHearth, GlacialEmpress, MoonCrystal, SpringCeremony) |
| BORIS | Boris (*Paragon: Boris*, `Content/ParagonBoris`) |
| SOLDAT D'ORIGINE | le soldat sans héros |

En Spider-Male / Spider-Femelle / PANTERA (roue TAB / F2-F4), le soldat garde le héros choisi
et **K change la couleur de sa tenue** (lignes `Tenue=` de Wraith.txt) :

| Héros | Tenues (ordre de K) |
|---|---|
| Spider-Male | d'origine, VERT, BLEU ET VERT, DORE, NOIR |
| Spider-Femelle | d'origine, VIOLETTE, VERTE, ORANGE |
| PANTERA | d'origine, ROUGE, BLEUE, DOREE, BLANCHE |

À préparer **une fois** : dans Unreal, *Fichier > Exécuter un script Python…* >
`Tools\TenuesHeros\preparer_tenues_heros.py`. Il ajoute 3 réglages (`ChellesTeinte`, `ChellesUnie`,
`ChellesMelange`) aux matériaux de ces héros, sans rien changer à leur image par défaut
(copies dans `/Game/Personnages/Sauvegarde/TenuesHeros`). En multijoueur, la tenue passe par le relais.

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

## LENA (24/11/2026)

- Ligne `Heros=Lena|Anims:...` : un heros SANS Animation Blueprint. Le plugin joue lui-meme ses animations Mixamo
  selon le soldat : repos, marche, course, vol (pouvoir ESPACE), accroupi, chute. `chemin@0` = image figee.
  Deux valeurs en plus a la fin de la ligne Heros= : taille et rotation (degres).
- `Skin=LENA` (avant SOLDAT D'ORIGINE). Elle se choisit aussi dans **JOUER > HABILLAGE > LENA** (plugin ChellesMenu,
  qui ecrit `[ChellesWraith] Demande=LENA` dans GameUserSettings.ini ; ce plugin la lit deux fois par seconde).
- L'arme 3e personne du soldat est cachee sur LENA (ses mains ne visent pas). En 1re personne : armes du soldat.
- Le soldat du jeu (BP_Character_SpiderMan, corps SK_SpiderMan_UE4) est maintenant reconnu : avant, seul un corps
  "Mannequin" l'etait, et aucune apparence ne s'affichait. Sans choix enregistre, on reste en SOLDAT D'ORIGINE.
  Sur ce soldat en tenue d'origine, K garde son role d'avant (couleurs de la tenue) ; une fois habille (LENA...),
  K passe a l'apparence suivante.
- Console : `Chelles.Wraith Skin LENA` (un nom marche aussi, pas seulement un numero).

## SOFIA (24/11/2026)

- Meme principe que LENA : `Heros=Sofia|Anims:...` + `Skin=SOFIA`, bouton SOFIA dans HABILLAGE.
- Animations nommees `A_<Nom>_<Etat>_Anim` (nom donne par Unreal a l'import). Saut (chute) joue une fois, comme accroupi.
- Import des personnages : `Tools\Personnages\creer_personnages.py` (remplace `Tools\Lena\creer_lena.py`).

## LUCIA (24/11/2026)

- Meme principe : `Heros=Lucia` + `Skin=LUCIA`, bouton LUCIA dans HABILLAGE, fichiers Mixamo dans `Bureau\LUCIA 3D\Animations`.

## DANSES (24/11/2026)

- `ToucheDanse=Alt+D` (D seul = aller a droite en ZQSD, Y = changer de pouvoir). Modificateurs Alt+, Ctrl+, Shift+.
- `Danses=Lena|anim;anim...` : jouees a la suite en boucle ; bouger (apres 0,7 s), sauter ou voler arrete la danse.
- `DanseArretSeulement=Oui` : la touche ne marche qu'immobile au sol.
- Multijoueur : AWraithRelais::bDanse (RPC ServeurDanse), les autres joueurs voient la danse.
- En 1re personne on ne voit pas son corps : W = camera pour la voir.
