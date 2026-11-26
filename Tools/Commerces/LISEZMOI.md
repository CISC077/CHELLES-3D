# Commerces : boulangerie et tabac-presse dans Rachid

Unreal FERMÉ, double-clic sur `INSTALLER_COMMERCES.bat` : compile le plugin ChellesCommerces, puis construit
une BOULANGERIE et un TABAC-PRESSE (9 x 7 m, pack MMSupermarket) près du CHELLES BAR, collés à un mur,
hors de la route. Une copie de Rachid est faite avant dans `Tools\Commerces\sauvegarde\`.
Relancer = reconstruit au même endroit. Journal : `commerces.log`.

- Enlever : `RETIRER_COMMERCES.bat` (le bar n'est pas touché).
- Comptoir à l'envers : dans `installer_commerces.py`, mettre `TOURNER_COMPTOIR_BOULANGERIE = True`
  (ou `TOURNER_COMPTOIR_TABAC`) puis relancer.
- Choisir la place à la main : `POSITION_BOULANGERIE = (X, Y, Z, Yaw)` (milieu de la porte au sol,
  Yaw = de la porte vers le fond), pareil pour `POSITION_TABAC`.
- Si le bar n'existe pas : `Tools\Bar\INSTALLER_LE_BAR.bat` d'abord (sinon les commerces sont mis
  près du départ du joueur).
