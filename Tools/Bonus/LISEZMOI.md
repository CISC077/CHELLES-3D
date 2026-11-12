# Bonus et armes au pied des murs (carte Rachid)

## Lancer
- **Unreal ouvert** sur Rachid : `File > Execute Python Script...` → `Tools\Bonus\disperser_bonus.py`, répondre **Oui**.
- **Unreal fermé** : double-clic sur `DISPERSER_BONUS.bat`.

## Ce que ça pose
| Blueprint FPS_Game | Nombre |
|---|---|
| Replicated_Health (vie, cube rouge) | 10 |
| Replicated_GrenadePickup (grenades, cube vert) | 6 |
| Replicated_Ammo (munitions) | 8 |
| Replicated_Factory_Rifle / Shotgun / Sniper / Laser / Launcher (socle avec arme animée) | 3 / 2 / 2 / 2 / 2 |

- Chaque bonus est posé au niveau de la rue, à 80 cm devant le pied d'un mur de bâtiment, tourné vers la rue.
- 30 m minimum entre deux bonus (réduit tout seul si la place manque).
- Dossier Outliner **Bonus**. Aucun Blueprint modifié, les bonus posés à la main ne bougent pas.
- Relancer le script = les anciens bonus du script sont enlevés et une nouvelle disposition est tirée au hasard.

## Régler
En haut de `disperser_bonus.py` : la liste `BONUS` (nombre de chaque sorte, 0 = aucun), `ESPACEMENT`, `DISTANCE_MUR`.

## Annuler
`retirer_bonus.py` (ou `RETIRER_BONUS.bat`) : enlève seulement les bonus posés par le script.
Copie de la carte faite avant chaque passage : `Tools\Bonus\sauvegarde\`.
Journal : `Tools\Bonus\bonus.log`.
