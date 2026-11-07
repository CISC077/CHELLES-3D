// CHELLES 3D - Dessin de la roue des modes (Slate, sans asset)
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "RoueModes.h"

struct FCaseRoue
{
	FText Nom;
	EIconeRoue Icone = EIconeRoue::Etoile;
	FLinearColor Couleur = FLinearColor::White;
	bool bDisponible = true;
};

class SRoueModes : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRoueModes) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(600.f, 600.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	/** Rayon exterieur de la roue (unites Slate), pour le curseur. */
	static float RayonPour(const FVector2D& Taille);

	TArray<FCaseRoue> Cases;
	int32 Survol = -1;
	int32 Actuel = -1;
	FVector2D Curseur = FVector2D::ZeroVector; // relatif au centre, en fraction du rayon exterieur
	float Apparition = 1.f;                    // 0 -> 1 a l'ouverture

private:
	void Secteur(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float RayonIn, float RayonOut, float AngleDebut, float AngleFin, const FLinearColor& Couleur) const;
	void Disque(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float Rayon, const FLinearColor& Couleur) const;
	void Icone(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float Taille, EIconeRoue Type, const FLinearColor& Couleur) const;
	void Texte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		const FText& Contenu, const FSlateFontInfo& Police, const FLinearColor& Couleur) const;
};
