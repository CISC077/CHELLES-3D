# ChellesCommerces : boulangerie, tabac-presse, bar et pièces du jeu

Ajouté le 26/11/2026 pour Rachid (idée « commerces ouverts »).

## Ce que ça fait

- **PIÈCES** : la monnaie du jeu. Tu en gagnes en tuant des zombies (3) et des ennemis (5), et avec l'XP
  (1 pièce pour 4 XP, donc environ 2 à 3 par minute de jeu). Tu commences avec 50 pièces.
  Le compteur est à droite de l'écran. Les pièces sont gardées sur ton PC
  (`Saved\SaveGames\ChellesCommerces.sav`).
- **BOUTIQUES** : entre dans la BOULANGERIE, le TABAC-PRESSE ou le CHELLES BAR et avance jusqu'au comptoir.
  La boutique s'ouvre toute seule avec la souris : clique sur un article pour l'acheter.
  FERMER la ferme ; elle se rouvre si tu t'éloignes puis reviens. **Aucune nouvelle touche.**

| Commerce | Articles |
|---|---|
| Boulangerie | Croissant +25 vie (10) · Pain au chocolat +50 vie (20) · Baguette vie au max (35) · Gâteau +3 vie/s pendant 30 s (45) |
| Tabac-presse | Journal +40 XP (15) · Carte à gratter jusqu'à 150 pièces (25) · Munitions armes lourdes (40) · Magazine +120 XP (40) |
| Chelles Bar | Café course x1,4 45 s (15) · Soda saut x1,6 45 s (20) · Diabolo menthe +40 vie (15) · Cocktail sans alcool bouclier 50 % 60 s (60) |

Prix, articles et gains : `Commerces.txt` (Bloc-notes, pas besoin de recompiler).

## Installer

Unreal FERMÉ, double-clic sur `Tools\Commerces\INSTALLER_COMMERCES.bat` : il compile le plugin puis construit
la boulangerie et le tabac-presse près du CHELLES BAR dans la carte Rachid (copie de Rachid faite avant).

## Console (touche au-dessus de Tab)

- `Chelles.Commerces` : pièces et commerces trouvés dans la carte
- `Chelles.Commerces Pieces 200` : ajouter 200 pièces (test)
- `Chelles.Commerces Ouvrir Boulangerie` (ou Tabac, Bar) : ouvrir la boutique n'importe où (test)
- `Chelles.Commerces Relire` : relire Commerces.txt

## Multijoueur

Chaque joueur garde ses pièces sur son PC. Le serveur donne les bonus (vie, vitesse, bouclier...).

## Technique

- Aucun autre plugin ni Blueprint modifié : vie (Health/MaxHealth), XP (PouvoirsChelles.GagnerXPBonus,
  XPLocale) et munitions (ArsenalRelais.ChargeMitrailleuse / ChargeFlammes) sont lues par leur nom.
- Comptoirs repérés par étiquette : ChellesComptoir_Boulangerie, ChellesComptoir_Tabac, ChellesBarComptoir.
- Journal : lignes `[Commerces]` dans Saved\Logs.
