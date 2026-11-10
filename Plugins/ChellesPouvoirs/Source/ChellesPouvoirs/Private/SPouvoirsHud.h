// CHELLES 3D - XP et pouvoirs : affichage a l'ecran (Slate, sans asset)
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "PouvoirsTypes.h"

/** Ce que le HUD doit afficher, rempli a chaque image par APouvoirsChelles. */
struct FEtatPouvoirsHud
{
	int32 XP = 0;
	int32 Niveau = 1;
	int32 Selection = 0;
	float Recharge[ChellesPouvoirs::NombrePouvoirs] = {};
	float Effet[ChellesPouvoirs::NombrePouvoirs] = {};
	/** Secondes restantes de l'effet verre sur moi (0 = rien). */
	float MoiVerre = 0.f;
	/** Secondes restantes pendant lesquelles je suis gele (0 = rien). */
	float MoiGele = 0.f;
	/** Grand titre (nouveau niveau) et son age en secondes. */
	FString Banniere;
	FString SousBanniere;
	float AgeBanniere = 100.f;
	/** Petit message (pouvoir pas pret, etc.) et son age. */
	FString Info;
	float AgeInfo = 100.f;
	bool bVisible = true;
};

class SPouvoirsHud : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SPouvoirsHud) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800.f, 600.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	FEtatPouvoirsHud Etat;

private:
	void Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FVector2D& Taille, const FLinearColor& Couleur) const;
	void Cadre(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FVector2D& Taille, const FLinearColor& Couleur, float Epaisseur) const;
	/** Ecrit un texte ; Alignement 0 = a gauche de Pos, 0.5 = centre sur Pos, 1 = a droite. */
	void Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FString& Texte, const FSlateFontInfo& Police, const FLinearColor& Couleur, float Alignement) const;
	void Icone(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float Taille, int32 Quel, const FLinearColor& Couleur) const;
};
