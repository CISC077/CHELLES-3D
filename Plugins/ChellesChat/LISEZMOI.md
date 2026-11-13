# Chat global CHELLES 3D (jeu + site)

Le jeu se branche sur le même chat que la fenêtre du site chelles3d.fr
(serveur du VPS `ws://51.178.54.208:8080/chat`).
Dans la fenêtre, 🎮 = un joueur du jeu, 🌐 = un visiteur du site.

## Installer (une seule fois)

1. Fermer Unreal.
2. Double-clic sur `Plugins\ChellesChat\Compiler_le_plugin.bat` → attendre **TERMINE**.
3. Double-clic sur `Plugins\ChellesMenu\Compiler_le_plugin.bat` (pour la nouvelle ligne dans OPTIONS) → attendre **TERMINE**.
4. Ouvrir le projet CHELLES 3D.

## Utiliser

- En jeu, touche **B** : la fenêtre du chat global s'ouvre à gauche.
- La première fois, écrire son **pseudo** (3 à 20 lettres ou chiffres) puis ENTRÉE. Il est retenu pour les fois suivantes.
- Écrire un message puis ENTRÉE : il part à tout le monde (jeu + site). ÉCHAP = fermer.
- Changer de pseudo : écrire `/pseudo NouveauNom` dans la fenêtre.
- Couper le chat global : ÉCHAP > OPTIONS > JEU > **CHAT GLOBAL SITE + JEU (B) : OFF**.
- Quand la fenêtre est fermée, les nouveaux messages s'affichent 8 secondes puis disparaissent.

La touche ENTRÉE garde son rôle : panneau de droite (chat entre joueurs d'une même partie + aide).

## Réglages

- Pseudo et option : `Saved\Config\Windows\GameUserSettings.ini`, sections `[ChellesChat]` (Pseudo) et `[ChellesMenu]` (ChatGlobal).
- Autre adresse de serveur (facultatif) : `Config\DefaultGame.ini`, ajouter
  ```
  [ChellesChat]
  Adresse=ws://51.178.54.208:8080/chat
  ```
- Console (touche au-dessus de Tab) : `Chelles.ChatGlobal` écrit l'état de la connexion dans le journal,
  `Chelles.ChatGlobalPseudo Nom` change le pseudo.

## Bon à savoir

- Le serveur multijoueur du VPS (sans écran) ne se connecte jamais au chat global.
- Expulsé ou banni par un admin : pas de reconnexion automatique. Remettre l'option OFF puis ON pour réessayer après une expulsion.
- Coupure du serveur : nouvel essai tout seul après 2, 4, 8, 16 puis 30 secondes.
- Les emojis 🎮 🌐 utilisent la police emoji de Windows ; si elle manque, le jeu écrit [JEU] / [SITE].

## Retirer

Supprimer le dossier `Plugins\ChellesChat`. Rien d'autre à défaire (la ligne d'OPTIONS reste, elle ne fait alors plus rien).
