# Chelles Pluie (plugin ChellesPluie)

Ce que fait la pluie, toute seule à chaque partie (rien n'est modifié dans la map Rachid ni dans `MI_AsphaltRoad`) :

| Quoi | Comment |
|---|---|
| **Averses** | En mode `Auto`, il pleut au hasard (30 % de chance toutes les 20 minutes, la même heure pour tous les joueurs). La pluie arrive et s'en va doucement (25 s). |
| **Gouttes** | Des traits de pluie tombent autour de la caméra (à pied, en voiture, en vol). Plus clairs le jour, discrets la nuit. Presque invisibles sous un toit ou un pont. |
| **Bruit de pluie** | En boucle, plus doux à l'abri. |
| **Flaques** | 160 flaques posées **sur les routes en MI_AsphaltRoad** (decals, le matériau des routes n'est pas touché). Elles se remplissent en 2 minutes de pluie et sèchent en 6 minutes. Elles brillent et reflètent les lumières (réglage REFLETS des OPTIONS). |
| **Sol mouillé** | Autour du joueur, le sol devient plus sombre et brillant pendant la pluie. |
| **Ciel gris** | Un peu plus de brouillard et des couleurs plus ternes pendant l'averse. |
| **Neige** | Pendant une averse, la neige de ChellesMeteo s'arrête, puis revient après. |

## Installer (une seule fois)

1. Fermer Unreal.
2. Double-cliquer `Tools\Pluie\INSTALLER_PLUIE.bat` (compile les plugins, crée les matériaux et le son, 5 à 15 minutes).
3. Ouvrir le projet, Play.

## PC faible

**OPTIONS > AVANCÉS > PLUIE / FLAQUES** :
- `AUTO` : suit la QUALITÉ (BAS = LÉGÈRE, sinon COMPLÈTE) ;
- `NON` : jamais de pluie ;
- `LÉGÈRE` : moitié moins de gouttes + le bruit, sans flaques ni sol mouillé ;
- `COMPLÈTE` : tout.

## Tester tout de suite (console, touche ²)

- `Chelles.Pluie Oui` : averse tout de suite. `Chelles.Pluie Non` : couper. `Chelles.Pluie Auto` : retour à la météo.
- `Chelles.Pluie Flaques` : remplir les flaques tout de suite (sans attendre 2 minutes).
- `Chelles.Pluie Info` : averse, force, flaques, options, abri.
- `Chelles.Pluie Relire` : relire `Pluie.txt`.

## Régler

Tout est dans `Plugins\ChellesPluie\Pluie.txt` (Bloc-notes, pas besoin de recompiler) : chance d'averse, nombre et vitesse
des gouttes, volume, nombre et taille des flaques, temps de remplissage / séchage, brouillard...

Journal : lignes `[Pluie]` dans `Saved\Logs\FPS_Chelles.log`.
Pour tout enlever : supprimer le dossier `Plugins\ChellesPluie`.
