# Agents de police aux ronds-points et grands carrefours (plugin ChellesPolicier)

Un agent de police (le personnage `Content/Personnages/Mondes/police_officer`) se tient
au **centre de chaque rond-point** et **au coin de chaque grand carrefour** de la carte.
Il se tourne vers les voitures de la circulation :

- une voiture arrive : il la regarde et **balaie le bras** pour la faire passer ;
- une voiture attend son tour : il **lève la main** vers elle (stop) ;
- personne : il attend, les bras le long du corps.

Les agents n'apparaissent qu'autour du joueur (250 m, 12 au maximum), comme les voitures.
Le plugin ne modifie aucun niveau, aucun Blueprint ni aucun autre plugin : il lit seulement le
réseau routier du plugin **Chelles Circulation** (il faut donc que la circulation soit dans le niveau).

## Installation (une seule fois)

1. **Fermez Unreal.**
2. Double-cliquez sur `Plugins\ChellesPolicier\Compiler_le_plugin.bat` et attendez « TERMINE ».
3. Ouvrez le projet, ouvrez la carte **Rachid**, cliquez sur **Play**.
   En haut à gauche, un message vert dit combien de ronds-points et de carrefours sont surveillés.

Le fichier `Plugins\ChellesPolicier\Policier_Rapport.txt` liste ensuite tous les postes trouvés.

## Réglages : `Plugins\ChellesPolicier\Policier.txt`

| Réglage | Par défaut | Rôle |
|---|---|---|
| Actif | 1 | 0 = plus aucun agent |
| RondsPoints / GrandsCarrefours | 1 / 1 | où poser des agents |
| BranchesMinimum | 4 | un grand carrefour = au moins 4 rues qui se croisent |
| BranchesPrincipales | 1 | … dont au moins 1 rue à 50 km/h |
| EcartCarrefour | 750 | distance (cm) entre le centre du carrefour et l'agent |
| DistanceApparition | 25000 | les agents apparaissent à moins de 250 m du joueur |
| AgentsMaximum | 12 | nombre d'agents en même temps |
| Taille | 180 | taille de l'agent (cm) |
| CorrectionRotation | (vide) | si l'agent regarde de côté : -90, 90 ou 180 |

Après une modification : relancez **Play**, ou tapez `Chelles.Policier Relire` dans la console (touche ²).

Console : `Chelles.Policier Info` (état), `Chelles.Policier Montrer` (dessine les postes 30 s :
bleu = rond-point, jaune = carrefour), `Chelles.Policier Liste` (réécrit le rapport).

## Gestes : sans rien faire, ou avec de vraies animations Mixamo (facultatif)

Sans animation, le plugin **calcule lui-même les bras** de l'agent (passez / stop / repos).
Pour des mouvements plus naturels, vous pouvez prendre des animations gratuites sur Mixamo :

1. Allez sur <https://www.mixamo.com> et connectez-vous (compte Adobe gratuit).
2. **Upload Character** : choisissez le fichier
   `Bureau\Monde\(fbx)T pose - rigs\police_officer.fbx` (celui qui a servi à importer le policier).
3. Onglet **Animations**, cherchez et téléchargez 3 animations :
   - une attente : tapez `idle` (par exemple « Breathing Idle ») ;
   - un geste pour faire passer : tapez `waving` ou `pointing` ou `directing` ;
   - un geste stop : tapez `stop` ou `halt` (sinon une main levée).
   Au téléchargement : **Format FBX Binary**, **Skin : Without Skin**, **30 fps**,
   et cochez **In Place** si la case existe.
4. Dans Unreal, créez le dossier `Content/Personnages/Mondes/Policier_Animations`.
5. Glissez les 3 fichiers FBX dedans. Dans la fenêtre d'import :
   **Skeleton = police_officer_Skeleton**, décochez **Import Mesh**, cliquez **Import All**.
6. Renommez-les **Policier_Repos**, **Policier_Passez**, **Policier_Stop**, puis **File > Save All**.
7. Play : le message vert dit maintenant « animations ».

Si Unreal refuse l'import (« os introuvables » / bones mismatch), envoyez une capture à Claude :
les noms des os de Mixamo sont alors différents de ceux du policier et il faudra les recibler.

## Tout retirer

Fermez Unreal et supprimez le dossier `Plugins\ChellesPolicier` (ou mettez `Actif=0`).
