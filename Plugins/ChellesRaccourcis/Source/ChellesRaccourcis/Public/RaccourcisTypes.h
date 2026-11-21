// CHELLES 3D - Ligne du tableau de donnees des touches F1-F12 (Data Table DT_Raccourcis)
//
// Une ligne = une touche. Ajouter / changer une touche = modifier une ligne du tableau
// (dans l'editeur : double-clic sur /Game/Chelles/Donnees/DT_Raccourcis), sans toucher au code.
// Le tableau est cree depuis Tools\RaccourcisF\Raccourcis.csv par CREER_LA_TABLE.bat.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "RaccourcisTypes.generated.h"

USTRUCT(BlueprintType)
struct CHELLESRACCOURCIS_API FChellesRaccourciLigne : public FTableRowBase
{
	GENERATED_BODY()

	/** Touche du clavier (F1 ... F12, ou une autre touche libre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raccourci")
	FKey Touche;

	/**
	 * Ce que fait la touche :
	 *  - le nom d'un mode de la roue : Soldat, Spider-Male, Spider-Femelle, PANTERA, Motoneige, POLICE,
	 *    FLASH ECLAIR, FLAMME FIRE, SuperPowers, DYNAMIC COMBAT, HORSE ;
	 *  - Case1 .. Case4 : une case d'arme du soldat ;
	 *  - Blaster, Bazooka, Sabre, Grenade, LanceFlammes, Mitrailleuse : arme de l'arsenal ;
	 *  - Reserve : rien (gardee pour plus tard).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raccourci")
	FString Action;

	/** Texte montre dans le menu du haut (vide = le nom du mode). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raccourci")
	FText NomAffiche;

	/** Couleur de la case dans le menu (transparente = couleur du mode dans la roue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raccourci")
	FLinearColor Couleur = FLinearColor(0.f, 0.f, 0.f, 0.f);

	/** Petite image (pas encore affichee : servira a la roue des armes, etape 3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raccourci")
	TSoftObjectPtr<UTexture2D> Icone;
};
