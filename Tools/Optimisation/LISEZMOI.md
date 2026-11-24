# Optimisation finale CHELLES 3D (24/11/2026)

## Ce que Rachid doit faire (une seule fois)
1. Fermer Unreal.
2. Double-cliquer `Tools\Optimisation\OPTIMISER_TOUT.bat`.
3. Attendre « TERMINE » (5 à 15 minutes), puis ouvrir le projet.

## Ce qui a changé
- **Config\DefaultEngine.ini** (déjà en place) : la mémoire des textures ne dépasse plus jamais
  la carte graphique (`r.Streaming.LimitPoolSizeToVRAM=1`), les objets du décor s'ajoutent par
  petits paquets, et le jeu exporté charge ses fichiers sur un fil à part (`s.AsyncLoadingThreadEnabled`).
  Aucun de ces réglages ne relance la compilation des shaders.
- **Préférences de l'éditeur** (déjà en place) : Unreal ne rouvre plus au démarrage toutes les
  fenêtres d'assets laissées ouvertes (gros gain de mémoire et de temps), et ralentit quand il est en arrière-plan.
- **ChellesInventaire + ChellesTourelle** (à compiler, fait par le .bat) : 5 fois par seconde, ils
  passaient en revue TOUS les objets chargés (plus d'un million dans Rachid) pour trouver un menu.
  Ils utilisent maintenant la liste rapide des widgets d'Unreal : moins de petites saccades régulières.
- **Squelettes réparés à chaque ouverture** (fait par le .bat) : 22 squelettes (armes FPS_Game,
  voitures, Sport_Female...) étaient « réparés » par Unreal à chaque ouverture du projet. Ils sont
  re-enregistrés une fois pour toutes. Copies dans `_Sauvegardes\avant_optim_2026-11-24`.

## Annuler
`Tools\Optimisation\ANNULER_OPTIMISATION.bat` (Unreal fermé) remet les .ini, les 2 fichiers
de code et les squelettes. Sauvegardes : `*.avant_optim_2026-11-24` et `*.avant_optim`.
