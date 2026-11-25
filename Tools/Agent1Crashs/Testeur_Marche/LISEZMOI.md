# Test « Marcher tout droit » (25/11/2026, agent 1)

Le test automatique (ChellesTesteur, PR #73) donnait 0,0 m à l'étape « Marcher tout droit ».

Cause : le testeur faisait marcher le personnage droit vers l'endroit où regardait la caméra
après la téléportation en Zone 9, souvent un mur. Le rapport ne disait pas pourquoi il n'avançait pas.

Correctif (dans `TesteurChelles.cpp`, fonction `AjouterMarche`, seule partie changée) :
- au début de l'étape, 16 directions sont essayées (balayage de 6 m) et la première libre est prise ;
- la vitesse maximale et la classe du personnage sont notées dans le rapport ;
- si le perso n'avance toujours pas, le rapport donne la cause : commandes bloquées par un menu,
  mouvement coupé (MOVE_None), vitesse à zéro, resté en l'air, ou mur.

À placer dans `Plugins/ChellesTesteur/Source/ChellesTesteur/Private/` (déjà fait sur le PC,
copie d'avant `TesteurChelles.cpp.avant_marche`), puis lancer `Tools\Testeur\REPARER_TEST_MARCHE.bat`.
