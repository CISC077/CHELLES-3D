# Nettoyage du journal du test (25/11/2026, agent 1)

Test de 12h35 : 28/28 étapes. Restes du journal et ce qui a été fait :

| Message | Cause | Correctif |
|---|---|---|
| `GetSimplePhysicalMaterial : GEngine not initialized` (Error) | `AKitSurvie` (ChellesPvP) appelait `SetMassOverrideInKg` / `SetLinearDamping` dans son constructeur | réglages posés directement dans `BodyInstance` (`KitSurvie.cpp`) |
| 264 × `GetSocketByName(GripPoint): No SkeletalMesh` sur `AI_Super_Heros_C_0` | AI_Super_Heros n'a plus de corps (`/Game/UndeadPack/SkeletonEnemy/Mesh/SK_Skeleton` absent) et ChellesEnnemis le posait quand même | ChellesEnnemis ignore tout ennemi sans corps ; l'exclusion par nom accepte aussi le suffixe `_C` (`EnnemisChelles.cpp`) |
| `Failed to validate media source FMS_01_ToggleFlightMode` (Error) | vidéos de démonstration du pack SuperheroFlight jamais livrées (`Content/SuperheroFlight/Movies` n'existe pas) | sans effet en jeu, laissé tel quel |
| 84 × `Accessed None CallFunc_AddComponent_ReturnValue` | Blueprints du pack Venice (`BP_brick_fence`, `BP_building`, fonction `windows`) | défaut interne du pack, au chargement seulement ; non modifié (règle : ne rien casser de ce qui marche) |

À lancer : `Tools\Testeur\NETTOYER_JOURNAL.bat` (recompile ChellesPvP + ChellesEnnemis puis relance le test).
