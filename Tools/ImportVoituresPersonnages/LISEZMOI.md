# Voitures et personnages (import)

Ce dossier contient vos modèles `.glb` de `Content/Voitures` et `Content/Personnages`,
convertis en FBX prêts pour Unreal 4.27 (bonne taille en cm, avant des voitures vers +X,
pivot au sol, une seule pièce par voiture, statues allégées à 100 000 triangles).
Vos fichiers `.glb` d'origine n'ont pas été modifiés.

## Importer (une seule fois)

1. Ouvrez le projet dans Unreal.
2. *File > Execute Python Script...* et choisissez
   `Tools\ImportVoituresPersonnages\importer_voitures_personnages.py`.
3. Attendez la fin (quelques minutes). Résultat dans *Content/Voitures/...* et
   *Content/Personnages/...*. Journal : `Tools\out\import_voitures_personnages.log`.
4. Relancez *File > Execute Python Script...* avec `Tools\installer_vie_urbaine.py` :
   la circulation reprend la liste des voitures (vos 10 voitures en font partie) et les
   passants reprennent la liste des personnages.

## Ce qui est importé

| Dossier Unreal | Type | Remarque |
|---|---|---|
| Voitures/SM_Voiture_* (10) | Static Mesh | roulent avec le plugin Chelles Circulation |
| Personnages/SK_Hulk | Skeletal Mesh + Walk + Idle | se promène avec les passants |
| Personnages/SK_BruceBanner | Skeletal Mesh | pas d'animation de marche |
| Personnages/SK_SpiderMan | Skeletal Mesh + Agreeing | pas d'animation de marche |
| Personnages/SM_Statue_* (8) | Static Mesh | statues à poser dans les rues |

Si une voiture roule à reculons : *Details* du Circulation Spawner > *Modeles* >
**Correction Orientation** = 180.

## Tout retirer

Supprimez les dossiers `Content/Voitures/SM_Voiture_*` et `Content/Personnages/SK_*`,
`SM_Statue_*` dans Unreal (clic droit > Delete), puis ce dossier.
