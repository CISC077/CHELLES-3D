// CHELLES 3D - Defi de la semaine : bandeau neon en haut au centre de l'ecran (cyan / magenta / or, comme le menu et le HUD)
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

#include "DefisChelles.h"

class SDefisBandeau : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDefisBandeau) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Montre le defi pendant Duree secondes. bReussite = bandeau "DEFI REUSSI". */
	void Montrer(const FDefiSemaine& Defi, bool bReussite, float Duree);
	/** Vrai tant que le bandeau est (encore) visible. */
	bool EstVisible() const;

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800.f, 200.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	void Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& Couleur) const;
	/** Ecrit un texte avec une ombre, centre sur Pos.X. Rend la taille. */
	FVector2D Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FString& Texte, const FVector2D& Pos,
		int32 Taille, const FLinearColor& Couleur, bool bGras) const;
	/** Coupe un texte en lignes qui tiennent dans LargeurMax. */
	TArray<FString> Couper(const FString& Texte, int32 Taille, float LargeurMax) const;

	FDefiSemaine Defi;
	bool bReussite = false;
	double Debut = -1000.0;
	float Duree = 0.f;
};
