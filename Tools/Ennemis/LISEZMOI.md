# Ennemis variés et éparpillés (carte Rachid)

## Lancer
- **Unreal ouvert** sur Rachid : `File > Execute Python Script...` → `Tools\Ennemis\disperser_ennemis.py`, répondre **Oui**.
- **Unreal fermé** : double-clic sur `DISPERSER_ENNEMIS.bat`.

## Ce que ça fait
| Ennemi | Tenue |
|---|---|
| AI_Soldier_Bandit (le premier) | bleu (inchangé) |
| AI_Soldier | rouge |
| AI_Soldier_Boss | noir |
| AI_Charger | vert |
| AI_Genesis | orange |
| AI_Patrol | violet |
| AI_Patrol1 | jaune |

- Ajoute des ennemis pour arriver à 40 (réglage `TOTAL_ENNEMIS` en haut du script).
- Éparpille tous les ennemis sur le NavMesh de la ville, à 35 m les uns des autres et à 40 m au moins du départ du joueur.
- Aucun Blueprint modifié. Les zombies de l'invasion ne sont pas touchés.
- Relancer le script = nouvelle disposition au hasard.

## Annuler
`retirer_ennemis.py` (ou `RETIRER_ENNEMIS.bat`) : enlève les ennemis ajoutés et remet les autres comme avant.
Copie de la carte faite avant chaque passage : `Tools\Ennemis\sauvegarde\`.

## Régler une couleur
Ouvrir `Content/Bandit_Sport/Material/Chelles/MI_Bandit_clothes_Orange` (ou Violet, Jaune) et changer **Couleur**.
Tenue trop sombre ou trop claire : paramètre **Intensite**.
