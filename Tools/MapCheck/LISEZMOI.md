# Corriger les avertissements du Map Check (carte Rachid)

Le Map Check affichait 143 avertissements (0 erreur). Aucun n'empêche le jeu de marcher,
mais certains coûtent des FPS ou laissent des objets invisibles.

## Lancer la correction

1. Ouvre Unreal avec la carte **Rachid**, fais **Ctrl+S**.
2. **File > Execute Python Script…** → `Tools\MapCheck\corriger_mapcheck.py`.
3. Lis la liste des corrections → **Oui**.
4. 2e question (ombre des bâtiments) → **Non** conseillé (voir plus bas).
5. **Build > Build Paths** (attendre la fin), puis **Ctrl+S**.
6. Vérifier : **Window > Developer Tools > Map Check**.

## Ce que fait le script

| Avertissement | Combien | Correction |
| --- | --- | --- |
| `Chelles_Tile_Road_*`, `Chelles_Trottoir_*` : « pre-shadow » | 32 | Ombre portée coupée. Une route plate n'a pas d'ombre visible ; elle **reçoit** toujours les ombres. |
| `CH_lamp_*`, `SM_Bench_*` : « NULL StaticMesh » | 31 | Lampadaires / bancs invisibles (leur modèle a disparu du projet, sans doute `SM_Street_lamp_4` de Quartal). On leur remet le modèle du voisin le plus proche du même nom, à la même taille, posé au sol. |
| Sprites (BAT01, BAT19, EPICERIE, HNK…) : « scaled non-uniformly » | 53 | Ces images étirées ne pouvaient pas avoir de collision (le moteur échouait). On coupe la collision proprement : l'image ne change pas. |
| `BP_Hotel_*` : « More overridden materials » | 3 | Le modèle de tente n'a qu'un matériau, le 2e posé ne servait à rien : enlevé. |

## Ce qu'il laisse (et pourquoi)

- **`Chelles_Tile_X_Y` (blocs de bâtiments, 16) et `Tour2025` (2)** : couper l'avertissement = couper
  leur ombre. Avec le soleil qui bouge (cycle jour/nuit GoodSky, lumière « Movable »), ces ombres
  passent par les ombres normales du soleil : l'avertissement ne coûte presque rien ici,
  et sans ombre la ville paraît plate. Le script propose quand même de les couper (2e question).
- **`BP_building_5`, `BP_building2`** (6 avertissements) : ce sont des Blueprints du pack
  Venice_fast_building qui ont des emplacements de modèle vides à l'intérieur. Ça ne coûte rien.
  Pour les faire disparaître il faudrait modifier le Blueprint du pack : on n'y touche pas.

Il devrait donc rester environ **24 avertissements** (18 ombres de bâtiments + 6 BP_building), tous sans danger.

## Ce qui n'est PAS touché

Lumières, GoodSky / cycle jour-nuit, réglages de `Tools\Eclairage`, neige, `NavMesh_Chelles`,
les Blueprints et les assets du Content.

## NavMesh

Les lampadaires et bancs remis ont une collision, et les sprites n'en ont plus :
**refaire Build > Build Paths** après le script (et après l'annulation).

## Annuler

**File > Execute Python Script…** → `Tools\MapCheck\annuler_mapcheck.py` → **Oui** → Build Paths → **Ctrl+S**.
Ctrl+Z marche aussi juste après. En dernier recours : la copie du `.umap` dans
`Tools\MapCheck\sauvegarde\` (à recopier dans `Content\Maps\` Unreal fermé).

Journal : `Tools\MapCheck\journal_mapcheck.json` et `mapcheck.log`.
