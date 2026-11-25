# Roue grise coincée en plein jeu (25/11/2026, agent 1)

Symptôme : la vieille roue d'armes du kit FPS (4 armes grises en croix, `W_CircleWeaponSelect`)
reste au milieu de l'écran, souris visible, plus rien ne répond.

Cause : le PING du butin a été mis sur la molette (`Butin.txt`, `Ping.Touche=MiddleMouseButton`) le 25/11,
mais la molette ouvrait aussi la roue du kit (`SwitchWeapon` = `MiddleMouseButton` dans `DefaultInput.ini`).
La roue passe la souris en mode interface et ne voit plus le relâchement : elle reste ouverte.

Correctif :
- `Config/DefaultInput.ini` : ligne `SwitchWeapon` / `MiddleMouseButton` retirée (la roue TAB et le menu I la remplacent) ;
- ChellesRaccourcis (`MenuArmements`) : si la roue grise apparaît quand même, elle est retirée toute seule
  au bout d'une demi-seconde sans molette enfoncée, et Échap la retire aussi. La souris revient au jeu.

À lancer : `Tools\RaccourcisF\REPARER_ROUE_COINCEE.bat`.
