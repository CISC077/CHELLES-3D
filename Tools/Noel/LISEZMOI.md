# Événement de Noël – CHELLES 3D

Du **1er décembre au 1er janvier** (date du PC), le jeu passe tout seul en mode Noël :

- la **neige tombe** (flocons de `Tools/Neige`) et vos pas marquent le sol ;
- le **sol devient blanc** (seulement les parties à peu près plates du Landscape) ;
- chaque **passant porte un bonnet de Noël** (pack `Content/Christmas_pack`, bonnets `SM_decoration_hat_1` à `4`).
  La Christmas Girl est déjà en tenue : elle n'en reçoit pas ;
- des **cadeaux** (pack `Christmas_pack`, `SM_decoration_present_1` à `8`) sont **éparpillés** par terre et sur
  les trottoirs, jamais sur les routes ni les toits ; ils changent de place chaque jour (80 par défaut) ;
- le **ciel orangé le soir** vient du cycle jour/nuit GoodSky déjà installé (rien à faire).

Le reste de l'année, tout est comme avant : pas de neige, sol d'origine, pas de bonnet.

## Installer (une seule fois)

1. Fermez Unreal.
2. Double-cliquez **`Tools\Noel\INSTALLER_NOEL.bat`** (compile le plugin puis installe, 5 à 15 minutes).
3. À la fin, la fenêtre affiche le résultat. S'il y a « ABANDON » ou « ERREUR », envoyez le texte à Claude.

## Tester avant décembre

- **`TESTER_NOEL_MAINTENANT.bat`** : Noël tout le temps (pour essayer).
- **`NOEL_SELON_LA_DATE.bat`** : remet le fonctionnement normal (selon la date).

Pas besoin de recompiler : arrêtez Play et relancez-le.

## Régler

Ouvrez `Plugins\ChellesNoel\Noel.txt` avec le Bloc-notes : dates, bonnets utilisés, taille du bonnet,
enfoncement, rotation, nombre et taille des cadeaux. Enregistrez puis relancez Play.

## Retirer

**`RETIRER_NOEL.bat`** (Unreal fermé) : enlève la neige et rend au Landscape son matériau d'origine.

## Ce qui est créé

- `Plugins/ChellesNoel` : l'acteur « Evenement de Noel (Chelles) » est ajouté tout seul au lancement.
- `/Game/Chelles/Noel/M_Sol_Noel_*` : **copie** du matériau d'origine du sol + la neige par-dessus
  (le matériau d'origine n'est pas modifié).
- Paramètre `SaisonNeige` dans `/Game/Chelles/Neige/MPC_Empreintes` (0 = sol normal, 1 = neige).
- Tag `ChellesNoelSaison` sur les acteurs `Chelles_Neige_*` de Rachid.

Attention : ne relancez plus `Tools/Neige/ajouter_neige.py` seul, il remettrait la neige toute l'année.
Relancez plutôt `INSTALLER_NOEL.bat`.
