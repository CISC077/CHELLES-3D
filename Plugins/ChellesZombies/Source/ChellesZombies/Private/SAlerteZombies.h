// CHELLES 3D - Invasion zombies : affichage a l'ecran (Slate, sans asset)
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

/** Ce que l'ecran doit montrer, rempli a chaque image par AInvasionZombies. */
struct FEtatAlerteZombies
{
	/** Secondes depuis le debut de l'alerte (negatif = pas d'alerte). */
	float AgeAlerte = -1.f;
	float DureeAlerte = 30.f;
	FString Texte = TEXT("ATTENTION PURGE ZOMBIE INVASION");

	/** Compteur en haut de l'ecran pendant l'invasion. */
	bool bInvasion = false;
	int32 Restants = 0;
	float SecondesRestantes = 0.f;

	/** Message de fin (PURGE TERMINEE...) et son age. */
	FString MessageFin;
	float AgeFin = 100.f;
};

class SAlerteZombies : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAlerteZombies) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800.f, 600.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	FEtatAlerteZombies Etat;

private:
	void Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FVector2D& Taille, const FLinearColor& Couleur) const;
	void PeindreAlerte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G) const;
	void PeindreCompteur(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G) const;
	void PeindreFin(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G) const;
};
