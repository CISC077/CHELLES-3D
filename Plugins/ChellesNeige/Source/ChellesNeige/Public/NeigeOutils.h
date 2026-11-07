// CHELLES 3D - outils editeur pour Tools/Neige/ajouter_neige.py (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NeigeOutils.generated.h"

class UMaterial;
class UMaterialExpression;
class UMaterialParameterCollection;

/**
 * Ce que Python ne sait pas faire dans Unreal 4.27 :
 * brancher la sortie "World Position Offset" d'un materiau et regler une Material Parameter Collection.
 */
UCLASS()
class CHELLESNEIGE_API UNeigeOutils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Branche la 1re sortie de Expression sur "World Position Offset" du materiau. */
	UFUNCTION(BlueprintCallable, Category = "Chelles Neige")
	static bool BrancherPositionMonde(UMaterial* Materiau, UMaterialExpression* Expression);

	/** Ajoute (ou met a jour) un parametre scalaire dans une Material Parameter Collection. */
	UFUNCTION(BlueprintCallable, Category = "Chelles Neige")
	static bool AjouterScalaireCollection(UMaterialParameterCollection* Collection, FName Nom, float ValeurParDefaut);

	/** Relie un noeud "Collection Parameter" a la collection et au parametre donnes. */
	UFUNCTION(BlueprintCallable, Category = "Chelles Neige")
	static bool ReglerNoeudCollection(UMaterialExpression* Noeud, UMaterialParameterCollection* Collection, FName Nom);
};
