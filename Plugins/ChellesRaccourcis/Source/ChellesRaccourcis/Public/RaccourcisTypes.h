// CHELLES 3D - Ligne du tableau de donnees des touches F1-F12 (Data Table DT_Raccourcis)
//
// Une ligne = une touche. Ajouter / changer une touche = modifier une ligne du tableau
// (dans l'editeur : double-clic sur /Game/Chelles/Donnees/DT_Raccourcis), sans toucher au code.
// Le tableau est cree depuis Tools\RaccourcisF\Raccourcis.csv par CREER_LA_TABLE.bat.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
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

	/** Petite image (pas encore affichee dans le menu du haut). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raccourci")
	TSoftObjectPtr<UTexture2D> Icone;
};

// CHELLES 3D - Ligne du tableau de donnees des ARMES (Data Table DT_Armes, roue des armes sur TAB)
//
// Une ligne = une case de la roue. Ajouter une arme = ajouter une ligne (bouton Add du tableau), sans code.
// Le tableau est cree depuis Tools\RaccourcisF\Armes.csv par CREER_LA_TABLE.bat.
USTRUCT(BlueprintType)
struct CHELLESRACCOURCIS_API FChellesArmeLigne : public FTableRowBase
{
	GENERATED_BODY()

	/** Nom ecrit dans la roue (vide = le nom donne par l'arme elle-meme, WeaponName). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme")
	FText Nom;

	/**
	 * Arme du soldat FPS_Game : son Blueprint (enfant de BP_WeaponBase), par exemple BPC_WeaponPistol.
	 * Laisser vide pour une arme de l'arsenal (colonne Arsenal).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme")
	TSoftClassPtr<AActor> ClasseArme;

	/** Arme de l'arsenal ChellesArmes : Blaster, Bazooka, Sabre, Grenade, LanceFlammes, Mitrailleuse (vide = arme FPS_Game). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme")
	FString Arsenal;

	/** Image de la case (vide = l'image de l'arme, WeaponImage, ou un dessin simple). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme")
	TSoftObjectPtr<UTexture2D> Icone;

	/** Couleur de la case (transparente = gris-bleu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme")
	FLinearColor Couleur = FLinearColor(0.f, 0.f, 0.f, 0.f);

	/** Famille (Pistolet, Fusil, Pompe, Precision, Lourde, Melee, Energie, Lancer) : choisit le petit dessin si pas d'image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme")
	FString Categorie;

	/** Vrai = choisir la case DONNE l'arme si le joueur ne l'a pas encore ramassee (solo / hote seulement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arme")
	bool bDonner = false;
};
