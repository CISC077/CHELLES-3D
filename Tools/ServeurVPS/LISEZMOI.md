# CHELLES 3D sur le VPS : serveur multijoueur dédié

But : un serveur CHELLES 3D allumé 24 h/24 sur le VPS (51.178.54.208).
Les joueurs lancent le jeu sur leur PC, puis **MULTI-PLAYER > ADRESSE IP SERVEUR > 51.178.54.208 > REJOINDRE**.

## Le principe en 3 phrases

1. On n'envoie pas le projet Unreal sur le VPS : on fabrique sur le PC un **serveur dédié Linux** (le jeu sans image ni son, qui ne fait que gérer la partie), et c'est lui qu'on envoie.
2. Le Unreal du Launcher ne sait pas fabriquer ce serveur. Il faut une seconde copie d'Unreal 4.27, **compilée depuis le code source** (une seule fois). Votre Unreal du Launcher reste là, vous continuez à travailler avec.
3. Le serveur et le jeu donné aux joueurs doivent sortir **du même Unreal** : le script fabrique donc les deux en même temps.

Temps total la première fois : une journée (surtout de l'attente). Les mises à jour ensuite : 1 à 3 heures.

Ce qu'il faut : ~150 Go libres sur D:, ~30 Go libres sur C:, une connexion Internet correcte.

---

## Étape 1 : vérifier Visual Studio (10 min)

Ouvrez **Visual Studio Installer** > bouton **Modifier** sur votre Visual Studio > onglet **Composants individuels**, et cochez s'ils ne le sont pas :

- **MSVC v142 - VS 2019 C++ x64/x86 build tools** (le plus récent)
- **Windows 10 SDK** (10.0.18362 ou plus récent)
- **.NET Framework 4.6.2 targeting pack** (ou « pack de ciblage »)

Puis **Modifier** en bas à droite.

## Étape 2 : relier votre compte Epic à GitHub (15 min, puis attendre un mail)

Le code d'Unreal est gratuit mais privé : il faut prouver que vous avez un compte Epic.

1. Connectez-vous sur https://www.epicgames.com/account/connections
2. Onglet **Comptes**, sur **GitHub** cliquez **Connecter**, et acceptez avec votre compte GitHub (CISC077).
3. Vous recevez un mail de GitHub « invitation to join @EpicGames ». Cliquez **Join @EpicGames**. (Ça peut prendre de quelques minutes à quelques heures.)
4. Vérification : la page https://github.com/EpicGames/UnrealEngine doit s'ouvrir (sinon « 404 » = invitation pas encore acceptée).

## Étape 3 : télécharger le code source et la toolchain Linux (1 h)

1. Code source 4.27 : https://github.com/EpicGames/UnrealEngine/archive/refs/heads/4.27.zip
   Dézippez-le dans **D:\UE_4.27_Source** (chemin court, sans espace, important).
   Le bon dossier est celui qui contient **Setup.bat** et le dossier **Engine**. Si 7-Zip a créé `D:\UE_4.27_Source\UnrealEngine-4.27\`, ce n'est pas grave : vous donnerez ce chemin-là au script.
2. Toolchain Linux (permet de fabriquer un programme Linux depuis Windows) :
   https://cdn.unrealengine.com/CrossToolchain_Linux/v19_clang-11.0.1-centos7.exe
   Installez-la avec les choix par défaut.
3. **Redémarrez le PC** (pour que Windows prenne en compte la toolchain).

## Étape 4 : préparer Unreal « source » (3 à 6 h, une seule fois)

Double-clic sur **Tools\ServeurVPS\1_PREPARER_UNREAL_SOURCE.bat**.
Il vérifie tout, télécharge les fichiers d'Unreal (~15 Go), puis compile Unreal. Laissez la fenêtre ouverte jusqu'à **TERMINÉ**.

## Étape 5 : fabriquer le serveur (1 à 4 h)

1. Fermez Unreal.
2. Double-clic sur **Tools\ServeurVPS\2_CREER_LE_SERVEUR.bat**.
3. Il demande : dossier de sortie (Entrée), **D**émo ou jeu **C**omplet pour les joueurs, et s'il fabrique aussi le jeu Windows (répondez **O**).

À la fin, dans **D:\CHELLES3D_SERVEUR** :

- `CHELLES3D_Serveur_Linux.tar.gz` : le serveur, à envoyer sur le VPS
- `installer_serveur.sh` : le script d'installation pour le VPS
- `WindowsNoEditor\` : le jeu à donner aux joueurs (c'est **lui** qui sait se connecter au serveur)

Note : la prochaine fois que vous ouvrez le projet avec le Unreal du Launcher, il peut demander de recompiler des modules. Répondez **Oui** (ou relancez `Compiler_les_plugins.bat`).

## Étape 6 : envoyer sur le VPS avec FileZilla (selon votre connexion)

Dans FileZilla, **Fichier > Gestionnaire de sites > Nouveau site** :

- Protocole : **SFTP - SSH File Transfer Protocol**
- Hôte : **51.178.54.208**   Port : **22**
- Utilisateur : **ubuntu**, et le même mot de passe (ou la même clé) que pour `ssh`

Connectez-vous, allez dans **/home/ubuntu** (à droite), et glissez-y les deux fichiers
`CHELLES3D_Serveur_Linux.tar.gz` et `installer_serveur.sh`.

## Étape 7 : installer le serveur (5 min)

Ouvrez un terminal Windows (touche Windows, tapez `cmd`) et copiez-collez ligne par ligne :

```
ssh ubuntu@51.178.54.208
```

puis, une fois connecté au VPS :

```
sudo bash installer_serveur.sh
```

Le script installe tout, ouvre le port 7777 (UDP) dans le pare-feu du VPS (sans couper SSH ni le site web), et démarre le serveur. Il affiche **SERVEUR CHELLES 3D EN LIGNE** quand c'est bon.

## Étape 8 : pare-feu OVH (2 min, seulement s'il est activé)

Dans l'espace client OVH : **Bare Metal Cloud > Network > IP publiques** (ou **IP**), sur la ligne **51.178.54.208**, menu **…** > **Configurer le Edge Network Firewall**.

- Si le pare-feu est **désactivé** : rien à faire.
- S'il est **activé** : ajoutez une règle **Autoriser**, protocole **UDP**, port de destination **7777**.

## Étape 9 : tester

Lancez `D:\CHELLES3D_SERVEUR\WindowsNoEditor\VehicleGame.exe` > **MULTI-PLAYER > ADRESSE IP SERVEUR** > tapez `51.178.54.208` > **REJOINDRE**.

Pour les amis : zippez `WindowsNoEditor` et remplacez la démo dans `/var/www/html/download` (l'ancienne démo faite avec le Launcher risque de dire « version incompatible »).

---

## Mettre à jour le serveur plus tard

Refaites les étapes 5, 6 et 7 (le script garde l'ancienne version dans `/opt/chelles3d/ancien`). Redonnez aussi le nouveau jeu aux joueurs : serveur et jeu doivent toujours venir de la même fabrication.

## Commandes utiles sur le VPS

| Pour… | Tapez |
|---|---|
| Voir si le serveur tourne | `sudo systemctl status chelles3d` |
| Voir ce qu'il raconte en direct | `sudo journalctl -u chelles3d -f` (Ctrl+C pour sortir) |
| Le redémarrer | `sudo systemctl restart chelles3d` |
| L'arrêter | `sudo systemctl stop chelles3d` |
| Le journal Unreal complet | `sudo less /opt/chelles3d/serveur/FPS_Chelles/Saved/Logs/FPS_Chelles.log` |
| Vérifier qu'il écoute | `sudo ss -ulnp \| grep 7777` |

Le serveur redémarre tout seul s'il plante et au redémarrage du VPS.
**Ne lancez jamais** `do-release-upgrade` : on reste en Ubuntu 22.04.

## Plus simple mais moins pratique

Sans VPS : un joueur choisit **LAN LOCAL** sur son PC et ouvre le port **7777 UDP** de sa box vers son PC ; les autres tapent son IP publique. Son PC doit rester allumé pendant la partie.
