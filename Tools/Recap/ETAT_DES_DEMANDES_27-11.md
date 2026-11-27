# CHELLES 3D : état de toutes les demandes (27/11/2026)

Relu : la mémoire du projet (130 fiches), les 90 PR du dépôt, les journaux `Saved\Logs` d'aujourd'hui, le rapport du testeur automatique (26/11 19h15) et la vérification des erreurs du soir.

## À lancer à ton retour (un seul fichier, Unreal fermé)

`C:\Users\CISCO\Documents\Unreal Projects\CHELLES 3D\TOUT_INSTALLER_27-11.bat` (30 à 60 min)

| Étape | Ce que ça installe | Pourquoi c'était en attente |
|---|---|---|
| 1 | Sauvegarde de la carte Rachid dans `_Sauvegardes\Avant_TOUT_INSTALLER_27-11_<heure>` | Sécurité avant de toucher la carte |
| 2 | RACHID VIP (code 8466), animations de Mr.JONES, recompilation de ChellesMenu, ChellesWraith et ChellesFun | `CREER_PERSONNAGES.bat` jamais relancé depuis le 26/11 13h53 (pas de dossier `Joueur` pour Rachid ni Mr.Jones) |
| 2 | Correction : erreur « Cannot create SoftObjectPath with short package name 'Soldat' » quand on choisit le sexe/perso dans HABILLAGE (vue ce matin à 11h23) | Nouveau, trouvé dans le journal |
| 3 | NOUVEAU : CADEAU DU JOUR et pièces x2 le week-end (ChellesCommerces) | Idée pour faire revenir les joueurs |
| 4 | Routes en MI_AsphaltRoad, croisements à la même hauteur, liste des fleurs | `AMELIORER_ROUTES.bat` du 26/11 jamais lancé (pas de journal) |

Le résumé s'écrit dans `TOUT_INSTALLER_27-11.log`. Si une ligne dit ECHEC, envoie-la.

## Fait et installé sur le PC

Les 50 plugins Chelles sont compilés et à jour (DLL plus récente que les sources), sauf ChellesMenu et ChellesWraith qui seront recompilés par l'étape 2.

| Domaine | Fait |
|---|---|
| Ville | Routes, trottoirs, sol cartoon, Stylized Landscape, végétation/rivière/lac, bâtiments habillés, Tour Gambetta, NavMesh, Map Check |
| Vie urbaine | Passants qui parlent (et IA du VPS), circulation 20 voitures + feux tricolores, policiers aux carrefours, bus, tramway, voitures garées Horo |
| Ambiance | Cycle jour/nuit, météo réelle, neige, pluie et flaques, Noël, fêtes du calendrier |
| Combat | Armes FPS_Game, arsenal lourd, coffres, caisses de munitions, tourelle, drone, ennemis variés, bots volants, zombies de feu, invasion toutes les 2 h, boss COLOSSE, PvP + kit de survie |
| Joueur | HUD, mini-carte GPS, XP et pouvoirs, jetpacks, accessoires, sauvegarde de progression, HABILLAGE lobby, héros Paragon/Wraith, LENA, SOFIA, LUCIA, BRENDA, Shems VIP, appartement (F1) |
| Activités | Missions (F10), courses de rue, commerces et pièces, Dernier Refuge + classement, défis de la semaine, mode photo, vol de voiture, motoneige, SUV stable |
| Outils | Testeur automatique du soir, vérification des erreurs, sauvegarde auto, écran de chargement, démo 4 (D:\DEMO 4), serveur dédié VPS, chat global (B) |

## Attend une réponse de toi

| Sujet | Ce qu'il faut |
|---|---|
| Mr.JONES jouable / VIP | Il n'a pas d'animation « debout au repos » (Idle Mixamo) : en fournir une, puis dire s'il devient VIP |
| PREVOR | Fil dédié en cours |
| Défis de la semaine sur Discord | Fil en attente de ta réponse |
| Agent 2 Armements | Fil en attente de ta réponse |
| Routes 2 voies / 4 voies | Le script fait 2 voies partout, dis si certaines rues doivent passer à 4 |
| Multijoueur par Internet | Choisir Steam ou EOS (le LAN marche déjà) |

## Volontairement pas fait (décisions déjà prises)

HTML5 (en pause), thème Cyberpunk (abandonné), touches F1-F12 = modes (supprimées le 24/11), FastDDS (inutile en 4.27).

## À surveiller (pas cassé, mais à améliorer)

- **Fluidité** (test du 26/11, en mode éditeur, donc plus lent que le vrai jeu) : 33 img/s en moyenne, mais Zone 1 sud-ouest 13 img/s et Zone 2 sud 19 img/s, voiture 21 img/s avec une saccade de 1,7 s. C'est la prochaine priorité.
- **21 fichiers introuvables** venant de packs supprimés (CityPark, Venice bricks, Safe_House...) : sans plantage, ils font seulement des avertissements.
- **Avertissements BP_building** (pack Venice) au chargement : sans gravité.
- **Dépôt GitHub** : 66 PR (#24 à #90) sont restées en brouillon et `main` s'arrête à la fusion #23. C'est le PC qui fait foi. Une fusion groupée comme la #23 est possible si tu la veux.
