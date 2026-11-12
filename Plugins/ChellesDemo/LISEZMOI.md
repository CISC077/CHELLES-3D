# Demo beta CHELLES 3D (20 minutes)

1. Fermer Unreal.
2. Double-clic sur `Plugins\ChellesDemo\Compiler_le_plugin.bat` (une seule fois).
3. (Conseille) Ouvrir le projet, lancer Play, touche ² puis `Chelles.Demo 1` : au bout d'1 minute de jeu l'ecran FIN DE LA DEMO s'affiche. `Chelles.Demo 0` pour arreter. Dans l'editeur la demo n'est jamais active toute seule.
4. Fermer Unreal, double-clic sur `Tools\Demo\CREER_LA_DEMO.bat` (1 a 3 heures).
5. Le jeu est dans `D:\CHELLES3D_DEMO\WindowsNoEditor` (+ ZIP si demande). Le tester avant de l'envoyer.

Reglages : `Config\DefaultGame.ini`, section `[ChellesDemo]` (Active, DureeMinutes, TempsCumule, Lien, Message).
Pour exporter un jour le jeu complet : `Active=False`.
Retour arriere : `Config\DefaultGame.ini.avant_demo` + supprimer `Plugins\ChellesDemo`.

En cas d'echec : `Tools\Demo\Erreurs_demo.txt` et le dossier `Tools\Demo\Journal`.
