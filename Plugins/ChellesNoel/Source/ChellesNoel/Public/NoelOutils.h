// CHELLES 3D - outils editeur pour Tools/Noel/installer_noel.py (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NoelOutils.generated.h"

class UMaterial;
class UMaterialParameterCollection;

/**
 * Ce que Python ne sait pas faire dans Unreal 4.27 : lire et rebrancher les sorties d'un materiau.
 */
UCLASS()
class CHELLESNOEL_API UNoelOutils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Ajoute (ou met a jour) un parametre scalaire dans une Material Parameter Collection. */
	UFUNCTION(BlueprintCallable, Category = "Chelles Noel")
	static bool AjouterScalaireCollection(UMaterialParameterCollection* Collection, FName Nom, float ValeurParDefaut);

	/**
	 * Copie tout le graphe de SourceNeige (le materiau de neige M_Sol_Neige) dans Cible (une copie du
	 * materiau d'origine du sol), puis melange les deux : couleur, rugosite, normale.
	 * Le melange suit le parametre Parametre de la collection : 0 = sol d'origine, 1 = neige
	 * (seulement sur les zones a peu pres plates).
	 * Renvoie "" si tout va bien, sinon le probleme en francais.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chelles Noel")
	static FString MelangerNeige(UMaterial* Cible, UMaterial* SourceNeige, UMaterialParameterCollection* Collection, FName Parametre);
};
